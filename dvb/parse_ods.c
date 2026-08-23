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

// Return the number of pixels represented by the current RLE command. Two
// special commands emit one or two pixels of CLUT entry 0 without storing the
// count in runlength.
static size_t
rle_count (const RLE *r) {

  if (r->emit_one_00_pixel) return (1);
  if (r->emit_two_00_pixels) return (2);

  return (r->runlength);
}

// The 2-bit and 4-bit pixel-code strings are byte-aligned after their
// end-of-string signal. Advance to the next byte without crossing the end of
// the current top- or bottom-field data block.
static int
align_byte (size_t *p, size_t lim) {

  size_t rem = *p % 8, add;

  if (!rem) return (EXIT_SUCCESS);
  add = 8 - rem;
  if (*p > lim || add > lim - *p) return (EXIT_FAILURE);
  *p += add;

  return (EXIT_SUCCESS);
}

// Decode one interlaced ODS field.
//
// field = 0 decodes the top field onto image lines 0, 2, 4, ...
// field = 1 decodes the bottom field onto image lines 1, 3, 5, ...
//
// The function is deliberately called twice for each field. During the first
// pass write == 0, and only the maximum width and height are measured. During
// the second pass write != 0, and pixels are stored using that final fixed
// width as the row stride. This is necessary because ETSI EN 300 743 permits
// a ragged right edge: individual code strings may end at different x
// positions even though the object has one overall maximum width.
static int
decode_field (STATE *s, PAGE **page, size_t pi, size_t oi, SEGMENT *seg, size_t start, size_t len, size_t field, int write, FILE *fo) {

  OBJECT *obj = &(*page)[pi].object[oi];
  RLE r;
  size_t bp, lim, x = 0, y = field, line = 0, n;
  uint8_t type, dummy;
  unsigned int i, count;

  // Convert the field's byte range to a bit range, checking every arithmetic
  // operation before it is used.
  if (!bytes_available (start, len, seg[s->pid].length) ||
      start > SIZE_MAX / 8 || len > SIZE_MAX / 8 ||
      start * 8 > SIZE_MAX - len * 8)
    return (EXIT_FAILURE);
  bp = start * 8;
  lim = bp + len * 8;

  // Each sub-block begins with an 8-bit data_type.
  while (bp < lim) {
    if (get_bits (s, seg, &bp, lim, 8, &type)) return (EXIT_FAILURE);
    switch (type) {

      // 2-, 4-, and 8-bit/pixel code strings.
      case 0x10:
      case 0x11:
      case 0x12:
        memset (&r, 0, sizeof (r));
        while (!r.end_of_string_signal) {
          int rc = (type == 0x10) ?
            parse_two_bit_code_string (s, seg, &bp, lim, &r) :
            (type == 0x11) ?
            parse_four_bit_code_string (s, seg, &bp, lim, &r) :
            parse_eight_bit_code_string (s, seg, &bp, lim, &r);
          if (rc) return (EXIT_FAILURE);
          if (write) {
            if (emit_pixels (page, pi, oi, &r, &x, y)) return (EXIT_FAILURE);
          }
          else {
            n = rle_count (&r);
            if (n > SIZE_MAX - x) return (EXIT_FAILURE);
            x += n;
          }
        }

        // 2- and 4-bit strings are padded to a byte boundary. The 8-bit
        // string is already byte-aligned by construction.
        if (type != 0x12 && align_byte (&bp, lim)) return (EXIT_FAILURE);
        break;

      // Pixel-code mapping tables. The present renderer does not yet retain
      // per-object mapping tables, so these bytes are consumed to keep the ODS
      // parser synchronized. Streams which rely on non-default 2-to-4,
      // 2-to-8, or 4-to-8 mapping tables therefore remain a limitation.
      case 0x20:
        count = 2;
        for (i = 0; i < count; i++) if (get_bits (s, seg, &bp, lim, 8, &dummy)) return (EXIT_FAILURE);
        break;

      case 0x21:
        count = 4;
        for (i = 0; i < count; i++) if (get_bits (s, seg, &bp, lim, 8, &dummy)) return (EXIT_FAILURE);
        break;

      case 0x22:
        count = 16;
        for (i = 0; i < count; i++) if (get_bits (s, seg, &bp, lim, 8, &dummy)) return (EXIT_FAILURE);
        break;

      // End-of-line code. On the measuring pass, update the object's maximum
      // dimensions; on the writing pass, merely verify the measured width.
      case 0xf0:
        if (!write) {
          if (x > obj->width) obj->width = x;
          if (y >= obj->height) obj->height = y + 1;
        }
        else if (x > obj->width) return (EXIT_FAILURE);

        x = 0;
        line++;
        if (line > (SIZE_MAX - field) / 2) return (EXIT_FAILURE);
        y = field + line * 2;
        break;

      default:
        fprintf (fo, "    Reserved ODS data_type: 0x%02x\n", type);
        break;
    }
  }

  // A field is expected to finish immediately after an end-of-line code.
  if (x != 0) {
    fprintf (stderr, "ODS field ended before an end-of-line code.\n");
    return (EXIT_FAILURE);
  }
  return (EXIT_SUCCESS);
}

