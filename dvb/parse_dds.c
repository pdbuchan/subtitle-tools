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
parse_dds (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, FILE *fo) {

  size_t body_len, end;
  uint8_t *buf, window_flag, version;
  uint16_t pid, page_id, width_minus_1, height_minus_1;

  (void) page;
  pid = state->pid;
  buf = segment[pid].buffer;
  fprintf (fo, "\n  Display Definition Segment (DDS)\n");

  // Every DVB subtitle segment starts with a 6-byte segment header.
  if (!bytes_available (*offset, 6, segment[pid].length) ||
      buf[*offset] != 0x0f || buf[*offset + 1] != 0x14) {
    fprintf (stderr, "Invalid or truncated DDS header.\n");
    return (EXIT_FAILURE);
  }
  // Sync Byte (1 byte) and Segment Type (1 byte)
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", buf[*offset]);
  segment_types (state, buf[*offset + 1], fo);
  // Page ID (2 bytes)
  page_id = (uint16_t) (((uint16_t) buf[*offset + 2] << 8) | buf[*offset + 3]);
  // Segment Length (2 bytes)
  body_len = (size_t) (((uint16_t) buf[*offset + 4] << 8) | buf[*offset + 5]);
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", body_len);
  *offset += 6;

  // The fixed part of a DDS body occupies 5 bytes.
  if (!bytes_available (*offset, body_len, segment[pid].length) || body_len < 5) {
    fprintf (stderr, "Invalid or truncated DDS body.\n");
    return (EXIT_FAILURE);
  }
  end = *offset + body_len;

  // DDS Version Number (4 bits) and Display Window Flag (1 bit).
  // The remaining 3 bits of this byte are reserved.
  version = (buf[*offset] >> 4) & 0x0f;
  window_flag = (buf[*offset] >> 3) & 1;
  (*offset)++;
  // Display Width (2 bytes) is coded as the maximum horizontal coordinate,
  // therefore the number of displayed pixels is display_width + 1.
  width_minus_1 = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
  *offset += 2;
  // Display Height (2 bytes) is coded in the same way.
  height_minus_1 = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
  *offset += 2;

  state->display_width = width_minus_1;
  state->display_height = height_minus_1;
  fprintf (fo, "    DDS Version Number (4 bits): 0x%02x\n", version);
  fprintf (fo, "    Display Window Flag (1 bit): %u\n", window_flag);
  fprintf (fo, "    Display Width (2 bytes): %u px\n", (unsigned) width_minus_1 + 1U);
  fprintf (fo, "    Display Height (2 bytes): %u px\n", (unsigned) height_minus_1 + 1U);

  // If a display window is present, retrieve its horizontal and vertical
  // minima and maxima. The values are reported even though composition bounds
  // are determined from the regions and objects actually rendered.
  if (window_flag) {
    uint16_t hmin, hmax, vmin, vmax;
    if (!bytes_available (*offset, 8, end)) {
      fprintf (stderr, "Truncated DDS display-window fields.\n");
      return (EXIT_FAILURE);
    }
    // Display Window Horizontal Position Minimum (2 bytes)
    hmin = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
    *offset += 2;
    // Display Window Horizontal Position Maximum (2 bytes)
    hmax = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
    *offset += 2;
    // Display Window Vertical Position Minimum (2 bytes)
    vmin = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
    *offset += 2;
    // Display Window Vertical Position Maximum (2 bytes)
    vmax = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
    *offset += 2;
    fprintf (fo, "    Display Window Horizontal Position Minimum: %u px\n", hmin);
    fprintf (fo, "    Display Window Horizontal Position Maximum: %u px\n", hmax);
    fprintf (fo, "    Display Window Vertical Position Minimum: %u lines\n", vmin);
    fprintf (fo, "    Display Window Vertical Position Maximum: %u lines\n", vmax);
  }

  // The decoder should finish exactly at the end declared by segment_length.
  if (*offset != end) {
    fprintf (stderr, "DDS segment length does not match its fields.\n");
    return (EXIT_FAILURE);
  }
  return (EXIT_SUCCESS);
}
