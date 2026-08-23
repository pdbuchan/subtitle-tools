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

// Region Composition Segment (RCS)
// Reference: ETSI EN 300 743
int
parse_rcs (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, FILE *fo) {

  int temp;
  size_t end, page_idx, region_idx, nobjects;
  uint8_t *buf, region_id, object_type;
  uint16_t pid, page_id, object_id;
  REGION *region;
  void *tmp;

  pid = state->pid;
  buf = segment[pid].buffer;
  fprintf (fo, "\n  Region Composition Segment (RCS)\n");

  // Segment header: Sync Byte, Segment Type, Page ID, Segment Length.
  if (!bytes_available (*offset, 6, segment[pid].length)) {
    fprintf (stderr, "Truncated RCS header.\n");
    return (EXIT_FAILURE);
  }
  if (buf[*offset] != 0x0f || buf[*offset + 1] != 0x11) {
    fprintf (stderr, "Invalid RCS sync byte or segment type.\n");
    return (EXIT_FAILURE);
  }

  // Sync Byte (1 byte) and Segment Type (1 byte).
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", buf[*offset]);
  segment_types (state, buf[*offset + 1], fo);

  // Page ID (2 bytes).
  page_id = (uint16_t) (((uint16_t) buf[*offset + 2] << 8) | buf[*offset + 3]);
  state->page_id = page_id;
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);

  // Segment Length (2 bytes).
  end = (size_t) (((uint16_t) buf[*offset + 4] << 8) | buf[*offset + 5]);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", end);
  *offset += 6;
  if (!bytes_available (*offset, end, segment[pid].length)) {
    fprintf (stderr, "RCS segment_length exceeds available PES data.\n");
    return (EXIT_FAILURE);
  }
  end += *offset;

  // Obtain Page array index from page_id.
  temp = find_page_index (state, *page, page_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find page_id 0x%04x in parse_rcs().\n", page_id);
    return (EXIT_FAILURE);
  }
  page_idx = (size_t) temp;

  // Fixed part of the RCS body is ten bytes.
  if (!bytes_available (*offset, 10, end)) {
    fprintf (stderr, "Truncated fixed portion of RCS.\n");
    return (EXIT_FAILURE);
  }

  // Region ID (1 byte).
  region_id = buf[(*offset)++];
  state->region_id = region_id;
  fprintf (fo, "    Region ID (1 byte): 0x%02x\n", region_id);

  // Region IDs are identifiers, not array indexes. Find the compact array
  // index, adding a new region if this is its first definition.
  temp = find_region_index (*page, page_idx, region_id);
  if (temp < 0) {
    if ((*page)[page_idx].nregions >= MAX_REGIONS) {
      fprintf (stderr, "Too many regions in page 0x%04x.\n", page_id);
      return (EXIT_FAILURE);
    }
    region_idx = (*page)[page_idx].nregions;
    tmp = realloc ((*page)[page_idx].region,
                   (region_idx + 1) * sizeof (*(*page)[page_idx].region));
    if (!tmp) {
      fprintf (stderr, "Cannot allocate region %zu.\n", region_idx);
      return (EXIT_FAILURE);
    }
    (*page)[page_idx].region = tmp;
    (*page)[page_idx].nregions++;
  }
  else {
    region_idx = (size_t) temp;
  }

  // An RCS replaces the complete definition of this region version, so clear
  // the REGION structure before populating it with the new data.
  region = &(*page)[page_idx].region[region_idx];
  memset (region, 0, sizeof (*region));
  region->page_id = page_id;
  region->region_id = region_id;

  // Region Version Number (4 bits), Region Fill Flag (1 bit), Reserved (3 bits).
  region->version = (buf[*offset] >> 4) & 0x0f;
  region->fill_flag = (buf[*offset] >> 3) & 1;
  fprintf (fo, "    Region Version Number (4 bits): 0x%01x\n", region->version);
  fprintf (fo, "    Region Fill Flag (1 bit): %u\n", region->fill_flag);
  (*offset)++;

  // Region Width and Height (2 bytes each).
  region->width = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
  *offset += 2;
  region->height = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
  *offset += 2;
  fprintf (fo, "    Region Width (2 bytes): %u px\n", region->width);
  fprintf (fo, "    Region Height (2 bytes): %u px\n", region->height);

  // Region Level of Compatibility (3 bits), Region Depth (3 bits),
  // Reserved (2 bits).
  region->region_level_of_compatibility = (buf[*offset] >> 5) & 7;
  region->depth = (buf[*offset] >> 2) & 7;
  fprintf (fo, "    Region Level of Compatibility (3 bits): 0x%02x\n", region->region_level_of_compatibility);
  fprintf (fo, "    Region Depth (3 bits): 0x%02x\n", region->depth);
  (*offset)++;

  // CLUT ID (1 byte).
  region->clut_id = buf[(*offset)++];
  fprintf (fo, "    CLUT ID (1 byte): 0x%02x\n", region->clut_id);

  // Store these through region_idx, never region_id. A region_id may be any
  // 8-bit value and therefore cannot safely index the allocated region array.

  // Region 8-bit Pixel Code (1 byte).
  region->pixel_code_8bit = buf[(*offset)++];

  // Region 4-bit Pixel Code (4 bits), Region 2-bit Pixel Code (2 bits),
  // Reserved (2 bits). These are the background fill codes selected according
  // to region depth when region_fill_flag is set.
  region->pixel_code_4bit = (buf[*offset] >> 4) & 0x0f;
  region->pixel_code_2bit = (buf[*offset] >> 2) & 0x03;
  fprintf (fo, "    Region 8-bit Pixel Code (8 bits): 0x%02x\n", region->pixel_code_8bit);
  fprintf (fo, "    Region 4-bit Pixel Code (4 bits): 0x%01x\n", region->pixel_code_4bit);
  fprintf (fo, "    Region 2-bit Pixel Code (2 bits): 0x%01x\n", region->pixel_code_2bit);
  (*offset)++;

  // Object Loop. REGION contains a fixed-capacity object_pos[] array, so reject
  // a malformed segment which would exceed MAX_OBJECTS.
  nobjects = 0;
  while (*offset < end) {
    OBJECT_POS *pos;

    if (nobjects >= MAX_OBJECTS) {
      fprintf (stderr, "Too many objects in RCS.\n");
      return (EXIT_FAILURE);
    }
    if (!bytes_available (*offset, 6, end)) {
      fprintf (stderr, "Truncated object entry in RCS.\n");
      return (EXIT_FAILURE);
    }

    pos = &region->object_pos[nobjects];
    memset (pos, 0, sizeof (*pos));

    // Object ID (2 bytes).
    object_id = (uint16_t) (((uint16_t) buf[*offset] << 8) | buf[*offset + 1]);
    pos->object_id = object_id;
    *offset += 2;

    // Object Type (2 bits), Object Provider Flag (2 bits), and high four bits
    // of the 12-bit Object Horizontal Position.
    object_type = (buf[*offset] >> 6) & 3;
    pos->object_type = object_type;
    pos->horizontal_position = (uint16_t) (((buf[*offset] & 0x0f) << 8) | buf[*offset + 1]);
    *offset += 2;

    // Reserved (4 bits) and Object Vertical Position (12 bits).
    pos->vertical_position = (uint16_t) (((buf[*offset] & 0x0f) << 8) | buf[*offset + 1]);
    *offset += 2;

    fprintf (fo, "    Object ID (2 bytes): 0x%04x\n", object_id);
    fprintf (fo, "      Object Type (2 bits): 0x%01x\n", object_type);
    fprintf (fo, "      Object Horizontal Position (12 bits): %u px\n", pos->horizontal_position);
    fprintf (fo, "      Object Vertical Position (12 bits): %u px\n", pos->vertical_position);

    // Basic Character Objects and Composite Character String Objects include
    // 8-bit foreground and background pixel codes after the position data.
    // Reduction to 4-bit or 2-bit CLUT depth is performed during composition.
    if (object_type == 1 || object_type == 2) {
      if (!bytes_available (*offset, 2, end)) {
        fprintf (stderr, "Truncated foreground/background codes in RCS.\n");
        return (EXIT_FAILURE);
      }
      pos->foreground_color = buf[(*offset)++];
      pos->background_color = buf[(*offset)++];
      fprintf (fo, "      Foreground Pixel Code (1 byte): 0x%02x\n", pos->foreground_color);
      fprintf (fo, "      Background Pixel Code (1 byte): 0x%02x\n", pos->background_color);
    }
    nobjects++;
  }

  // Save the number of object-position entries actually present in this RCS.
  region->nobjects = nobjects;

  return (EXIT_SUCCESS);
}
