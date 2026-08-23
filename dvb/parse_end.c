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

// End of Display Set Segment (END)
// This segment indicates that a complete set of pages, regions, CLUTs, and
// objects for the Display Set has been received. The page may now be rendered
// when its end time is established by a subsequent PTS (or by finalization at
// end of input).
// Reference: ETSI EN 300 743
int
parse_end (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, FILE *fo) {

  int temp;
  size_t body_len;
  uint16_t pid, page_id;
  uint8_t *buf;

  pid = state->pid;
  buf = segment[pid].buffer;
  fprintf (fo, "\n  End of Display Set Segment (END)\n");

  // Validate the fixed 6-byte DVB segment header.
  if (!bytes_available (*offset, 6, segment[pid].length) || buf[*offset] != 0x0f || buf[*offset + 1] != 0x80) {
    fprintf (stderr, "Invalid or truncated END segment.\n");
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

  // Although EN 300 743 currently defines END with zero data bytes, honor and
  // safely skip any declared body so an extension cannot desynchronize parsing.
  if (!bytes_available (*offset + 6, body_len, segment[pid].length)) {
    fprintf (stderr, "END segment length exceeds available PES data.\n");
    return (EXIT_FAILURE);
  }
  *offset += 6 + body_len;
  state->page_id = page_id;

  // Find the page index associated with this page_id. It may not exist if an
  // END segment appears before any Page Composition Segment has defined it.
  temp = find_page_index (state, *page, page_id);
  // Mark the page complete only if regions have actually been defined. END can
  // legitimately appear before useful page contents in a transport stream.
  if (temp >= 0 && (*page)[(size_t) temp].nregions > 0) {
    (*page)[(size_t) temp].complete = 1;
  }

  return (EXIT_SUCCESS);
}
