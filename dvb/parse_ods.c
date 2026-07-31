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

// Object Definition Segment (ODS)
// Reference: ETSI EN 300 743
int
parse_ods (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, FILE *fo) {

  int temp;
  size_t i, segment_length, top_field_data_block_length, bottom_field_data_block_length, field, bitpos, field_end_bitpos, field0_start, field1_start, field_start_byte, x, y, field_line, field_length, number_of_codes, old_size, page_idx, object_idx;
  uint8_t sync_byte, segment_type, object_version_number, object_coding_method, non_modifying_colour_flag, data_type, stuffing, character_horizontal_position, character_vertical_position;
  uint16_t pid, page_id, object_id, character_code;
  RLE rle;
  void *tmp;

  pid = state->pid;

  fprintf (fo, "\n  Object Definition Segment (ODS)\n");
      
  // Sync Byte (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
  sync_byte = segment[pid].buffer[*offset]; 
  if (sync_byte != 0x0f) {
    fprintf (stderr, "Sync byte not found in parse_ods().\n");
    fprintf (stderr, "Found: 0x%02x\n", sync_byte);
    exit (EXIT_FAILURE);
  }         
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", sync_byte);
  (*offset)++;
      
  // Segment Type (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
  segment_type = segment[pid].buffer[*offset];
  if (segment_type != 0x13) {
    fprintf (stderr, "Wrong Segment Type found in parse_ods().\n");
    fprintf (stderr, "Found: 0x%02x\n", segment_type);
    exit (EXIT_FAILURE);
  }
  segment_types (state, segment_type, fo);
  (*offset)++;
      
  // Page ID (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
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
    fprintf (stderr, "Cannot find index for page_id: 0x%04x in parse_ods().\n", page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  // Segment Length (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
  segment_length = (size_t) ((segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1]);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", segment_length);
  (*offset) += 2;

  // Object ID (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
  object_id = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
  state->object_id = object_id;
  fprintf (fo, "    Object ID (2 bytes): 0x%04x\n", object_id);
  (*offset) += 2;

  // Find Object index from object_id.
  temp = find_object_index (state, *page, object_id);
  if (temp < 0) {
    old_size = (*page)[page_idx].nobjects;
    object_idx = (*page)[page_idx].nobjects;  // Note it's a 0-based array.
    tmp = (OBJECT *) realloc ((*page)[page_idx].object, (old_size + 1) * sizeof (OBJECT));
    if (tmp != NULL) {
      (*page)[page_idx].object = tmp;
    } else {
      fprintf (stderr, "Cannot allocate memory for page[%zu].object[%zu] in parse_ods().\n", page_idx, object_idx);
      fprintf (stderr, "page_id: 0x%04x, object_id: 0x%04x\n", page_id, object_id);
      exit (EXIT_FAILURE);
    }
    memset (&(*page)[page_idx].object[old_size], 0, sizeof (OBJECT));  // Clear only new elements.
    (*page)[page_idx].object[object_idx].object_id = object_id;
    (*page)[page_idx].object[object_idx].buffer = allocate_u8mem (IMG_BUFFER_SIZE);
    (*page)[page_idx].nobjects++;

  // Object already has memory allocated for it. Clear it for new data.
  } else {
    object_idx = (uint16_t) temp;
    (*page)[page_idx].object[object_idx].width = 0;
    (*page)[page_idx].object[object_idx].height = 0;
    memset ((*page)[page_idx].object[object_idx].buffer, 0, IMG_BUFFER_SIZE * sizeof (uint8_t));
  }

  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }

  // Object Version Number (4 bits)
  object_version_number = (segment[pid].buffer[*offset] >> 4) & 0x0f;  // 0x0f = 0000 1111
  (*page)[page_idx].object[object_idx].version = object_version_number;
  fprintf (fo, "    Object Version Number (4 bits): 0x%1x\n", object_version_number);

  // Object Coding Method (2 bits)
  object_coding_method = (segment[pid].buffer[*offset] >> 2) & 3;
  fprintf (fo, "    Object Coding Method (2 bits): %u ", object_coding_method);
  switch (object_coding_method) {

    case 0:
      fprintf (fo, "Coding of pixels\n");
      break;

    case 1:
      fprintf (fo, "Coded as a string of characters\n");
      break;

    default:
      fprintf (stderr, "Object Coding Method %u is Reserved\n", object_coding_method);
      exit (EXIT_FAILURE);

  }  // End switch

  // Non-Modifying Colour Flag (1 bit)
  non_modifying_colour_flag = (segment[pid].buffer[*offset] >> 1) & 1;
  (*page)[page_idx].object[object_idx].non_modifying_colour_flag = non_modifying_colour_flag;
  fprintf (fo, "    Non-Modifying Colour Flag (1 bit): %u\n", non_modifying_colour_flag);
  if (non_modifying_colour_flag) {
    fprintf (fo, " CLUT entry value of 1 is a non-modifying colour; background pixel shall not be modified\n");
  }

  // Reserved (1 bit)

  (*offset)++;

  // Coding of pixels
  if (object_coding_method == 0) {

    // Top Field Data Block Length (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
      exit (EXIT_FAILURE);
    }
    top_field_data_block_length = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
    fprintf (fo, "    Top Field Data Block Length (2 bytes): %zu bytes\n", top_field_data_block_length);
    (*offset) += 2;

    // Bottom Field Data Block Length (2 bytes)
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
      exit (EXIT_FAILURE);
    }
    bottom_field_data_block_length = (segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1];
    fprintf (fo, "    Bottom Field Data Block Length (2 bytes): %zu bytes\n", bottom_field_data_block_length);
    (*offset) += 2;

    // Subpicture Data
    // Subpicture data is interlaced: Top Field are lines 0, 2, 4, etc. Bottom Field are line 1, 3, 5, etc.
    // Line 0 is at top of screen.
    (*page)[page_idx].object[object_idx].width = 0;
    (*page)[page_idx].object[object_idx].height = 0;
    field0_start = *offset;
    field1_start = field0_start + top_field_data_block_length;

    // Process both fields: 0 = top, 1 = bottom
    for (field = 0; field < 2; field++) {

      // ETSI EN 300 743: If bottom field length is 0, then the data for the top field
      // is valid for the bottom field as well.
      if (bottom_field_data_block_length == 0) {
        bottom_field_data_block_length = top_field_data_block_length;
        field1_start = field0_start;
      }

      field_start_byte = (field == 0) ? field0_start : field1_start;
      field_length     = (field == 0) ? top_field_data_block_length
                                      : bottom_field_data_block_length;

      bitpos = field_start_byte * 8;
      field_end_bitpos = bitpos + (field_length * 8);

      x = 0;                 // Horizontal pixel position
      field_line = 0;        // Line index within this field
      y = field;             // Actual screen line (0 or 1 start)

      while (bitpos < field_end_bitpos) {

        // Check if 8 bits are available for data_type.
        if ((bitpos + 8) > field_end_bitpos) break;

        // Read next data_type.
        get_8bits (state, segment, &bitpos, &data_type);
        bitpos += 8;

        rle.end_of_string_signal = 0;

        switch (data_type) {

          case 0x10:  // 2-bit/pixel code string
            while (!rle.end_of_string_signal) {
              parse_two_bit_code_string (state, segment, &bitpos, &rle);
              emit_pixels (state, page, &rle, &x, y);
            }

            // Align_to_next_byte.
            bitpos = (bitpos + 7) & ~7;
            break;

          case 0x11:  // 4-bit/pixel code string
            while (!rle.end_of_string_signal) {
              parse_four_bit_code_string (state, segment, &bitpos, &rle);
              emit_pixels (state, page, &rle, &x, y);
            }

            // Align_to_next_byte.
            bitpos = (bitpos + 7) & ~7;
            break;

          case 0x12:  // 8-bit/pixel code string
            while (!rle.end_of_string_signal) {
              parse_eight_bit_code_string (state, segment, &bitpos, &rle);
              emit_pixels (state, page, &rle, &x, y);
            }
            break;

          case 0xf0:  // End of line

            // Update maximum width.
            if ((*page)[page_idx].object[object_idx].width < x) {
              (*page)[page_idx].object[object_idx].width = x;
            }

            // Track total height as maximum y reached.
            if ((*page)[page_idx].object[object_idx].height <= y) {
              (*page)[page_idx].object[object_idx].height = y + 1;  // Add 1 since y starts at 0.
            }

            x = 0;

            // Move to next interlaced line.
            field_line++;
            y = field + (field_line * 2);

            break;

          default:  // Keep going; this is poor ODS structure, but allowed.

            fprintf (stderr, "Unknown data_type 0x%02x in parse_ods(). This is poor ODS structure but allowed. Continuing...\n", data_type);

            // Update maximum width.
            if ((*page)[page_idx].object[object_idx].width < x) {
              (*page)[page_idx].object[object_idx].width = x;
            }

            // Track total height as maximum y reached.
            if ((*page)[page_idx].object[object_idx].height <= y) {
              (*page)[page_idx].object[object_idx].height = y + 1;  // Add 1 since y starts at 0.
            }

            fprintf (fo, "    Object Width: %zu px\n", (*page)[page_idx].object[object_idx].width);
            fprintf (fo, "    Object Height: %zu px\n", (*page)[page_idx].object[object_idx].height);

            return (EXIT_SUCCESS);
        }  // End switch
      }  // End while
    }  // End for field

    // Update offset in bytes.
    (*offset) = field0_start + top_field_data_block_length + bottom_field_data_block_length;

    // Ensure 16-bit word alignment if segment_length is odd.
    if ((*offset) % 2 != 0) {
      stuffing = segment[pid].buffer[*offset];
      if (stuffing != 0x00) {
        fprintf (stderr, "Invalid ODS stuffing byte (0x%02x) in parse_ods().\n", stuffing);
        exit (EXIT_FAILURE);
      }
      (*offset)++;  // Consume the padding byte.
    }

  // Coded as a string of characters.
  } else if (object_coding_method == 1) {

    // Number of Codes (1 byte)
    number_of_codes = segment[pid].buffer[*offset];
    fprintf (fo, "    Number of Codes (1 byte): %zu\n", number_of_codes);
    (*offset)++;

    for (i = 0; i < number_of_codes; i++) {

      // Character code (2 bytes)
      character_code = (segment[pid].buffer[*offset] << 8) |
                        segment[pid].buffer[*offset + 1];
      (*offset) += 2;

      // Horizontal position (1 byte)
      character_horizontal_position = segment[pid].buffer[*offset];
      (*offset)++;

      // Vertical position (1 byte)
      character_vertical_position = segment[pid].buffer[*offset];
      (*offset)++;

      fprintf (fo, "    Character %zu: code=0x%04x x=%u y=%u\n", i + 1, character_code, character_horizontal_position, character_vertical_position);

      /*
         Typical implementations store these glyphs and render
         them later with a font. Since DVB subtitle streams almost
         never use this coding mode, a minimal decoder can simply
         ignore them safely.
      */
    }

  }  // End if object_coding_method

  // Report object dimensions.
  fprintf (fo, "    Object Width: %zu px\n", (*page)[page_idx].object[object_idx].width);
  fprintf (fo, "    Object Height: %zu px\n", (*page)[page_idx].object[object_idx].height);

  return (EXIT_SUCCESS);
}
