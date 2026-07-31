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
  size_t i, consumed, segment_length, nobjects, old_size, page_idx, region_idx;
  uint8_t sync_byte, segment_type, region_id, region_version_number, region_fill_flag, region_level_of_compatibility, region_depth, clut_id;
  uint8_t region_8_bit_pixel_code, region_4_bit_pixel_code, region_2_bit_pixel_code, object_type, object_provider_flag;
  uint8_t foreground_pixel_code, background_pixel_code;
  uint16_t pid, page_id, region_width, region_height, object_id, object_horizontal_position, object_vertical_position;
  OBJECT_POS object_pos[MAX_OBJECTS] = {0};
  void *tmp;

  pid = state->pid;

  fprintf (fo, "\n  Region Composition Segment (RCS)\n");

  // Sync Byte (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  sync_byte = segment[pid].buffer[*offset];
  if (sync_byte != 0x0f) {
    fprintf (stderr, "Sync byte not found in parse_rcs().\n");
    fprintf (stderr, "Found: 0x%02x\n", sync_byte);
    exit (EXIT_FAILURE);
  }
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", sync_byte);
  (*offset)++;

  // Segment Type (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  segment_type = segment[pid].buffer[*offset];
  if (segment_type != 0x11) {
    fprintf (stderr, "Wrong Segment Type found in parse_rcs().\n");
    fprintf (stderr, "Found: 0x%02x\n", segment_type);
    exit (EXIT_FAILURE);
  }
  segment_types (state, segment_type, fo);
  (*offset)++;

  // Page ID (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  page_id = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
  state->page_id = page_id;
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);
  (*offset) += 2;

  // Obtain Page index from page_id.
  temp = find_page_index (state, *page, page_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find index for page_id: 0x%04x in parse_rcs().\n", page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  // Segment Length (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  segment_length = (size_t) ((segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1]);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", segment_length);
  (*offset) += 2;
  consumed = 0;

  // Region ID (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  region_id = segment[pid].buffer[*offset];
  fprintf (fo, "    Region ID (1 byte): 0x%02x\n", region_id);
  state->region_id = region_id;
  (*offset)++;
  consumed++;

  // Search the regions of page[page_id] to retrieve state->region_id's index.
  // If we can't find it, allocate memory for this region.
  temp = find_region_index (state, *page, region_id);
  if (temp < 0) {
    old_size = (*page)[page_idx].nregions;
    region_idx = (*page)[page_idx].nregions;  // Note it's a 0-based array.
    tmp = (REGION *) realloc ((*page)[page_idx].region, (old_size + 1) * sizeof (REGION));
    if (tmp != NULL) {
      (*page)[page_idx].region = tmp;
    } else {
      fprintf (stderr, "Cannot allocate memory for region[%zu] in parse_rcs().\n", region_idx);
      fprintf (stderr, "region_id: 0x%04x\n", region_id);
      exit (EXIT_FAILURE);
    }
    memset (&((*page)[page_idx].region)[old_size], 0, sizeof (REGION));  // Clear only new elements.
    (*page)[page_idx].region[region_idx].page_id = page_id;
    (*page)[page_idx].region[region_idx].region_id = region_id;
    (*page)[page_idx].region[region_idx].version = 0;
    (*page)[page_idx].region[region_idx].nobjects = 0;
    (*page)[page_idx].nregions++;

  // Region already has memory allocated for it. Clear it for new data.
  } else {
    region_idx = (size_t) temp;
    memset (&(*page)[page_idx].region[region_idx], 0, sizeof (REGION));
    (*page)[page_idx].region[region_idx].page_id = page_id;
    (*page)[page_idx].region[region_idx].region_id = region_id;
  }

  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }

  // Region Version Number (4 bits)
  region_version_number = (segment[pid].buffer[*offset] >> 4) & 0x0f;  // 0x0f = 1111
  (*page)[page_idx].region[region_idx].version = region_version_number;
  fprintf (fo, "    Region Version Number (4 bits): 0x%01x\n", region_version_number);

  // Region Fill Flag (1 bit)
  region_fill_flag = (segment[pid].buffer[*offset] >> 3) & 1;
  (*page)[page_idx].region[region_idx].fill_flag = region_fill_flag;
  fprintf (fo, "    Region Fill Flag (1 bit): %u\n", region_fill_flag);

  // Reserved (3 bits)

  (*offset)++;
  consumed++;

  // Region Width (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  region_width = (segment[pid].buffer[*offset] << 8) |
                  segment[pid].buffer[(*offset) + 1];
  (*page)[page_idx].region[region_idx].width = region_width;
  fprintf (fo, "    Region Width (2 bytes): %u px\n", region_width);
  (*offset) += 2;
  consumed += 2;

  // Region Height (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  region_height = (segment[pid].buffer[*offset] << 8) |
                  segment[pid].buffer[(*offset) + 1];
  (*page)[page_idx].region[region_idx].height = region_height;
  fprintf (fo, "    Region Height (2 bytes): %u px\n", region_height);
  (*offset) += 2;
  consumed += 2;

  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }

  // Region Level of Compatibility (3 bits)
  region_level_of_compatibility = (segment[pid].buffer[*offset] >> 5) & 0x07;  // 0x07 = 111
  (*page)[page_idx].region[region_idx].region_level_of_compatibility = region_level_of_compatibility;
  switch (region_level_of_compatibility) {

    case 0x00:
      fprintf (fo, "    Region Level of Compatibility (3 bits): 0x%02x Reserved\n", region_level_of_compatibility);
      break;

    case 0x01:
      fprintf (fo, "    Region Level of Compatibility (3 bits): 0x%02x 2-bit/entry CLUT required\n", region_level_of_compatibility);
      break;

    case 0x02:
      fprintf (fo, "    Region Level of Compatibility (3 bits): 0x%02x 4-bit/entry CLUT required\n", region_level_of_compatibility);
      break;

    case 0x03:
      fprintf (fo, "    Region Level of Compatibility (3 bits): 0x%02x 8-bit/entry CLUT required\n", region_level_of_compatibility);
      break;

    default:
      if ((region_level_of_compatibility >= 0x04) && (region_level_of_compatibility <= 0x07)) {
        fprintf (fo, "    Region Level of Compatibility (3 bits): 0x%02x Reserved\n", region_level_of_compatibility);
        break;
      } else {
        fprintf (stderr, "Unknown Region Level of Compatibility (3 bits) 0x%02x in parse_rcs().\n", region_level_of_compatibility);
        exit (EXIT_FAILURE);
      }

  }  // End switch

  // Region Depth (3 bits)
  region_depth = (segment[pid].buffer[*offset] >> 2) & 0x07;  // 0x07 = 111
  (*page)[page_idx].region[region_idx].depth = region_depth;
  switch (region_depth) {

    case 0x00:
      fprintf (fo, "    Region Depth (3 bits): 0x%02x Reserved\n", region_depth);
      break;

    case 0x01:
      fprintf (fo, "    Region Depth (3 bits): 0x%02x 2-bit pixel depth\n", region_depth);
      break;

    case 0x02:
      fprintf (fo, "    Region Depth (3 bits): 0x%02x 4-bit pixel depth\n", region_depth);
      break;

    case 0x03:
      fprintf (fo, "    Region Depth (3 bits): 0x%02x 8-bit pixel depth\n", region_depth);
      break;

    default:
      if ((region_depth >= 0x04) && (region_depth <= 0x07)) {
        fprintf (fo, "    Region Depth (3 bits): 0x%02x Reserved\n", region_depth);
        break;
      } else {
        fprintf (stderr, "Unknown Region Depth (3 bits) 0x%02x in parse_rcs().\n", region_depth);
        exit (EXIT_FAILURE);
      }

  }  // End switch

  // Reserved (2 bits)

  (*offset)++;
  consumed++;

  // CLUT ID (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  clut_id = segment[pid].buffer[*offset];
  (*page)[page_idx].region[region_idx].clut_id = clut_id;
  fprintf (fo, "    CLUT ID (1 byte): 0x%02x\n", clut_id);
  (*offset)++;
  consumed++;

  // Region 8-bit Pixel Code (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }
  region_8_bit_pixel_code = segment[pid].buffer[*offset];
  (*page)[page_idx].region[region_id].pixel_code_8bit = region_8_bit_pixel_code;
  fprintf (fo, "    Region 8-bit Pixel Code (8 bits): 0x%02x\n", region_8_bit_pixel_code);
  (*offset)++;
  consumed++;

  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
    exit (EXIT_FAILURE);
  }

  // Region 4-bit Pixel Code (4 bits)
  region_4_bit_pixel_code = (segment[pid].buffer[*offset] >> 4) & 0x0f;  // 0x0f = 1111
  (*page)[page_idx].region[region_id].pixel_code_4bit = region_4_bit_pixel_code;
  fprintf (fo, "    Region 4-bit Pixel Code (4 bits): 0x%01x\n", region_4_bit_pixel_code);

  // Region 2-bit Pixel Code (2 bits)
  region_2_bit_pixel_code = (segment[pid].buffer[*offset] >> 2) & 0x03;  // 0x03 = 11
  (*page)[page_idx].region[region_id].pixel_code_2bit = region_2_bit_pixel_code;
  fprintf (fo, "    Region 2-bit Pixel Code (2 bits): 0x%01x\n", region_2_bit_pixel_code);

  // Reserved (2 bits)

  (*offset)++;
  consumed++;

  // Object Loop
  nobjects = 0;
  while ((consumed < segment_length) && ((*offset) < segment[pid].length)) {

    // Object ID (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
      exit (EXIT_FAILURE);
    }
    object_id = (segment[pid].buffer[*offset] << 8) |
                 segment[pid].buffer[(*offset) + 1];
    object_pos[nobjects].object_id = object_id;
    fprintf (fo, "    Object ID (2 bytes): 0x%04x\n", object_id);
    (*offset) += 2;
    consumed += 2;

    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
      exit (EXIT_FAILURE);
    }

    // Object Type (2 bits)
    object_type = (segment[pid].buffer[*offset] >> 6) & 0x03;  // 0x03 = 11
    object_pos[nobjects].object_type = object_type;
    switch (object_type) {

      case 0x00:
        fprintf (fo, "      Object Type (2 bits): 0x%01x Basic object, bitmap\n", object_type);
        break;

      case 0x01:
        fprintf (fo, "      Object Type (2 bits): 0x%01x Basic object, character\n", object_type);
        break;

      case 0x02:
        fprintf (fo, "      Object Type (2 bits): 0x%01x Composite object, string of characters\n", object_type);
        break;

      case 0x03:
        fprintf (fo, "      Object Type (2 bits): 0x%01x Reserved\n", object_type);
        break;

      default:
        fprintf (stderr, "Unknown Object Type (2 bits) 0x%01x in parse_rcs().\n", object_type);
        exit (EXIT_FAILURE);

    }  // End switch object_type


    // Object Provider Flag (2 bits)
    object_provider_flag = (segment[pid].buffer[*offset] >> 4) & 0x03;  // 0x03 = 11
    switch (object_provider_flag) {

      case 0x00:
        fprintf (fo, "      Object Provider Flag (2 bits): 0x%01x Provided in the subtitling stream\n", object_provider_flag);
        break;

      case 0x01:
        fprintf (fo, "      Object Provider Flag (2 bits): 0x%01x Provided by a ROM in the Integrated Receiver Decoder (IRD)\n", object_provider_flag);
        break;

      case 0x02:
        fprintf (fo, "      Object Provider Flag (2 bits): 0x%01x Reserved\n", object_provider_flag);
        break;

      case 0x03:
        fprintf (fo, "      Object Provider Flag (2 bits): 0x%01x Reserved\n", object_provider_flag);
        break;

      default:
        fprintf (stdout, "Unknown Object Provider Flag (2 bits) 0x%01x in parse_rcs().\n", object_provider_flag);
        exit (EXIT_FAILURE);

    }  // End switch object_provider_flag

    // Object Horizontal Position Within Region (12 bits)
    object_horizontal_position = ((segment[pid].buffer[*offset] & 0x0f) << 8) |
                                   segment[pid].buffer[(*offset) + 1];
    object_pos[nobjects].horizontal_position = object_horizontal_position;
    fprintf (fo, "      Object Horizontal Position (12 bits): %u px from left of region 0x%02x\n", object_horizontal_position, region_id);
    (*offset) += 2;
    consumed += 2;

    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
      exit (EXIT_FAILURE);
    }

    // Reserved (4 bits)

    // Object Vertical Position Within Region (12 bits)
    object_vertical_position = ((segment[pid].buffer[*offset] & 0x0f) << 8) |
                                   segment[pid].buffer[(*offset) + 1];
    object_pos[nobjects].vertical_position = object_vertical_position;
    fprintf (fo, "      Object Vertical Position (12 bits): %u px from top of region 0x%02x\n", object_vertical_position, region_id);
    (*offset) += 2;
    consumed += 2;

    // Foreground and Background Pixel Codes (for Basic Character Objects, or Composite Character String Objects)
    if ((object_type == 0x01) || (object_type == 0x02)) {

      // Foreground Pixel Code (1 byte)
      // Specifies the entry of the 8-bit CLUT used for foreground color. For 4 or 16-entry CLUTs, use reduction schemes (ETSI EN 300 743).
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
        exit (EXIT_FAILURE);
      }
      foreground_pixel_code = segment[pid].buffer[*offset];
      object_pos[nobjects].foreground_color = foreground_pixel_code;
      fprintf (fo, "      Foreground Pixel Code (1 byte): 0x%02x (entry selected for foreground character(s) in 8-bit CLUT)\n", foreground_pixel_code);
      (*offset)++;
      consumed++;

      // Background Pixel Code (1 byte)
      // Specifies the entry of the 8-bit CLUT used for background color. For 4 or 16-entry CLUTs, use reduction schemes (ETSI EN 300 743).
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_rcs().\n");
        exit (EXIT_FAILURE);
      }
      background_pixel_code = segment[pid].buffer[*offset];
      object_pos[nobjects].background_color = background_pixel_code;
      fprintf (fo, "      Background Pixel Code (1 byte): 0x%02x (entry selected for foreground character(s) in 8-bit CLUT)\n", background_pixel_code);
      (*offset)++;
      consumed++;
    }

    // Increment count of objects for this region.
    nobjects++;

  }  // End while

  // Populate list of object positions.
  // Note that REGION struct has element OBJECT_POS object_pos[MAX_OBJECTS];
  (*page)[page_idx].region[region_idx].nobjects = nobjects;
  for (i = 0; i < nobjects; i++) {
    (*page)[page_idx].region[region_idx].object_pos[i].object_id = object_pos[i].object_id;
    (*page)[page_idx].region[region_idx].object_pos[i].object_type = object_pos[i].object_type;
    (*page)[page_idx].region[region_idx].object_pos[i].horizontal_position = object_pos[i].horizontal_position;
    (*page)[page_idx].region[region_idx].object_pos[i].vertical_position = object_pos[i].vertical_position;
    (*page)[page_idx].region[region_idx].object_pos[i].foreground_color = object_pos[i].foreground_color;
    (*page)[page_idx].region[region_idx].object_pos[i].background_color = object_pos[i].background_color;
  }

  return (EXIT_SUCCESS);
}
