/*  Copyright (C) 2026 P. David Buchan (pdbuchan@gmail.com)
  
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
  
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dvb.h"

// Release the current PAT and every PMT/elementary-stream list owned by it.
// PID classifications are cleared while the old PID values are still
// available, before the structures containing those values are freed.
static void
release_pat (STATE *state, PAT *pat) {

  size_t i, j;

  for (i = 0; i < pat->nprograms; i++) {
    if (pat->program[i].pmt_pid < MAX_PIDS) {
      state->pid_type[pat->program[i].pmt_pid] = PID_UNKNOWN;
    }
    for (j = 0; j < pat->program[i].pmt.nstreams; j++) {
      uint16_t epid = pat->program[i].pmt.stream[j].elementary_stream_pid;
      if (epid < MAX_PIDS) {
        state->pid_type[epid] = PID_UNKNOWN;
      }
    }
    free (pat->program[i].pmt.stream);
  }
  free (pat->program);
  pat->program = NULL;
  pat->nprograms = 0;
}

// Parse a Program Association Table (PAT).
// One PAT maps program numbers to the PIDs carrying their Program Map Tables.
// Reference: ISO/IEC 13818-1.
int
parse_pat (STATE *state, PAT *pat, SECTION *section, FILE *fo) {

  size_t offset, section_length, total, entries_end, entries_bytes;
  size_t nprograms, i;
  uint8_t table_id, ssi, version, current, section_number, last_section;
  uint16_t pid, tsid, program_number, pmt_pid;
  PROGRAM *new_programs;

  pid = state->pid;
  offset = 0;
  nprograms = 0;
  new_programs = NULL;

  fprintf (fo, "Program Association Table (PAT):\n");

  // Table ID (1 byte), Section Syntax Indicator (1 bit), and Section Length
  // (12 bits). section_length counts from transport_stream_id through CRC.
  if (!bytes_available (offset, 3, section[pid].length)) {
    return (EXIT_FAILURE);
  }
  table_id = section[pid].buffer[offset++];
  ssi = (section[pid].buffer[offset] >> 7) & 1;
  section_length = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) |
                             section[pid].buffer[offset + 1]);
  offset += 2;

  if (table_id != 0x00 || ssi != 1 || section_length < 9 ||
      section_length > 1021 ||
      !bytes_available (0, section_length + 3, section[pid].length)) {
    return (EXIT_FAILURE);
  }
  total = section_length + 3;
  entries_end = total - 4;  // CRC begins here.

  // Transport Stream ID (2 bytes), Version Number (5 bits), Current/Next
  // Indicator (1 bit), Section Number, and Last Section Number.
  if (!bytes_available (offset, 5, entries_end)) {
    return (EXIT_FAILURE);
  }
  tsid = (uint16_t) (((uint16_t) section[pid].buffer[offset] << 8) |
                     section[pid].buffer[offset + 1]);
  offset += 2;
  version = (section[pid].buffer[offset] >> 1) & 0x1f;
  current = section[pid].buffer[offset++] & 1;
  section_number = section[pid].buffer[offset++];
  last_section = section[pid].buffer[offset++];

  fprintf (fo,
           "  Table ID: 0x%02x Section Length: %zu Transport Stream ID: "
           "0x%04x Version: %u Current: %u Section: %u/%u\n",
           table_id, section_length, tsid, version, current,
           section_number, last_section);

  // This program currently expects the PAT to fit in one PSI section.
  if (last_section != 0 || section_number != 0) {
    fprintf (stderr, "Multi-section PATs are not supported.\n");
    return (EXIT_FAILURE);
  }

  entries_bytes = entries_end - offset;
  if ((entries_bytes % 4) != 0) {
    return (EXIT_FAILURE);
  }

  // Program loop, first pass. Each four-byte entry contains Program Number
  // (16 bits), Reserved (3 bits), and Program Map PID (13 bits). Program
  // number zero identifies the NIT PID and is not a PMT program entry.
  for (i = 0; i < entries_bytes / 4; i++) {
    program_number = (uint16_t) (((uint16_t) section[pid].buffer[offset] << 8) | section[pid].buffer[offset + 1]);
    pmt_pid = (uint16_t) (((section[pid].buffer[offset + 2] & 0x1f) << 8) | section[pid].buffer[offset + 3]);
    offset += 4;

    if (program_number == 0) {
      fprintf (fo, "  NIT PID: 0x%04x\n", pmt_pid);
    } else {
      fprintf (fo, "  Program %u -> PMT PID 0x%04x\n", program_number, pmt_pid);
      nprograms++;
    }
  }

  // CRC (4 bytes). parse_psi_section() has already verified it, but include
  // the transmitted value in the report.
  fprintf (fo, "  CRC: %02x%02x%02x%02x\n", section[pid].buffer[entries_end], section[pid].buffer[entries_end + 1], section[pid].buffer[entries_end + 2], section[pid].buffer[entries_end + 3]);

  // A current_next_indicator of zero describes a future PAT and must not
  // replace the active PID map.
  if (!current) {
    return (EXIT_SUCCESS);
  }

  // PATs are broadcast frequently. If the version has not changed, retain
  // the existing program and PMT state after reporting the repeated table.
  if (state->have_pat && pat->version == version) {
    return (EXIT_SUCCESS);
  }

  // Allocate the replacement program array at exactly the required size.
  if (nprograms > 0) {
    new_programs = allocate_progmem (nprograms);
  }

  // Second pass: populate the newly allocated PROGRAM array. Starting from a
  // separate allocation lets a new PAT safely contain more programs than the
  // previous version.
  offset = 8;
  nprograms = 0;
  while (offset < entries_end) {
    program_number = (uint16_t) (((uint16_t) section[pid].buffer[offset] << 8) | section[pid].buffer[offset + 1]);
    pmt_pid = (uint16_t) (((section[pid].buffer[offset + 2] & 0x1f) << 8) | section[pid].buffer[offset + 3]);
    offset += 4;

    if (program_number == 0) {
      continue;
    }
    new_programs[nprograms].program_number = program_number;
    new_programs[nprograms].pmt_pid = pmt_pid;
    nprograms++;
  }

  // Release old PMTs only after the replacement PAT has been fully parsed.
  // This avoids writing a larger new PAT into storage sized for an older PAT.
  release_pat (state, pat);
  pat->program = new_programs;
  pat->nprograms = nprograms;
  pat->version = version;
  state->have_pat = 1;

  // Every PMT PID listed by the PAT carries PSI rather than PES data.
  for (i = 0; i < nprograms; i++) {
    state->pid_type[new_programs[i].pmt_pid] = PID_PSI;
  }

  return (EXIT_SUCCESS);
}
