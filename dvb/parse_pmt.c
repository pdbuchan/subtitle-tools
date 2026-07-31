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

// Parse Program Map Table (PMT).
// One PMT lists all PES PIDs and their Stream Types associated with a given Program Number.
// Reference: ISO/IEC 13818-1
int
parse_pmt (STATE *state, PAT *pat, SECTION *section, FILE *fo) {

  int index;
  size_t i, offset, end, section_length, program_info_length, stream, nstreams, es_info_len, old_size, program_idx, new_elements;
  uint8_t table_id, section_syntax_indicator, version_number, current_next_indicator, section_number, last_section_number, stream_type[MAX_STREAMS];
  uint16_t pid, program_number, pcr_pid, elementary_stream_pid[MAX_STREAMS];
  void *tmp;

  pid = state->pid;

  // Set some arrays to 0.
  memset (stream_type, 0, MAX_STREAMS * sizeof (uint8_t));
  memset (elementary_stream_pid, 0, MAX_STREAMS * sizeof (uint16_t));

  offset = 0;  // Start at beginning of section.

  fprintf (fo, "Program Map Table (PMT):\n");

  // Table ID (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
    exit (EXIT_FAILURE);
  }
  table_id = section[pid].buffer[offset];
  fprintf (fo, "  Table ID (1 byte): 0x%02x\n", table_id);
  offset++;

  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
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

  // Program Number (2 bytes)
  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
    exit (EXIT_FAILURE);
  }
  program_number = (section[pid].buffer[offset]) << 8 |
                 section[pid].buffer[offset + 1];
  fprintf (fo, "  Program Number (2 bytes): 0x%04x\n", program_number);
  offset += 2;

  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
    exit (EXIT_FAILURE);
  }

  // Reserved (2 bits)

  // Version Number (5 bits)
  version_number = (section[pid].buffer[offset] >> 1) & 0x1f;  // 0x1f = 0001 1111
  fprintf (fo, "  Version Number (5 bits): 0x%02x\n", version_number);

  // Normally you don't bother processing anything more if version hasn't changed.
  // We will continue anyway for the sake of the output file.

  // Current Next Indicator (1 bit)
  current_next_indicator = section[pid].buffer[offset] & 1;
  fprintf (fo, "  Current Next Indicator (1 bit): %u\n", current_next_indicator);
  offset++;

  // Section Number (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
    exit (EXIT_FAILURE);
  }
  section_number = section[pid].buffer[offset];
  fprintf (fo, "  Section Number (1 byte): 0x%02x\n", section_number);
  offset++;

  // Last Section Number (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
    exit (EXIT_FAILURE);
  }
  last_section_number = section[pid].buffer[offset];
  fprintf (fo, "  Last Section Number (1 byte): 0x%02x\n", last_section_number);
  offset++;

  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
    exit (EXIT_FAILURE);
  }

  // Reserved (3 bits)

  // Program Clock Reference (PCR) PID (13 bits)
  pcr_pid = ((section[pid].buffer[offset] & 0x1f) << 8) |
             section[pid].buffer[offset + 1];
  fprintf (fo, "  Program Clock Reference (PCR) PID (13 bits): 0x%04x\n", pcr_pid);
  offset += 2;

  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
    exit (EXIT_FAILURE);
  }

  // Reserved (4 bits)

  // Program Info Length (12 bits)
  program_info_length = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) |
             section[pid].buffer[offset + 1]);
  fprintf (fo, "  Program Info Length (12 bits): %zu bytes\n", program_info_length);
  offset += 2;

  // Descriptor Loop
  if (program_info_length > 0) {
    fprintf (fo, "  Program Descriptors:\n");
    offset += program_info_length;  // Skip program-level descriptors
  }

  // Elementary Stream (ES) Loop
  end = 3 + section_length - 4;  // end of ES loop (before CRC)
  nstreams = 0;  // Initially assume no elementary streams.

  while ((offset + 5) <= end) {  // Minimum ES entry size

    fprintf (fo, "  Elementary Stream %lu:\n", nstreams);

    // Stream Type (1 byte)
    if (offset >= MAX_BUFFERLEN) {
      fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
      exit (EXIT_FAILURE);
    }
    stream_type[nstreams] = section[pid].buffer[offset];
    stream_types (state, stream_type[nstreams], fo);  // Prints description appropriate for stream_type.
    offset++;

    if ((offset + 1) >= MAX_BUFFERLEN) {
      fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
      exit (EXIT_FAILURE);
    }
    // Reserved (3 bits)

    // Elementary PID (13 bits)
    elementary_stream_pid[nstreams] = ((section[pid].buffer[offset] & 0x1f) << 8) |
                       section[pid].buffer[offset + 1];
    fprintf (fo, "    Elementary PID (13 bits): 0x%04x\n", elementary_stream_pid[nstreams]);
    offset += 2;

    if ((offset + 1) >= MAX_BUFFERLEN) {
      fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
      exit (EXIT_FAILURE);
    }

    // Reserved (4 bits)

    // ES Info Length (12 bits)
    es_info_len = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) |
                   section[pid].buffer[offset + 1]);
    fprintf (fo, "    ES Info Length (12 bits): %zu bytes\n", es_info_len);
    offset += 2;

    // ES Descriptor Loop
    offset += es_info_len;  // Skip ES descriptors

    nstreams++;
    if (nstreams > 1) {
      fprintf (stderr, "Warning: More than one elementary streams listed in PMT.\n");
      fprintf (stderr, "         The first elementary stream with DVB subtitles will be used:\n");
      fprintf (stderr, "         Stream Type == 0x06, PES data ID == 0x20, DVB subtitle stream ID == 00\n");
    }

    // Set flag indicating PID listed in PMT corresponds to PES packets.
    state->pid_type[state->pid] = PID_PES;

  }

  // CRC (4 bytes)
  if ((offset + 3) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_pmt().\n");
    exit (EXIT_FAILURE);
  }
  fprintf (fo, "  CRC (4 bytes): ");
  for (i = 0; i < 4; i++) {
    fprintf (fo, "%02x", section[pid].buffer[offset + i]);
  }
  fprintf (fo, "\n");

  // Determine the program index corresponding to current PID.
  // If no program has been recorded for this PMT PID (weird, but could happen depending upon when recording of transport stream commenced),
  // then no need to do anything else; a PAT will come along since they are transmitted roughly every 1/10th of a second.
  if ((index = find_program_by_pmt_pid (pat, state->pid)) < 0) {
    return (EXIT_SUCCESS);

  // Already have a program index for this PID.
  } else {
    program_idx = (size_t) index;
  }

  // If we already have the PMT with this version number, then no need to do anything else.
  if ((pat->program[program_idx].pmt.version == version_number) && pat->program[program_idx].have_pmt) {
    return (EXIT_SUCCESS);

  // Must be new PMT or new version of PMT.
  } else {

    // Update version number.
    pat->program[program_idx].pmt.version = version_number;

    // Allocate memory for stream array, if more is needed.
    old_size = pat->program[program_idx].pmt.nstreams;
    if (nstreams > old_size) {
      tmp = (PMT_STREAM *) realloc (pat->program[program_idx].pmt.stream, nstreams * sizeof (PMT_STREAM));
      if (tmp != NULL) {
        pat->program[program_idx].pmt.stream = tmp;
      } else {
        fprintf (stderr, "Cannot allocate memory for pmt->program[%zu].pmt.stream array of type PMT_STREAM.\n", program_idx);
        fprintf (stderr, "nstreams: %zu\n", nstreams);
        exit (EXIT_FAILURE);
      }

      // Initialize only the newly allocated memory.
      new_elements = nstreams - old_size; // Calculate the number of new elements.
      if (new_elements > 0) {
        memset (&pat->program[program_idx].pmt.stream[old_size], 0, new_elements * sizeof (PMT_STREAM));

        // Update pid_type for new PIDs to PID_UNKNOWN in state struct.
        for (stream = old_size; stream < nstreams; stream++) {
          state->pid_type[pat->program[program_idx].pmt.stream[stream].elementary_stream_pid] = PID_UNKNOWN;
        }
      }

    // Enough memory is already allocated. Just clear it.
    // nstreams <= old_size
    } else {
      for (stream = 0; stream < old_size; stream++) {
        pat->program[program_idx].pmt.stream[stream].elementary_stream_pid = 0;
        pat->program[program_idx].pmt.stream[stream].stream_type = 0;
        state->pid_type[pat->program[program_idx].pmt.stream[stream].elementary_stream_pid] = PID_UNKNOWN;
      }
    }

    // Copy stream IDs and types, and mark them as PES streams.
    // All elementary streams listed in a PMT are, by definition, PES streams.
    pat->program[program_idx].pmt.nstreams = nstreams;
    for (stream = 0; stream < nstreams; stream++) {
      pat->program[program_idx].pmt.stream[stream].elementary_stream_pid = elementary_stream_pid[stream];
      pat->program[program_idx].pmt.stream[stream].stream_type = stream_type[stream];
      state->pid_type[pat->program[program_idx].pmt.stream[stream].elementary_stream_pid] = PID_PES;
    }

    // Mark this program as having a parsed PMT. i.e., we have the (elementary_stream_pid, stream_type) values for
    // each elementary stream, and we have identified those PIDs as PES.
    pat->program[program_idx].have_pmt = 1;
  }

  return (EXIT_SUCCESS);
}
