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

// Page Composition Segment (PCS)
// Reference: ETSI EN 300 743
int
parse_pcs (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, PES *pes, FILE *fo) {

  int temp;
  size_t i, consumed, segment_length, nregion_pos, old_size, new_elements, page_idx;
  uint8_t sync_byte, segment_type, page_time_out, page_version_number, page_state, region_id[MAX_REGIONS];
  uint16_t pid, page_id, region_horizontal_address[MAX_REGIONS], region_vertical_address[MAX_REGIONS];
  void *tmp;

  memset (region_id, 0, MAX_REGIONS * sizeof (uint8_t));
  memset (region_horizontal_address, 0, MAX_REGIONS * sizeof (uint16_t));
  memset (region_vertical_address, 0, MAX_REGIONS * sizeof (uint16_t));

  pid = state->pid;

  fprintf (fo, "\n  Page Composition Segment (PCS)\n");

  // Sync Byte (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  sync_byte = segment[pid].buffer[*offset];
  if (sync_byte != 0x0f) {
    fprintf (stderr, "Sync byte not found in parse_pcs().\n");
    fprintf (stderr, "Found: 0x%02x\n", sync_byte);
    exit (EXIT_FAILURE);
  }
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", sync_byte);
  (*offset)++;

  // Segment Type (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  segment_type = segment[pid].buffer[*offset];
  if (segment_type != 0x10) {
    fprintf (stderr, "Wrong Segment Type found in parse_pcs().\n");
    fprintf (stderr, "Found: 0x%02x\n", segment_type);
    exit (EXIT_FAILURE);
  }
  segment_types (state, segment_type, fo);
  (*offset)++;

  // Page ID (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  page_id = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
  state->page_id = page_id;
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);
  (*offset) += 2;

  // Allocate memory for this page_id if not already available.
  temp = find_page_index (state, *page, page_id);
  if ((temp) < 0) {
    old_size = state->npages;
    page_idx = state->npages;  // Note it's a 0-based array.
    tmp = (PAGE *) realloc (*page, (old_size + 1) * sizeof (PAGE));
    if (tmp != NULL) {
      (*page) = tmp;
    } else {
      fprintf (stderr, "Cannot allocate memory for page[%zu] in parse_pcs().\n", page_idx);
      fprintf (stderr, "page_id: 0x%04x\n", page_id);
      exit (EXIT_FAILURE);
    }
    memset (&(*page)[old_size], 0, sizeof (PAGE));  // Clear only new elements.
    (*page)[page_idx].page_id = page_id;
    (*page)[page_idx].version = 0;
    (*page)[page_idx].complete = 0;
    (*page)[page_idx].nregion_pos = 0;  // Number of region positions defined for page[page_id] by parse_pcs(); these are the regions to be displayed.
    (*page)[page_idx].region_pos = NULL;  // We will allocate region positions dynamically as needed.
    (*page)[page_idx].nregions = 0;  // Number of regions defined for page[page_id] by parse_rcs(); none, some, or all of these may be displayed.
    (*page)[page_idx].region = NULL;  // We will allocate regions dynamically as needed.
    (*page)[page_idx].nobjects = 0;
    (*page)[page_idx].object = NULL;  // We will allocate objects and their image buffers dynamically as needed. See parse_ods().
    (*page)[page_idx].ncluts = 0;
    (*page)[page_idx].clut = NULL;  // We will allocate CLUT families dynamically as needed.
    (*page)[page_idx].buffer = allocate_u8mem (IMG_BUFFER_SIZE);  // Buffer for final page composition as RGBA.
    state->npages++;

  // Page already has memory allocated for it.
  } else {
    page_idx = (size_t) temp;
  }

  // If page_id is complete, then render it; we're starting a new Display Set now.
  if ((*page)[page_idx].complete) {
    finalize_page_if_needed (state, *page, page_idx, pes);
  }

  // The latest PTS is the start timestamp of a new Display Set.
  (*page)[page_idx].start = pes->pts;

  // Segment Length (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  segment_length = (size_t) ((segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1]);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", segment_length);
  (*offset) += 2;
  consumed = 0;

  // Page Time-Out (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  page_time_out = segment[pid].buffer[*offset];
  (*page)[page_idx].time_out = page_time_out;
  fprintf (fo, "    Page Time-Out (1 byte): %u seconds\n", page_time_out);
  (*offset)++;
  consumed++;

  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }

  // Page Version Number (4 bits)
  page_version_number = (segment[pid].buffer[*offset] >> 4) & 0x0f;  // 0x0f = 1111
  (*page)[page_idx].version = page_version_number;
  fprintf (fo, "    Page Version Number (4 bits): 0x%01x\n", page_version_number);

  // Page State (2 bits)
  page_state = (segment[pid].buffer[*offset] >> 2) & 0x03;  // 0x03 = 0000 0011
  switch (page_state) {

    case 0x00:  // Normal Case
      fprintf (fo, "    Page State (2 bits): 0x%01x Normal Case\n", page_state);
      break;

    case 0x01:  // Acquisition Point
      fprintf (fo, "    Page State (2 bits): 0x%01x Acquisition Point\n", page_state);
      break;

    case 0x02:  // Mode Change
      fprintf (fo, "    Page State (2 bits): 0x%01x Mode Change\n", page_state);
      break;

    case 0x03:  // Reserved
      fprintf (fo, "    Page State (2 bits): 0x%01x Reserved for future use\n", page_state);
      break;

    default:
      fprintf (stderr, "Unknown Page State in parse_pcs(): 0x%01x\n", page_state);
      exit (EXIT_FAILURE);

  }  // End switch page_state

  // Reserved (2 bits)

  (*offset)++;
  consumed++;

  // Region Address Loop
  // Only these regions will be displayed, although more regions may be defined by parse_rcs().
  nregion_pos = 0;
  while ((consumed < segment_length) && ((*offset) < segment[pid].length)) {

    // Region ID (1 byte)
    if ((*offset) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    region_id[nregion_pos] = segment[pid].buffer[*offset];
    (*offset)++;
    consumed++;

    // Reserved (1 byte)
    if ((*offset) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    (*offset)++;
    consumed++;

    // Region Horizontal Address (px from left of page) (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    region_horizontal_address[nregion_pos] = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
    (*offset) += 2;
    consumed += 2;

    // Region Vertical Address (px from top of page) (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    region_vertical_address[nregion_pos] = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
    (*offset) += 2;
    consumed += 2;

    fprintf (fo, "    Region ID (1 byte): 0x%02x\n", region_id[nregion_pos]);
    fprintf (fo, "      Region Horizontal Address (2 bytes): %u px from left of page\n", region_horizontal_address[nregion_pos]);
    fprintf (fo, "      Region Vertical Address (2 bytes): %u px from top of page\n", region_vertical_address[nregion_pos]);

    // Increment count of regions for this page.
    nregion_pos++;

  }  // End while

  // Allocate memory for region positions if not already available.
  if (((*page)[page_idx].nregion_pos < nregion_pos) || ((*page)[page_idx].region_pos == NULL)) {

    old_size = (*page)[page_idx].nregion_pos;
    (*page)[page_idx].nregion_pos = nregion_pos;
    if (nregion_pos > old_size) {

      // Region positions
      tmp = (REGION_POS *) realloc ((*page)[page_idx].region_pos, nregion_pos * sizeof (REGION_POS));
      if (tmp != NULL) {
        (*page)[page_idx].region_pos = tmp;
      } else {
        fprintf (stderr, "Cannot allocate memory for page[%zu].region_pos array in parse_pcs().\n", page_idx);
        fprintf (stderr, "page_id: 0x%04x, nregion_pos: %zu\n", page_id, nregion_pos);
        exit (EXIT_FAILURE);
      }
    }

    // Initialize only the newly allocated memory.
    new_elements = nregion_pos - old_size;
    if (new_elements > 0) {
      memset (&(*page)[page_idx].region_pos[old_size], 0, new_elements * sizeof (REGION_POS));
    }
  }

  // Store region_id's and region positions for this page.
  // Only these regions will be displayed, although more regions may be defined by parse_rcs().
  (*page)[page_idx].nregion_pos = nregion_pos;
  for (i = 0; i < nregion_pos; i++) {
    (*page)[page_idx].region_pos[i].region_id = region_id[i];
    (*page)[page_idx].region_pos[i].region_horizontal_address = region_horizontal_address[i];
    (*page)[page_idx].region_pos[i].region_vertical_address = region_vertical_address[i];
  }

  return (EXIT_SUCCESS);
}
