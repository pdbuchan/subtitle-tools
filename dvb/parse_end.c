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
// This segment indicates that a complete set of elements (pages, regions, cluts, objects) for a Display Set have been received.
// The final composition can now be prepared in page[page_id].buffer at the appropriate time.
// We assume only 1 Page per Display Segment, therefore we take this as meaning page_id is complete.
// The time to render occurs when a new PTS occurs via PES header.
// Reference: ETSI EN 300 743
int        
parse_end (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, FILE *fo) {

  int temp;
  size_t page_idx, segment_length;
  uint8_t sync_byte, segment_type;
  uint16_t pid, page_id;

  pid = state->pid;

  fprintf (fo, "\n  End of Display Set Segment (END)\n");

  // Sync Byte (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_end().\n");
    exit (EXIT_FAILURE);
  }
  sync_byte = segment[pid].buffer[*offset];
  if (sync_byte != 0x0f) {
    fprintf (stderr, "Sync byte not found in parse_end().\n");
    fprintf (stderr, "Found: 0x%02x\n", sync_byte);
    exit (EXIT_FAILURE);
  } 
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", sync_byte);
  (*offset)++;

  // Segment Type (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_end().\n");
    exit (EXIT_FAILURE);
  }
  segment_type = segment[pid].buffer[*offset];
  if (segment_type != 0x80) {
    fprintf (stderr, "Wrong Segment Type found in parse_end().\n");
    fprintf (stderr, "Found: 0x%02x\n", segment_type);
    exit (EXIT_FAILURE);
  }
  segment_types (state, segment_type, fo);
  (*offset)++;

  // Page ID (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_end().\n");
    exit (EXIT_FAILURE);
  }
  page_id = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
  state->page_id = page_id;
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);
  (*offset) += 2;

  // Segment Length (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_end().\n");
    exit (EXIT_FAILURE);
  }
  segment_length = (size_t) ((segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1]);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", segment_length);
  (*offset) += 2;

  // Find page index for page_id. 
  // It may not succeed if we haven't created any pages yet.
  temp = find_page_index (state, *page, page_id);
  if (temp > -1) {
    page_idx = (size_t) temp;

    // Mark Page with current page_id as complete and ready for display
    // but only if we actually have regions defined. We do this check because
    // the End of Display Set segment can appear prior to anything being defined.
    if ((*page)[page_idx].nregions > 0) {
      (*page)[page_idx].complete = 1;
    }
  }

  return (EXIT_SUCCESS);
}