// Object Definition Segment (ODS)
// Reference: ETSI EN 300 743
int
parse_ods (STATE *s, PAGE **page, size_t *off, SEGMENT *seg, FILE *fo) {

  int t;
  size_t body, end, len, top, bottom, top_start, bottom_start, bstart, blen, data, stuff, old, pi, oi, i, ncodes;
  uint8_t sync, type, ver, method, nm, pad;
  uint16_t pid = s->pid, page_id, obj_id, cc;
  OBJECT *obj;
  void *tmp;

  fprintf (fo, "\n  Object Definition Segment (ODS)\n");

  // Segment header: Sync Byte, Segment Type, Page ID, Segment Length.
  if (!bytes_available (*off, 6, seg[pid].length)) return (EXIT_FAILURE);

  // Sync Byte (1 byte)
  sync = seg[pid].buffer[(*off)++];
  if (sync != 0x0f) return (EXIT_FAILURE);
  fprintf (fo, "    Sync Byte (1 byte): 0x%02x\n", sync);

  // Segment Type (1 byte)
  type = seg[pid].buffer[(*off)++];
  if (type != 0x13) return (EXIT_FAILURE);
  segment_types (s, type, fo);

  // Page ID (2 bytes)
  page_id = (uint16_t) (((uint16_t) seg[pid].buffer[*off] << 8) | seg[pid].buffer[*off + 1]);
  *off += 2;
  s->page_id = page_id;
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n", page_id);

  // Segment Length (2 bytes)
  len = (size_t) (((uint16_t) seg[pid].buffer[*off] << 8) | seg[pid].buffer[*off + 1]);
  *off += 2;
  body = *off;
  fprintf (fo, "    Segment Length (2 bytes): %zu bytes\n", len);
  if (!bytes_available (body, len, seg[pid].length) || len < 3) return (EXIT_FAILURE);
  end = body + len;

  // Obtain the compact Page array index from page_id.
  t = find_page_index (s, *page, page_id);
  if (t < 0) return (EXIT_FAILURE);
  pi = (size_t) t;

  // Object ID (2 bytes)
  obj_id = (uint16_t) (((uint16_t) seg[pid].buffer[*off] << 8) | seg[pid].buffer[*off + 1]);
  *off += 2;
  s->object_id = obj_id;
  fprintf (fo, "    Object ID (2 bytes): 0x%04x\n", obj_id);

  // Find the Object array index from object_id. If this is a new object,
  // allocate both its CLUT-entry image buffer and its coded-pixel mask.
  t = find_object_index (*page, pi, obj_id);
  if (t < 0) {
    old = (*page)[pi].nobjects;
    oi = old;
    tmp = realloc ((*page)[pi].object, (old + 1) * sizeof (OBJECT));
    if (!tmp) return (EXIT_FAILURE);
    (*page)[pi].object = tmp;
    memset (&(*page)[pi].object[oi], 0, sizeof (OBJECT));
    obj = &(*page)[pi].object[oi];
    obj->page_id = page_id;
    obj->object_id = obj_id;
    obj->buffer = allocate_u8mem (IMG_PIXEL_COUNT);
    obj->coded = allocate_u8mem (IMG_PIXEL_COUNT);
    (*page)[pi].nobjects++;
  }

  // Object already has memory allocated for it. Clear it for the new version.
  else {
    oi = (size_t) t;
    obj = &(*page)[pi].object[oi];
    obj->width = obj->height = 0;
    memset (obj->buffer, 0, IMG_PIXEL_COUNT);
    memset (obj->coded, 0, IMG_PIXEL_COUNT);
  }

  // Object Version Number (4 bits)
  ver = (seg[pid].buffer[*off] >> 4) & 0x0f;

  // Object Coding Method (2 bits)
  method = (seg[pid].buffer[*off] >> 2) & 3;

  // Non-Modifying Colour Flag (1 bit)
  nm = (seg[pid].buffer[*off] >> 1) & 1;
  obj->version = ver;
  obj->non_modifying_colour_flag = nm;
  (*off)++;
  fprintf (fo, "    Object Version Number (4 bits): 0x%01x\n", ver);
  fprintf (fo, "    Object Coding Method (2 bits): %u\n", method);
  fprintf (fo, "    Non-Modifying Colour Flag (1 bit): %u\n", nm);

  // Coding method 0: pixel data represented by interlaced top and bottom
  // fields of RLE code strings.
  if (method == 0) {
    if (len < 7 || !bytes_available (*off, 4, end)) return (EXIT_FAILURE);

    // Top Field Data Block Length (2 bytes)
    top = (size_t) (((uint16_t) seg[pid].buffer[*off] << 8) | seg[pid].buffer[*off + 1]);
    *off += 2;

    // Bottom Field Data Block Length (2 bytes)
    bottom = (size_t) (((uint16_t) seg[pid].buffer[*off] << 8) | seg[pid].buffer[*off + 1]);
    *off += 2;
    fprintf (fo, "    Top Field Data Block Length (2 bytes): %zu bytes\n", top);
    fprintf (fo, "    Bottom Field Data Block Length (2 bytes): %zu bytes\n", bottom);
    if (top > SIZE_MAX - bottom) return (EXIT_FAILURE);
    data = top + bottom;
    if (data > len - 7) return (EXIT_FAILURE);
    stuff = len - 7 - data;
    if (stuff > 1) return (EXIT_FAILURE);

    // Subpicture data is interlaced: top-field data describes image lines
    // 0, 2, 4, ... and bottom-field data describes lines 1, 3, 5, ... .
    top_start = *off;
    bottom_start = top_start + top;
    if (!bytes_available (top_start, data + stuff, end)) return (EXIT_FAILURE);

    // ETSI EN 300 743 specifies that a zero bottom-field length means the top
    // field data is valid for the bottom field as well. Do not change the
    // transmitted bottom length: it is still needed to advance *off correctly.
    if (bottom == 0) {
      bstart = top_start;
      blen = top;
    } else {
      bstart = bottom_start;
      blen = bottom;
    }

    // First pass: measure the object. Because individual scan lines may have
    // different coded widths, this pass finds the maximum x position before a
    // fixed row stride is chosen.
    obj->width = obj->height = 0;
    if (decode_field (s, page, pi, oi, seg, top_start, top, 0, 0, fo) || decode_field (s, page, pi, oi, seg, bstart, blen, 1, 0, fo)) return (EXIT_FAILURE);
    if (!obj->width || !obj->height || obj->width > MAX_IMAGE_WIDTH || obj->height > MAX_IMAGE_HEIGHT || obj->width > IMG_PIXEL_COUNT / obj->height) return (EXIT_FAILURE);

    // Second pass: decode again with the final width available as a stable row
    // stride. coded[] records exactly which pixels were supplied by the ODS,
    // preserving the DVB ragged-right-edge "leave unmodified" semantics.
    memset (obj->buffer, 0, IMG_PIXEL_COUNT);
    memset (obj->coded, 0, IMG_PIXEL_COUNT);
    if (decode_field (s, page, pi, oi, seg, top_start, top, 0, 1, fo) || decode_field (s, page, pi, oi, seg, bstart, blen, 1, 1, fo)) return (EXIT_FAILURE);

    // Advance over only the bytes actually present in the ODS. When the top
    // field was reused for the bottom field, bottom remains zero here.
    *off = bottom_start + bottom;

    // At most one stuffing byte may follow the pixel data.
    if (stuff) {
      pad = seg[pid].buffer[(*off)++];
      if (pad != 0) return (EXIT_FAILURE);
    }
  }

  // Coding method 1: string of 16-bit character codes. Character rendering is
  // not implemented here, but parse and report the codes without losing
  // synchronization with the next segment.
  else if (method == 1) {
    if (!bytes_available (*off, 1, end)) return (EXIT_FAILURE);
    ncodes = seg[pid].buffer[(*off)++];
    fprintf (fo, "    Number of Codes (1 byte): %zu\n", ncodes);
    if (ncodes > (end - *off) / 2) return (EXIT_FAILURE);
    for (i = 0; i < ncodes; i++) {
      cc = (uint16_t) (((uint16_t) seg[pid].buffer[*off] << 8) | seg[pid].buffer[*off + 1]);
      *off += 2;
      fprintf (fo, "    Character %zu: code=0x%04x\n", i + 1, cc);
    }
    *off = end;
  }

  // Other object coding methods are reserved or unsupported. The segment
  // length still allows us to skip them safely.
  else {
    fprintf (fo, "    Reserved/unsupported Object Coding Method.\n");
    *off = end;
  }

  if (*off != end) return (EXIT_FAILURE);
  fprintf (fo, "    Object Width: %zu px\n", obj->width);
  fprintf (fo, "    Object Height: %zu px\n", obj->height);

  return (EXIT_SUCCESS);
}
