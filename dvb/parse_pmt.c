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

// Parse a Program Map Table (PMT).
// One PMT lists all elementary-stream PIDs and Stream Types associated with a
// program identified by the PAT.
// Reference: ISO/IEC 13818-1.
int
parse_pmt (STATE *state, PAT *pat, SECTION *section, FILE *fo) {

  int program_index;
  size_t offset, section_length, total, entries_end, program_info_length;
  size_t es_info_length, nstreams, capacity, i, pidx;
  uint8_t table_id, ssi, version, current, section_number, last_section;
  uint8_t stream_type;
  uint16_t pid, program_number, pcr_pid, elementary_pid;
  PMT_STREAM *list, *tmp;

  pid = state->pid;
  offset = 0;
  nstreams = 0;
  capacity = 0;
  list = NULL;

  fprintf (fo, "Program Map Table (PMT):\n");

  // Table ID (1 byte), Section Syntax Indicator (1 bit), and Section Length
  // (12 bits).
  if (!bytes_available (offset, 3, section[pid].length)) {
    return (EXIT_FAILURE);
  }
  table_id = section[pid].buffer[offset++];
  ssi = (section[pid].buffer[offset] >> 7) & 1;
  section_length = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) | section[pid].buffer[offset + 1]);
  offset += 2;

  if (table_id != 0x02 || ssi != 1 || section_length < 13 || section_length > 1021 || !bytes_available (0, section_length + 3, section[pid].length)) {
    return (EXIT_FAILURE);
  }
  total = section_length + 3;
  entries_end = total - 4;  // CRC begins here.

  // Program Number (2 bytes), Version Number (5 bits), Current/Next Indicator
  // (1 bit), Section Number, Last Section Number, PCR PID (13 bits), and
  // Program Info Length (12 bits).
  if (!bytes_available (offset, 9, entries_end)) {
    return (EXIT_FAILURE);
  }
  program_number = (uint16_t) (((uint16_t) section[pid].buffer[offset] << 8) | section[pid].buffer[offset + 1]);
  offset += 2;
  version = (section[pid].buffer[offset] >> 1) & 0x1f;
  current = section[pid].buffer[offset++] & 1;
  section_number = section[pid].buffer[offset++];
  last_section = section[pid].buffer[offset++];
  pcr_pid = (uint16_t) (((section[pid].buffer[offset] & 0x1f) << 8) | section[pid].buffer[offset + 1]);
  offset += 2;
  program_info_length = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) | section[pid].buffer[offset + 1]);
  offset += 2;

  // This program currently expects the PMT to fit in one PSI section.
  if (last_section != 0 || section_number != 0) {
    fprintf (stderr, "Multi-section PMTs are not supported.\n");
    return (EXIT_FAILURE);
  }
  if (!bytes_available (offset, program_info_length, entries_end)) {
    return (EXIT_FAILURE);
  }

  // Program descriptor loop. Descriptors are not currently interpreted here,
  // but their declared length must be skipped before the ES loop begins.
  offset += program_info_length;

  fprintf (fo, "  Program: 0x%04x Version: %u Current: %u Section: %u/%u PCR PID: 0x%04x\n", program_number, version, current, section_number, last_section, pcr_pid);

  // Elementary Stream loop. Each entry supplies Stream Type, Elementary PID,
  // and a descriptor-loop length.
  while (offset < entries_end) {
    if (!bytes_available (offset, 5, entries_end)) {
      free (list);
      return (EXIT_FAILURE);
    }

    stream_type = section[pid].buffer[offset++];
    elementary_pid = (uint16_t) (((section[pid].buffer[offset] & 0x1f) << 8) | section[pid].buffer[offset + 1]);
    offset += 2;
    es_info_length = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) | section[pid].buffer[offset + 1]);
    offset += 2;

    if (!bytes_available (offset, es_info_length, entries_end)) {
      free (list);
      return (EXIT_FAILURE);
    }

    // ES Descriptor Loop. As above, descriptors are reported indirectly via
    // stream_type and skipped according to their declared aggregate length.
    offset += es_info_length;

    fprintf (fo, "  Elementary Stream %zu PID 0x%04x type 0x%02x\n", nstreams, elementary_pid, stream_type);
    stream_types (state, stream_type, fo);

    // Grow the temporary stream array geometrically while respecting the
    // program-wide MAX_STREAMS limit.
    if (nstreams == capacity) {
      size_t new_capacity = capacity ? capacity * 2 : 8;
      if (new_capacity > MAX_STREAMS) {
        new_capacity = MAX_STREAMS;
      }
      if (new_capacity <= capacity) {
        free (list);
        return (EXIT_FAILURE);
      }
      tmp = realloc (list, new_capacity * sizeof (*list));
      if (!tmp) {
        free (list);
        return (EXIT_FAILURE);
      }
      list = tmp;
      capacity = new_capacity;
    }
    list[nstreams].elementary_stream_pid = elementary_pid;
    list[nstreams].stream_type = stream_type;
    nstreams++;
  }

  // CRC (4 bytes). It was validated before parse_pmt() was called.
  fprintf (fo, "  CRC: %02x%02x%02x%02x\n", section[pid].buffer[entries_end], section[pid].buffer[entries_end + 1], section[pid].buffer[entries_end + 2], section[pid].buffer[entries_end + 3]);

  // Associate this PMT PID with the PROGRAM entry previously obtained from
  // the PAT. An unrecognized PMT is still valid PSI, but it is not part of the
  // PAT currently being followed by this program.
  program_index = find_program_by_pmt_pid (pat, pid);
  if (program_index < 0) {
    free (list);
    return (EXIT_SUCCESS);
  }
  pidx = (size_t) program_index;

  // The program_number carried by the PMT must agree with the PAT mapping.
  if (program_number != pat->program[pidx].program_number) {
    free (list);
    return (EXIT_FAILURE);
  }

  // Future PMTs and repeats of the active version are reported but do not
  // replace the stored elementary-stream map.
  if (!current || (pat->program[pidx].have_pmt && pat->program[pidx].pmt.version == version)) {
    free (list);
    return (EXIT_SUCCESS);
  }

  // Clear classifications while the old elementary PIDs are still known.
  // The original code zeroed the structures first and consequently cleared
  // PID 0 rather than the PIDs that had just been removed.
  for (i = 0; i < pat->program[pidx].pmt.nstreams; i++) {
    elementary_pid = pat->program[pidx].pmt.stream[i].elementary_stream_pid;
    state->pid_type[elementary_pid] = PID_UNKNOWN;
  }
  free (pat->program[pidx].pmt.stream);

  pat->program[pidx].pmt.stream = list;
  pat->program[pidx].pmt.nstreams = nstreams;
  pat->program[pidx].pmt.version = version;
  pat->program[pidx].have_pmt = 1;

  // The PMT PID remains PSI. Only the elementary stream PIDs are PES.
  state->pid_type[pid] = PID_PSI;
  for (i = 0; i < nstreams; i++) {
    state->pid_type[list[i].elementary_stream_pid] = PID_PES;
  }

  return (EXIT_SUCCESS);
}
