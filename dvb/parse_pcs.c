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
  size_t body_len, end, page_idx, nregions, i;
  uint8_t *buf, page_state;
  uint16_t pid, page_id;
  PAGE *pg;
  void *tmp;

  pid = state->pid;
  buf = segment[pid].buffer;
  fprintf (fo, "\n  Page Composition Segment (PCS)\n");

  // Segment header: Sync Byte, Segment Type, Page ID, Segment Length.
  if (!bytes_available (*offset, 6, segment[pid].length)) {
    fprintf (stderr, "Truncated PCS header.\n");
    return (EXIT_FAILURE);
  }
  if (buf[*offset] != 0x0f || buf[*offset + 1] != 0x10) {
    fprintf (stderr, "Invalid PCS sync byte or segment type.\n");
    return (EXIT_FAILURE);
  }

  // Sync Byte (1 byte) and Segment Type (1 byte).
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", buf[*offset]);
  segment_types (state, buf[*offset + 1], fo);

  // Page ID (2 bytes) and Segment Length (2 bytes).
  page_id = (uint16_t) (((uint16_t) buf[*offset + 2] << 8) | buf[*offset + 3]);
  body_len = (size_t) (((uint16_t) buf[*offset + 4] << 8) | buf[*offset + 5]);
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", body_len);
  state->page_id = page_id;
  *offset += 6;

  if (!bytes_available (*offset, body_len, segment[pid].length) || body_len < 2) {
    fprintf (stderr, "Invalid or truncated PCS body.\n");
    return (EXIT_FAILURE);
  }
  end = *offset + body_len;

  // The fixed PCS body is two bytes. Each displayed region then contributes
  // exactly six bytes: region_id, reserved, horizontal address, vertical
  // address.
  if ((body_len - 2) % 6 != 0) {
    fprintf (stderr, "PCS region-position loop has an invalid length.\n");
    return (EXIT_FAILURE);
  }
  nregions = (body_len - 2) / 6;
  if (nregions > MAX_REGIONS) {
    fprintf (stderr, "PCS contains %zu regions; maximum supported is %d.\n",
             nregions, MAX_REGIONS);
    return (EXIT_FAILURE);
  }

  // Allocate memory for this page_id if it has not been encountered before.
  // Page IDs are identifiers and need not correspond to PAGE array indexes.
  temp = find_page_index (state, *page, page_id);
  if (temp < 0) {
    page_idx = state->npages;
    if (page_idx == SIZE_MAX / sizeof (**page)) {
      return (EXIT_FAILURE);
    }
    tmp = realloc (*page, (page_idx + 1) * sizeof (**page));
    if (!tmp) {
      fprintf (stderr, "Cannot allocate page %zu.\n", page_idx);
      return (EXIT_FAILURE);
    }
    *page = tmp;
    memset (&(*page)[page_idx], 0, sizeof (**page));
    (*page)[page_idx].page_id = page_id;
    (*page)[page_idx].buffer = allocate_u8mem (IMG_BUFFER_SIZE);
    state->npages++;
  } else {
    page_idx = (size_t) temp;
  }
  pg = &(*page)[page_idx];

  // An END segment marks the preceding display set complete. Seeing the next
  // PCS for this same page therefore finalizes the previous display set first.
  if (pg->complete) {
    finalize_page_if_needed (state, *page, page_idx, pes);
  }

  // The PTS belonging to this PCS is the start timestamp of the new Display
  // Set for this page.
  pg->start = pes->pts;

  // Page Time-Out (1 byte), Page Version Number (4 bits), Page State (2 bits),
  // followed by 2 reserved bits.
  pg->time_out = buf[(*offset)++];
  pg->version = (buf[*offset] >> 4) & 0x0f;
  page_state = (buf[*offset] >> 2) & 3;
  (*offset)++;

  fprintf (fo, "    Page Time-Out (1 byte): %u seconds\n", pg->time_out);
  fprintf (fo, "    Page Version Number (4 bits): 0x%01x\n", pg->version);
  switch (page_state) {

    case 0:
      fprintf (fo, "    Page State (2 bits): 0x0 Normal Case\n");
      break;

    case 1:
      fprintf (fo, "    Page State (2 bits): 0x1 Acquisition Point\n");
      break;

    case 2:
      fprintf (fo, "    Page State (2 bits): 0x2 Mode Change\n");
      break;

    default:
      fprintf (fo, "    Page State (2 bits): 0x3 Reserved\n");
      break;
  }

  // A PCS may intentionally display no regions. In that case clear any
  // region positions retained from the preceding version of this page.
  if (nregions == 0) {
    free (pg->region_pos);
    pg->region_pos = NULL;
    pg->nregion_pos = 0;
    return (*offset == end ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  // Allocate exactly enough region-position entries for this PCS. Only these
  // regions are displayed, although RCS segments may define additional
  // regions for the same page.
  tmp = realloc (pg->region_pos, nregions * sizeof (*pg->region_pos));
  if (!tmp) {
    fprintf (stderr, "Cannot allocate PCS region positions.\n");
    return (EXIT_FAILURE);
  }
  pg->region_pos = tmp;
  pg->nregion_pos = nregions;

  // Region Address Loop.
  for (i = 0; i < nregions; i++) {
    REGION_POS *rp = &pg->region_pos[i];

    // Region ID (1 byte).
    rp->region_id = buf[(*offset)++];

    // Reserved (1 byte).
    (*offset)++;

    // Region Horizontal Address (2 bytes): pixels from the left of the page.
    rp->region_horizontal_address = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
    *offset += 2;

    // Region Vertical Address (2 bytes): pixels from the top of the page.
    rp->region_vertical_address = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
    *offset += 2;

    fprintf (fo, "    Region ID (1 byte): 0x%02x\n", rp->region_id);
    fprintf (fo, "      Region Horizontal Address: %u px\n", rp->region_horizontal_address);
    fprintf (fo, "      Region Vertical Address: %u px\n", rp->region_vertical_address);
  }

  return (*offset == end ? EXIT_SUCCESS : EXIT_FAILURE);
}
