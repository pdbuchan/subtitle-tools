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

// Parse a Program Association Table (PAT).
// The PAT lists all Programs and their PMT PIDs.
// PID = 0x0000; There can only be one, but it can be updated if version changes.
// Reference: ISO/IEC 13818-1
int
parse_pat (STATE *state, PAT *pat, SECTION *section, FILE *fo) {

  size_t i, j, offset, section_length, program_info_bytes, end, nprograms;
  uint8_t table_id, section_syntax_indicator, version_number, current_next_indicator, section_number, last_section_number;
  uint16_t pid, transport_stream_id, program_number[MAX_PROGRAMS], program_map_pid[MAX_PROGRAMS];

  pid = state->pid;

  // Set some arrays to 0.
  memset (program_number, 0, MAX_PROGRAMS * sizeof (uint16_t));
  memset (program_map_pid, 0, MAX_PROGRAMS * sizeof (uint16_t));

  offset = 0;  // Start at beginning of section.

  fprintf (fo, "Program Association Table (PAT):\n");

  // Table ID (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
    exit (EXIT_FAILURE);
  }
  table_id = section[pid].buffer[offset];
  fprintf (fo, "  Table ID (1 byte): 0x%02x\n", table_id);
  offset++;

  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
    exit (EXIT_FAILURE);
  }

  // Section Syntax Indicator (1 bit)
  section_syntax_indicator = (section[pid].buffer[offset] >> 7) & 1;
  fprintf (fo, "  Section Syntax Indicator (1 bit): %u\n", section_syntax_indicator);

  // 0 (1 bit)

  // Reserved (2 bits)

  // Section Length (12 bits)
  section_length = (size_t) ((section[pid].buffer[offset] & 0x0f) << 8 |
                 section[pid].buffer[offset + 1]);
  fprintf (fo, "  Section Length (12 bits): %zu bytes (%zu bytes including table ID, SSI, section len)\n", section_length, section_length + 3);
  offset += 2;

  // Transport Stream ID (2 bytes)
  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
    exit (EXIT_FAILURE);
  }
  transport_stream_id = (section[pid].buffer[offset] << 8) |
                         section[pid].buffer[offset + 1];
  fprintf (fo, "  Transport Stream ID (2 bytes): 0x%04x\n", transport_stream_id);
  offset += 2;

  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
    exit (EXIT_FAILURE);
  }

  // Reserved (2 bits)

  // Version Number (5 bits)
  version_number = (section[pid].buffer[offset] >> 1) & 0x1f;  // 0x1f = 11111
  fprintf (fo, "  Version Number (5 bits): 0x%02x\n", version_number);

  // Normally you don't bother processing anything more if version hasn't changed.
  // We will continue anyway for the sake of the output file.

  // Current Next Indicator (1 bit)
  current_next_indicator = section[pid].buffer[offset] & 1;
  fprintf (fo, "  Current Next Indicator (1 bit): %u\n", current_next_indicator);
  offset++;

  // Section Number (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
    exit (EXIT_FAILURE);
  }
  section_number = section[pid].buffer[offset];
  fprintf (fo, "  Section Number (1 byte): 0x%02x\n", section_number);
  offset++;

  // Last Section Number (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
    exit (EXIT_FAILURE);
  }
  last_section_number = section[pid].buffer[offset];
  fprintf (fo, "  Last Section Number (1 byte): 0x%02x\n", last_section_number);
  offset++;

  // Program loop
  program_info_bytes = section_length - 5 - 4;  // We start after section_length: header (5 bytes), CRC (4 bytes)
  end = offset + program_info_bytes;
  nprograms = 0;

  while (((offset) + 4) <= end) {  // 4 bytes for CRC

    // Program number (16 bits)
    if ((offset + 1) >= MAX_BUFFERLEN) {
      fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
      exit (EXIT_FAILURE);
    }
    program_number[nprograms] = (section[pid].buffer[offset] << 8) |
                      section[pid].buffer[offset + 1];
    offset += 2;

    if ((offset + 1) >= MAX_BUFFERLEN) {
      fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
      exit (EXIT_FAILURE);
    }

    // Reserved (3 bits)

    // Program Map PID (13 bits)
    program_map_pid[nprograms] = ((section[pid].buffer[offset] & 0x1f) << 8) |  // 0x1f = 11111
                        section[pid].buffer[offset + 1];
    offset += 2;

    if (program_number[nprograms] == 0) {
      fprintf (fo, "  NIT PID (13 bits): 0x%04x\n", program_map_pid[nprograms]);
    } else {
      fprintf (fo, "  Program (16 bits) %u -> PMT PID (13 bits): 0x%04x\n", program_number[nprograms], program_map_pid[nprograms]);

      // Increment count of programs.
      nprograms++;
    }
  }
  if (nprograms > 1) {
    fprintf (stderr, "Warning: More than one programs listed in PAT.\n");
    fprintf (stderr, "         Expected only one, corresponding to the PMT of single DVB stream extracted from media .ts file using ffmpeg.\n");
  }

  // CRC (4 bytes)
  if ((offset + 3) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pat().\n");
    exit (EXIT_FAILURE);
  }
  fprintf (fo, "  CRC (4 bytes): ");
  for (i = 0; i < 4; i++) {
    fprintf (fo, "%02x", section[pid].buffer[offset + i]);
  }
  fprintf (fo, "\n");

  // This is the first time we are parsing the PAT.
  if (!state->have_pat) {

    // Allocate memory for pat->program array.
    pat->program = allocate_progmem (nprograms);
    for (i = 0; i < nprograms; i++) {
      pat->program[i].pmt.stream = NULL;  // Will be allocated dynamically by parse_pmt().
    }

  // The PAT version has changed.
  // The PAT is broadcast roughly every 1/10th of a second, so most will have same version number and can be ignored, however,
  // we report to output file all data above regardless of whether new PAT/new version or repeat of existing PAT.
  } else if (pat->version != version_number) {

    // Clear existing PAT.
    for (i = 0; i < pat->nprograms; i++) {
      for (j = 0; j < pat->program[i].pmt.nstreams; j++) {
        memset (&pat->program[i].pmt.stream[j], 0, sizeof (PMT_STREAM));
      }
    }  // End for pat->nprograms
  }  // End if have_pat or new version

  // Store program_number/program_map_pid values.
  pat->nprograms = nprograms;
  for (i = 0; i < pat->nprograms; i++) {
    pat->program[i].program_number = program_number[i];
    pat->program[i].pmt_pid = program_map_pid[i];
    pat->program[i].have_pmt = 0;  // Awaiting PMT.
    state->pid_type[pat->program[i].pmt_pid] = PID_PSI;
  }

  // Update state to show we have parsed the PAT.
  state->have_pat = 1;

  return (EXIT_SUCCESS);
}
