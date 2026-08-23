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

// Disparity Signalling Segment (DSS)
// Reference: ETSI EN 300 743
int
parse_dss (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, FILE *fo) {

  size_t body_len, end, consumed, i, nsubregions;
  uint8_t *buf, page_update_flag, region_update_flag;
  uint16_t pid = state->pid, page_id;

  (void) page;
  buf = segment[pid].buffer;
  fprintf (fo, "\n  Disparity Signalling Segment (DSS)\n");

  // Segment header: Sync Byte, Segment Type, Page ID, Segment Length.
  if (!bytes_available (*offset, 6, segment[pid].length) || buf[*offset] != 0x0f || buf[*offset + 1] != 0x15) {
    fprintf (stderr, "Invalid or truncated DSS header.\n");
    return (EXIT_FAILURE);
  }

  // Sync Byte (1 byte) and Segment Type (1 byte).
  fprintf (fo, "    Sync Byte: 0x%02x\n", buf[*offset]);
  segment_types (state, buf[*offset + 1], fo);

  // Page ID (2 bytes) and Segment Length (2 bytes).
  page_id = (uint16_t) (((uint16_t) buf[*offset + 2] << 8) | buf[*offset + 3]);
  body_len = (size_t) (((uint16_t) buf[*offset + 4] << 8) | buf[*offset + 5]);
  fprintf (fo, "    Page ID: 0x%04x\n", page_id);
  fprintf (fo, "    Segment Length: %zu bytes\n", body_len);
  *offset += 6;

  if (!bytes_available (*offset, body_len, segment[pid].length) || body_len < 2) {
    fprintf (stderr, "Invalid or truncated DSS body.\n");
    return (EXIT_FAILURE);
  }
  end = *offset + body_len;
  consumed = 0;

  // DSS Version Number (4 bits), Page Update Sequence Flag (1 bit), followed
  // by reserved bits.
  fprintf (fo, "    DSS Version Number: 0x%x\n", (buf[*offset] >> 4) & 0x0f);
  page_update_flag = (buf[*offset] >> 3) & 1;
  fprintf (fo, "    Disparity Shift Update Sequence Page Flag: %u\n", page_update_flag);
  (*offset)++;
  consumed++;

  // Page Default Disparity Shift (1 byte, signed integer part).
  //
  // This byte was not advanced over in the old version. Consequently it was
  // parsed a second time as either the update-sequence length or Region ID.
  fprintf (fo, "    Page Default Disparity Shift: %d\n", (int8_t) buf[*offset]);
  (*offset)++;
  consumed++;

  // Optional page-wide Disparity Shift Update Sequence.
  if (page_update_flag && disparity_shift_update_sequence (state, offset, &consumed, end, segment, fo) != EXIT_SUCCESS) {
    return (EXIT_FAILURE);
  }

  // Region loop. Each entry identifies a region, tells us whether its
  // subregions carry update sequences, and gives the number of subregions.
  while (*offset < end) {
    uint8_t region_id;

    if (!bytes_available (*offset, 2, end)) {
      fprintf (stderr, "Truncated DSS region entry.\n");
      return (EXIT_FAILURE);
    }
    region_id = buf[(*offset)++];
    region_update_flag = (buf[*offset] >> 7) & 1;
    nsubregions = (size_t) (buf[(*offset)++] & 3) + 1;
    consumed += 2;
    fprintf (fo, "      Region ID: 0x%02x\n", region_id);
    fprintf (fo, "      Region Update Sequence Flag: %u\n", region_update_flag);
    fprintf (fo, "      Number of Subregions: %zu\n", nsubregions);

    // Subregion loop. When more than one subregion is present, each carries an
    // explicit horizontal position and width before its disparity value.
    for (i = 0; i < nsubregions; i++) {
      if (nsubregions > 1) {
        uint16_t hpos, width;
        if (!bytes_available (*offset, 4, end)) {
          return (EXIT_FAILURE);
        }
        hpos = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
        width = (uint16_t) (((uint16_t) buf[*offset + 2] << 8) | buf[*offset + 3]);
        *offset += 4;
        consumed += 4;
        fprintf (fo, "        Subregion Horizontal Position: %u px\n", hpos);
        fprintf (fo, "        Subregion Width: %u px\n", width);
      }

      // Subregion disparity shift: signed integer byte plus a 4-bit fractional
      // part in the high nibble of the following byte.
      if (!bytes_available (*offset, 2, end)) {
        return (EXIT_FAILURE);
      }
      fprintf (fo, "        Subregion Disparity Shift Integer Part: %d\n", (int8_t) buf[*offset]);
      fprintf (fo, "        Subregion Disparity Shift Fractional Part: %u\n", (buf[*offset + 1] >> 4) & 0x0f);
      *offset += 2;
      consumed += 2;

      // Optional update sequence applying to this region/subregion.
      if (region_update_flag && disparity_shift_update_sequence (state, offset, &consumed, end, segment, fo) != EXIT_SUCCESS) {
        return (EXIT_FAILURE);
      }
    }
  }

  // consumed independently tracks the bytes described by the DSS syntax; it
  // must agree with both the final offset and the declared segment_length.
  return (*offset == end && consumed == body_len ? EXIT_SUCCESS : EXIT_FAILURE);
}
