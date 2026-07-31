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

// Display Definition Segment (DDS)
// Reference: ETSI EN 300 743
int       
parse_dds (STATE *state, PAGE **, size_t *offset, SEGMENT *segment, FILE *fo) {

  size_t segment_length;
  uint8_t sync_byte, segment_type, dds_version_number, display_window_flag;
  uint16_t page_id, display_width, display_height, display_window_h_pos_min, display_window_h_pos_max;
  uint16_t pid, display_window_v_pos_min, display_window_v_pos_max;

  pid = state->pid;

  fprintf (fo, "\n  Display Definition Segment (DDS)\n");

  // Sync Byte (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
    exit (EXIT_FAILURE);
  }
  sync_byte = segment[pid].buffer[*offset];
  if (sync_byte != 0x0f) {
    fprintf (stderr, "Sync byte not found in parse_dds().\n");
    fprintf (stderr, "Found: 0x%02x\n", sync_byte);
    exit (EXIT_FAILURE);
  }         
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", sync_byte);
  (*offset)++;

  // Segment Type (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
    exit (EXIT_FAILURE);
  }
  segment_type = segment[pid].buffer[*offset];
  if (segment_type != 0x14) {
    fprintf (stderr, "Wrong Segment Type found in parse_dds().\n");
    fprintf (stderr, "Found: 0x%02x\n", segment_type);
    exit (EXIT_FAILURE);
  }         
  segment_types (state, segment_type, fo);
  (*offset)++;

  // Page ID (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
    exit (EXIT_FAILURE);
  }
  page_id = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);
  (*offset) += 2;

  // Segment Length (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
    exit (EXIT_FAILURE);
  }
  segment_length = (size_t) ((segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1]);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", segment_length);
  (*offset) += 2;

  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
    exit (EXIT_FAILURE);
  }

  // DDS Version Number (4 bits)
  dds_version_number = (segment[pid].buffer[*offset] >> 4) & 0x0f;  // 0x0f = 1111
  fprintf (fo, "    DDS Version Number (4 bits): 0x%02x\n", dds_version_number);

  // Display Window Flag (1 bit)
  display_window_flag = (segment[pid].buffer[*offset] >> 3) & 1;
  fprintf (fo, "    Display Window Flag (1 bit): %u\n", display_window_flag);

  // Reserved (3 bits)

  (*offset)++;

  // Display Width (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
    exit (EXIT_FAILURE);
  }
  display_width = (segment[pid].buffer[*offset] << 8) |
                   segment[pid].buffer[(*offset) + 1];
  state->display_width = display_width;
  fprintf (fo, "    Display Width (2 bytes): %u px\n", display_width + 1);
  (*offset) += 2;

  // Display Height (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
    exit (EXIT_FAILURE);
  }
  display_height = (segment[pid].buffer[*offset] << 8) |
                   segment[pid].buffer[(*offset) + 1];
  state->display_height = display_height;
  fprintf (fo, "    Display Height (2 bytes): %u px\n", display_height + 1);
  (*offset) += 2;

  // Display Window Minima and Maxima
  // We'll compute the necessary dimenions from the final composition of regions and objects for the page to be rendered.
  if (display_window_flag) {

    // Display Window Horizontal Position Minimum (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
      exit (EXIT_FAILURE);
    }
    display_window_h_pos_min = (segment[pid].buffer[*offset] << 8) |
                   segment[pid].buffer[(*offset) + 1];
    fprintf (fo, "    Display Window Horizontal Position Minimum (2 bytes): %u px\n", display_window_h_pos_min);
    (*offset) += 2;

    // Display Window Horizontal Position Maximum (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
      exit (EXIT_FAILURE);
    }
    display_window_h_pos_max = (segment[pid].buffer[*offset] << 8) |
                   segment[pid].buffer[(*offset) + 1];
    fprintf (fo, "    Display Window Horizontal Position Maximum (2 bytes): %u px\n", display_window_h_pos_max);
    (*offset) += 2;

    // Display Window Vertical Position Minimum (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
      exit (EXIT_FAILURE);
    }
    display_window_v_pos_min = (segment[pid].buffer[*offset] << 8) |
                   segment[pid].buffer[(*offset) + 1];
    fprintf (fo, "    Display Window Vertical Position Minimum (2 bytes): line %u\n", display_window_v_pos_min);
    (*offset) += 2;

    // Display Window Vertical Position Maximum (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_dds().\n");
      exit (EXIT_FAILURE);
    }
    display_window_v_pos_max = (segment[pid].buffer[*offset] << 8) |
                   segment[pid].buffer[(*offset) + 1];
    fprintf (fo, "    Display Window Vertical Position Maximum (2 bytes): line %u\n", display_window_v_pos_max);
    (*offset) += 2;

  }  // End if display_window_flag

  return (EXIT_SUCCESS);
}
