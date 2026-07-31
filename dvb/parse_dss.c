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

// Disparity Signalling Segment
// Reference: ETSI EN 300 743
int
parse_dss (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, FILE *fo) {

  size_t i, consumed, segment_length;
  int8_t page_default_disparity_shift;
  uint8_t sync_byte, segment_type, dss_version_number, disparity_shift_update_sequence_page_flag, region_id, disparity_shift_update_sequence_region_flag;
  uint8_t number_of_subregions_minus_1, subregion_disparity_shift_integer_part, subregion_disparity_shift_fractional_part;
  uint16_t pid, page_id, subregion_horizontal_position, subregion_width;

  pid = state->pid;

  fprintf (fo, "\n  Disparity Signalling Segment (DSS)\n");

  // Sync Byte (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
    exit (EXIT_FAILURE);
  }
  sync_byte = segment[pid].buffer[*offset];
  if (sync_byte != 0x0f) {
    fprintf (stderr, "Sync byte not found in parse_dss().\n");
    fprintf (stderr, "Found: 0x%02x\n", sync_byte);
    exit (EXIT_FAILURE);
  }
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", sync_byte);
  (*offset)++;

  // Segment Type (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
    exit (EXIT_FAILURE);
  }
  segment_type = segment[pid].buffer[*offset];
  if (segment_type != 0x15) {
    fprintf (stderr, "Wrong Segment Type found in parse_dss().\n");
    fprintf (stderr, "Found: 0x%02x\n", segment_type);
    exit (EXIT_FAILURE);
  }
  segment_types (state, segment_type, fo);
  (*offset)++;

  // Page ID (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
    exit (EXIT_FAILURE);
  }
  page_id = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);
  (*offset) += 2;

  // Segment Length (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
    exit (EXIT_FAILURE);
  }
  segment_length = (size_t) ((segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1]);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", segment_length);
  (*offset) += 2;
  consumed = 0;

  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
    exit (EXIT_FAILURE);
  }

  // DSS Version Number (4 bits)
  dss_version_number = (segment[pid].buffer[*offset] >> 4) & 0x0f;  // 0x0f = 1111
  fprintf (fo, "    DDS Version Number (4 bits): 0x%02x\n", dss_version_number);

  // Disparity Shift Update Sequence Page Flag (1 bit)
  disparity_shift_update_sequence_page_flag = (segment[pid].buffer[*offset] >> 3) & 1;
  fprintf (fo, "    Disparity Shift Update Sequence Page Flag (1 bit): %u\n", disparity_shift_update_sequence_page_flag);

  // Reserved (3 bits)

  (*offset)++;
  consumed++;

  // Page Default Disparity Shift (1 byte, two's complement)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
    exit (EXIT_FAILURE);
  }
  page_default_disparity_shift = segment[pid].buffer[*offset];
  fprintf (fo, "    Page Default Disparity Shift (1 byte): %d\n", (int8_t) page_default_disparity_shift);

  if (disparity_shift_update_sequence_page_flag) {
    disparity_shift_update_sequence (state, offset, &consumed, segment, fo);
  }

  // Region Loop
  while ((consumed < segment_length) && ((*offset) < segment[pid].length)) {

    // Region ID (1 byte)
    if ((*offset) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
      exit (EXIT_FAILURE);
    }
    region_id = segment[pid].buffer[*offset];
    fprintf (fo, "      Region ID (1 byte): 0x%02x\n", region_id);
    (*offset)++;
    consumed++;

    if ((*offset) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
      exit (EXIT_FAILURE);
    }

    // Disparity Shift Update Sequence Region Flag (1 bit)
    disparity_shift_update_sequence_region_flag = (segment[pid].buffer[*offset] >> 7) & 1;
    fprintf (fo,  "      Disparity Shift Update Sequence Region Flag (1 bit): %u\n", disparity_shift_update_sequence_region_flag);

    // Reserved (5 bits)

    // Number of Subregions Minus 1 (2 bits)
    number_of_subregions_minus_1 = segment[pid].buffer[*offset] & 0x03;  // 0x03 = 11
    fprintf (fo, "      Number of Subregions Minus 1 (2 bits): %u\n", number_of_subregions_minus_1);
    (*offset)++;
    consumed++;

    // Subregion Loop
    for (i = 0; i <= (size_t) number_of_subregions_minus_1; i++) {

      if (number_of_subregions_minus_1 > 0) {

        // Subregion Horizontal Position (2 bytes)
        if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
          fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
          exit (EXIT_FAILURE);
        }
        subregion_horizontal_position = (segment[pid].buffer[*offset] << 8) |
                                         segment[pid].buffer[(*offset) + 1];
        fprintf (fo, "      Subregion Horizontal Position (2 bytes): %u px\n", subregion_horizontal_position);
        (*offset) += 2;
        consumed += 2;

        // Subregion Width (2 bytes)
        if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
          fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
          exit (EXIT_FAILURE);
        }
        subregion_width = (segment[pid].buffer[*offset] << 8) |
                                         segment[pid].buffer[(*offset) + 1];
        fprintf (fo, "      Subregion Width (2 bytes): %u px\n", subregion_width);
        (*offset) += 2;
        consumed += 2;

      }  // End if number_of_subregions_minus_1 > 0
  
      // Subregion Disparity Shift Integer Part (1 byte, two's complement)
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
        exit (EXIT_FAILURE);
      }
      subregion_disparity_shift_integer_part = segment[pid].buffer[*offset];
      fprintf (fo, "      Subregion Disparity Shift Integer Part (1 byte): %d\n", (int8_t) subregion_disparity_shift_integer_part);
      (*offset)++;
      consumed++;

      // Subregion Disparity Shift Fractional Part (4 bits)
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_dss().\n");
        exit (EXIT_FAILURE);
      }
      subregion_disparity_shift_fractional_part = (segment[pid].buffer[*offset] >> 4) & 0x0f;  // 0x0f = 0000 1111
      fprintf (fo, "      Subregion Disparity Shift Fractional Part (4 bits): %u\n", subregion_disparity_shift_fractional_part);

      // Reserved (4 bits)

      (*offset)++;
      consumed++;

      if (disparity_shift_update_sequence_region_flag) {
          disparity_shift_update_sequence (state, offset, &consumed, segment, fo);
      }  // End if disparity_shift_update_sequence_region_flag

    }  // End subregion loop

  }  // End while region loop

  return (EXIT_SUCCESS);
}
