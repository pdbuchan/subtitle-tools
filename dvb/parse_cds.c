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

// CLUT Definition Segment (CDS)
// Reference: ETSI EN 300 743
int
parse_cds (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, FILE *fo) {

  int temp, rgb[3];
  size_t consumed, segment_length, old_size, page_idx, clut_idx;
  uint8_t sync_byte, segment_type, clut_id, clut_version_number, clut_entry_id, full_range_flag, y, cb, cr, t, f2, f4, f8;
  uint16_t pid, page_id;
  void *tmp;

  pid = state->pid;

  fprintf (fo, "\n  CLUT Definition Segment (CDS)\n");

  // Sync Byte (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
    exit (EXIT_FAILURE);
  }
  sync_byte = segment[pid].buffer[*offset];
  if (sync_byte != 0x0f) {
    fprintf (stderr, "Sync byte not found in parse_cds().\n");
    fprintf (stderr, "Found: 0x%02x\n", sync_byte);
    exit (EXIT_FAILURE);
  }         
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", sync_byte);
  (*offset)++;

  // Segment Type (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
    exit (EXIT_FAILURE);
  }
  segment_type = segment[pid].buffer[*offset];
  if (segment_type != 0x12) {
    fprintf (stderr, "Wrong Segment Type found in parse_cds().\n");
    fprintf (stderr, "Found: 0x%02x\n", segment_type);
    exit (EXIT_FAILURE);
  }
  segment_types (state, segment_type, fo);
  (*offset)++;

  // Page ID (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
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
    fprintf (stderr, "Cannot find index for page_id: 0x%04x in parse_cds().\n", page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  // Segment Length (2 bytes)
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
    exit (EXIT_FAILURE);
  }
  segment_length = (size_t) ((segment[pid].buffer[*offset] << 8) |
            segment[pid].buffer[(*offset) + 1]);
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", segment_length);
  (*offset) += 2;
  consumed = 0;

  // CLUT ID (1 byte)
  // One clut_id identifies a CLUT family, which is composed of: 2-bit, 4-bit, and 8-bit CLUTs representing the same palette.
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
    exit (EXIT_FAILURE);
  }
  clut_id = segment[pid].buffer[*offset];
  fprintf (fo, "    CLUT ID (1 byte): 0x%02x\n", clut_id);
  (*offset)++;
  consumed++;

  // Allocate memory for this clut_id within page_id if not already available.
  temp = find_clut_index (state, *page, clut_id);
  if (temp < 0) {
    old_size = (*page)[page_idx].ncluts;
    clut_idx = (*page)[page_idx].ncluts;  // Note it's a 0-based array.
    tmp = (CLUT_FAMILY *) realloc ((*page)[page_idx].clut, (old_size + 1) * sizeof (CLUT_FAMILY));
    if (tmp != NULL) {
      (*page)[page_idx].clut = tmp;
    } else {
      fprintf (stderr, "Cannot allocate memory for page[%zu].clut[%zu] in parse_cds().\n", page_idx, clut_idx);
      fprintf (stderr, "page_id: 0x%04x, clut_id: 0x%02x\n", page_id, clut_id);
      exit (EXIT_FAILURE);
    }
    memset (&(*page)[page_idx].clut[old_size], 0, sizeof (CLUT_FAMILY));  // Clear only new elements.
    (*page)[page_idx].clut[clut_idx].clut_id = clut_id;
    (*page)[page_idx].ncluts++;

    // Initialize CLUT family clut_id with default CLUT contents.
    // Incoming CLUT Definition Sections can overwrite none, some, or all of the default CLUT entries.
    initialize_clut_family (state, *page, clut_idx);

  // CLUT already has memory allocated for it; keep index.
  // Incoming CLUT Definition Sections can overwrite none, some, or all of the default or existing CLUT entries.
  } else {
    clut_idx = (size_t) temp;
  }

  // CLUT Family Version Number (4 bits)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
    exit (EXIT_FAILURE);
  }
  clut_version_number = (segment[pid].buffer[*offset] >> 4) & 0x0f;  // 0x0f = 1111
  (*page)[page_idx].clut[clut_idx].version = clut_version_number;
  fprintf (fo, "    CLUT Version Number (4 bits): 0x%01x\n", clut_version_number);

  // Reserved (4 bits)

  (*offset)++;
  consumed++;

  // CLUT Entry Loop
  while ((consumed < segment_length) && ((*offset) < segment[pid].length)) {

    // CLUT Entry ID (1 byte)
    if ((*offset) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
      exit (EXIT_FAILURE);
    }
    clut_entry_id = segment[pid].buffer[*offset];
    fprintf (fo, "      CLUT Entry ID (1 byte): 0x%02x\n", clut_entry_id);
    (*offset)++;
    consumed++;

    if ((*offset) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
      exit (EXIT_FAILURE);
    }
    // 2-bit/entry CLUT Flag (1 bit)
    f2 = (segment[pid].buffer[*offset] >> 7) & 1;
    fprintf (fo, "        2-bit/entry CLUT Flag (1 bit): %u\n", f2);

    // 4-bit/entry CLUT Flag (1 bit)
    f4 = (segment[pid].buffer[*offset] >> 6) & 1;
    fprintf (fo, "        4-bit/entry CLUT Flag (1 bit): %u\n", f4);

    // 8-bit/entry CLUT Flag (1 bit)
    f8 = (segment[pid].buffer[*offset] >> 5) & 1;
    fprintf (fo, "        8-bit/entry CLUT Flag (1 bit): %u\n", f8);

    // Reserved (4 bits)

    // Full Range Flag (1 bit)
    full_range_flag = segment[pid].buffer[*offset] & 1;
    fprintf (fo, "        Full Range Flag (1 bit): %u\n", full_range_flag);

    (*offset)++;
    consumed++;

    // Full Range Color and Transparency
    if (full_range_flag) {

      // Y (1 byte)
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
        exit (EXIT_FAILURE);
      }
      y = segment[pid].buffer[*offset];
      fprintf (fo, "        Y (1 byte) 0x%02x\n", y);
      (*offset)++;
      consumed++;

      // Cr (1 byte)
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
        exit (EXIT_FAILURE);
      }
      cr = segment[pid].buffer[*offset];
      fprintf (fo, "        Cr (1 byte): 0x%02x\n", cr);
      (*offset)++;
      consumed++;

      // Cb (1 byte)
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
        exit (EXIT_FAILURE);
      }
      cb = segment[pid].buffer[*offset];
      fprintf (fo, "        Cb (1 byte): 0x%02x\n", cb);
      (*offset)++;
      consumed++;

      // Transparency (1 byte): 0 = opaque
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
        exit (EXIT_FAILURE);
      }
      t = segment[pid].buffer[*offset];
      fprintf (fo, "        T (1 byte): 0x%02x\n", t);
      (*offset)++;
      consumed++;

    } else {

      if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_cds().\n");
        exit (EXIT_FAILURE);
      }

      // Y (6 bits)
      y = (segment[pid].buffer[*offset] >> 2) & 0x3f;  // 0x3f = 0011 1111
      fprintf (fo, "        Y (6 bits) 0x%02x\n", y);

      // Cr (4 bits)
      cr = ((segment[pid].buffer[*offset] & 0x03) << 2) |  // 0x03 = 0000 0011
                                         ((segment[pid].buffer[(*offset) + 1] >> 6) & 0x03);
      fprintf (fo, "        Cr (4 bits): 0x%02x\n", cr);

      // Cb (4 bits)
      cb = (segment[pid].buffer[(*offset) + 1] >> 2) & 0x0f;  // 0x0f = 0000 1111
      fprintf (fo, "        Cb (4 bits): 0x%02x\n", cb);

      // Transparency (2 bits)
      t = segment[pid].buffer[(*offset) + 1] & 0x03;
      fprintf (fo, "        T (2 bits): 0x%02x\n", t);

      (*offset) += 2;
      consumed += 2;

    }  // End if full_range_flag

    // 2-bit/entry CLUT
    if (f2) {

      if (y != 0) {
        YCbCr2RGB_bt601 (full_range_flag, (int) y, (int) cb, (int) cr, rgb);  // Convert YCbCr to 8-bit sRGB.
        (*page)[page_idx].clut[clut_idx].clut2[clut_entry_id & 0x03].r = (uint8_t) rgb[0];
        (*page)[page_idx].clut[clut_idx].clut2[clut_entry_id & 0x03].g = (uint8_t) rgb[1];
        (*page)[page_idx].clut[clut_idx].clut2[clut_entry_id & 0x03].b = (uint8_t) rgb[2];
        (*page)[page_idx].clut[clut_idx].clut2[clut_entry_id & 0x03].a = (uint8_t) (255 * (3 - t) / 3);  // Convert from transparency t to alpha a.
      } else {
        (*page)[page_idx].clut[clut_idx].clut2[clut_entry_id & 0x03].r = 0;  // R
        (*page)[page_idx].clut[clut_idx].clut2[clut_entry_id & 0x03].g = 0;  // G
        (*page)[page_idx].clut[clut_idx].clut2[clut_entry_id & 0x03].b = 0;  // B
        (*page)[page_idx].clut[clut_idx].clut2[clut_entry_id & 0x03].a = 0;  // Alpha (fully transparent)
      }
      (*page)[page_idx].clut[clut_idx].state2 = 'c';  // Mark this CLUT as customized.
    }

    // 4-bit/entry CLUT
    if (f4) {

      if (y != 0) {
        YCbCr2RGB_bt601 (full_range_flag, (int) y, (int) cb, (int) cr, rgb);  // Convert YCbCr to 8-bit sRGB.
        (*page)[page_idx].clut[clut_idx].clut4[clut_entry_id & 0x0f].r = (uint8_t) rgb[0];
        (*page)[page_idx].clut[clut_idx].clut4[clut_entry_id & 0x0f].g = (uint8_t) rgb[1];
        (*page)[page_idx].clut[clut_idx].clut4[clut_entry_id & 0x0f].b = (uint8_t) rgb[2];
        (*page)[page_idx].clut[clut_idx].clut4[clut_entry_id & 0x0f].a = (uint8_t) (255 * (3 - t) / 3);  // Convert from transparency t to alpha a.
      } else {
        (*page)[page_idx].clut[clut_idx].clut4[clut_entry_id & 0x0f].r = 0;  // R
        (*page)[page_idx].clut[clut_idx].clut4[clut_entry_id & 0x0f].g = 0;  // G
        (*page)[page_idx].clut[clut_idx].clut4[clut_entry_id & 0x0f].b = 0;  // B
        (*page)[page_idx].clut[clut_idx].clut4[clut_entry_id & 0x0f].a = 0;  // Alpha (fully transparent)
      }
      (*page)[page_idx].clut[clut_idx].state4 = 'c';  // Mark this CLUT as customized.
    }

    // 8-bit/entry CLUT Flag (1 bit)
    if (f8) {

      if (y != 0) {
        YCbCr2RGB_bt601 (full_range_flag, (int) y, (int) cb, (int) cr, rgb);  // Convert YCbCr to 8-bit sRGB.
        (*page)[page_idx].clut[clut_idx].clut8[clut_entry_id].r = (uint8_t) rgb[0];
        (*page)[page_idx].clut[clut_idx].clut8[clut_entry_id].g = (uint8_t) rgb[1];
        (*page)[page_idx].clut[clut_idx].clut8[clut_entry_id].b = (uint8_t) rgb[2];
        (*page)[page_idx].clut[clut_idx].clut8[clut_entry_id].a = (uint8_t) (255 - t);  // Convert from transparency t to alpha a.
      } else {
        (*page)[page_idx].clut[clut_idx].clut8[clut_entry_id].r = 0;  // R
        (*page)[page_idx].clut[clut_idx].clut8[clut_entry_id].g = 0;  // G
        (*page)[page_idx].clut[clut_idx].clut8[clut_entry_id].b = 0;  // B
        (*page)[page_idx].clut[clut_idx].clut8[clut_entry_id].a = 0;  // Alpha (fully transparent)
      }
      (*page)[page_idx].clut[clut_idx].state8 = 'c';  // Mark this CLUT as customized.
    }

  }  // End while Entry loop

  return (EXIT_SUCCESS);
}
