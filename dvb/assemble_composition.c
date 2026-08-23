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

// Expand the current composition bounds to include the rectangle
// [x0, x1) by [y0, y1). Empty rectangles do not contribute.
static void
bounds (int64_t x0, int64_t y0, int64_t x1, int64_t y1, int64_t *l, int64_t *t, int64_t *r, int64_t *b) {

  if (x1 <= x0 || y1 <= y0) return;
  if (x0 < *l) *l = x0;
  if (y0 < *t) *t = y0;
  if (x1 > *r) *r = x1;
  if (y1 > *b) *b = y1;
}

// Find the CLUT family used by a region. If no CDS has defined this clut_id,
// create a family containing the ETSI default 2-, 4-, and 8-bit CLUTs. A CDS
// may subsequently replace any of those default entries.
static int
ensure_clut (PAGE **page, size_t p, uint8_t id, size_t *ci) {

  int k = find_clut_index (*page, p, id);
  size_t n;
  CLUT_FAMILY *q;

  if (k >= 0) {
    *ci = (size_t) k;
    return (EXIT_SUCCESS);
  }

  n = (*page)[p].ncluts;
  if (n >= 256) return (EXIT_FAILURE);

  q = realloc ((*page)[p].clut, (n + 1) * sizeof (*q));
  if (!q) return (EXIT_FAILURE);

  (*page)[p].clut = q;
  memset (&q[n], 0, sizeof (q[n]));
  q[n].clut_id = id;
  (*page)[p].ncluts++;
  initialize_clut_family (*page, p, n);
  *ci = n;

  return (EXIT_SUCCESS);
}

// Return the background pixel code appropriate for the region's declared
// pixel depth. The RCS carries separate fill codes for 2-, 4-, and 8-bit
// regions.
static uint8_t
bg (const REGION *r) {

  if (r->depth == 1) return (r->pixel_code_2bit);
  if (r->depth == 2) return (r->pixel_code_4bit);
  if (r->depth == 3) return (r->pixel_code_8bit);

  return (0);
}

// Store one RGBA pixel in the linear composition buffer.
static void
put (uint8_t *b, size_t w, size_t x, size_t y, RGBA a) {

  size_t i = (y * w + x) * 4;

  b[i] = a.r;
  b[i + 1] = a.g;
  b[i + 2] = a.b;
  b[i + 3] = a.a;
}

// Assemble a complete DVB subtitle Display Set from its PCS region positions,
// RCS region definitions, ODS objects, and CLUT families, then write the
// resulting composition as a bitmap.
//
// Pixel coordinates increase from left to right and from top to bottom.
// PCS region_pos[] contains only the regions which are to be displayed. RCS
// may define additional regions which are not part of this composition.
int
assemble_composition (STATE *s, PAGE **page, size_t p) {

  int k;
  size_t rp, ri, op, oi, ci, w, h, sx, sy, si, dx, dy, total;
  int64_t l = INT64_MAX, t = INT64_MAX, r = INT64_MIN, b = INT64_MIN, rx, ry, rr, rb, ox, oy, orr, obb, cl, ct, cr, cb, cx, cy;
  uint8_t rid, e;
  uint16_t oid;
  REGION *reg;
  OBJECT *obj;
  RGBA a;
  uint8_t *out;

  if (!page || !*page || p >= s->npages) return (EXIT_FAILURE);

  // First pass: determine the smallest rectangle containing all visible
  // region fills and all visible portions of their objects. Region IDs and
  // object IDs are identifiers, so look up their compact array indexes rather
  // than using the IDs as indexes directly.
  for (rp = 0; rp < (*page)[p].nregion_pos; rp++) {

    rid = (*page)[p].region_pos[rp].region_id;
    k = find_region_index (*page, p, rid);
    if (k < 0) continue;
    ri = (size_t) k;
    reg = &(*page)[p].region[ri];

    // Position of the current region within the page.
    rx = (*page)[p].region_pos[rp].region_horizontal_address;
    ry = (*page)[p].region_pos[rp].region_vertical_address;
    rr = rx + reg->width;
    rb = ry + reg->height;

    // A filled region contributes its complete rectangle even if it contains
    // no objects.
    if (reg->fill_flag) bounds (rx, ry, rr, rb, &l, &t, &r, &b);

    // Add the visible part of each object, clipped to the region rectangle.
    for (op = 0; op < reg->nobjects; op++) {
      oid = reg->object_pos[op].object_id;
      k = find_object_index (*page, p, oid);
      if (k < 0) continue;
      obj = &(*page)[p].object[(size_t) k];
      if (!obj->width || !obj->height) continue;

      // Position of the current object within the page.
      ox = rx + reg->object_pos[op].horizontal_position;
      oy = ry + reg->object_pos[op].vertical_position;
      orr = ox + (int64_t) obj->width;
      obb = oy + (int64_t) obj->height;
      cl = ox > rx ? ox : rx;
      ct = oy > ry ? oy : ry;
      cr = orr < rr ? orr : rr;
      cb = obb < rb ? obb : rb;
      bounds (cl, ct, cr, cb, &l, &t, &r, &b);
    }
  }

  // Nothing visible was found, or the accumulated rectangle is invalid.
  if (l == INT64_MAX || r <= l || b <= t) return (EXIT_FAILURE);

  // Save final composition dimensions after checking all size calculations.
  w = (size_t) (r - l);
  h = (size_t) (b - t);
  if (w > MAX_IMAGE_WIDTH || h > MAX_IMAGE_HEIGHT || w > SIZE_MAX / h || w * h > SIZE_MAX / 4) return (EXIT_FAILURE);
  (*page)[p].width = w;
  (*page)[p].height = h;
  total = w * h * 4;
  out = allocate_u8mem (total);

  // Second pass: render every displayed region and its objects into the final
  // RGBA composition buffer.
  for (rp = 0; rp < (*page)[p].nregion_pos; rp++) {
    rid = (*page)[p].region_pos[rp].region_id;
    k = find_region_index (*page, p, rid);
    if (k < 0) continue;
    ri = (size_t) k;
    reg = &(*page)[p].region[ri];

    // Retrieve the CLUT family for this region, creating the default family
    // if the stream never supplied a CDS for the requested clut_id.
    if (ensure_clut (page, p, reg->clut_id, &ci)) {
      free (out);
      return (EXIT_FAILURE);
    }

    rx = (*page)[p].region_pos[rp].region_horizontal_address;
    ry = (*page)[p].region_pos[rp].region_vertical_address;
    rr = rx + reg->width;
    rb = ry + reg->height;

    // If region_fill_flag is set, initialize the complete region using the
    // depth-appropriate background pixel code from the RCS.
    if (reg->fill_flag && reg->width && reg->height) {
      e = mask_entry (bg (reg), reg->depth);
      a = resolve_clut_color (&(*page)[p].clut[ci], reg->depth, e);
      for (cy = ry; cy < rb; cy++) {
        if (cy < t || cy >= b) continue;
        dy = (size_t) (cy - t);
        for (cx = rx; cx < rr; cx++) {
          if (cx < l || cx >= r) continue;
          dx = (size_t) (cx - l);
          put (out, w, dx, dy, a);
        }
      }
    }

    // Render all objects belonging to the current region.
    for (op = 0; op < reg->nobjects; op++) {
      oid = reg->object_pos[op].object_id;
      k = find_object_index (*page, p, oid);
      if (k < 0) continue;
      oi = (size_t) k;
      obj = &(*page)[p].object[oi];
      if (!obj->width || !obj->height || !obj->buffer || !obj->coded) continue;
      if (obj->width > IMG_PIXEL_COUNT / obj->height) {
        free (out);
        return (EXIT_FAILURE);
      }

      ox = rx + reg->object_pos[op].horizontal_position;
      oy = ry + reg->object_pos[op].vertical_position;
      for (sy = 0; sy < obj->height; sy++) {
        cy = oy + (int64_t) sy;
        if (cy < ry || cy >= rb || cy < t || cy >= b) continue;
        dy = (size_t) (cy - t);
        for (sx = 0; sx < obj->width; sx++) {

          // ODS code strings may have a ragged right edge. Pixels which were
          // not actually coded must leave the existing region contents
          // unchanged rather than being interpreted as CLUT entry 0.
          si = sy * obj->width + sx;
          if (!obj->coded[si]) continue;
          cx = ox + (int64_t) sx;
          if (cx < rx || cx >= rr || cx < l || cx >= r) continue;

          // Reduce or mask the stored CLUT entry to the current region depth.
          e = mask_entry (obj->buffer[si], reg->depth);

          // When non_modifying_colour_flag is set, CLUT entry 1 means that
          // the destination pixel is to remain unchanged.
          if (obj->non_modifying_colour_flag && e == 1) continue;

          a = resolve_clut_color (&(*page)[p].clut[ci], reg->depth, e);
          dx = (size_t) (cx - l);
          put (out, w, dx, dy, a);
        }
      }
    }
  }

  // The final composition is complete; write it to disk and release the
  // temporary RGBA buffer.
  if (write_bmp (s, *page, p, out)) {
    free (out);
    return (EXIT_FAILURE);
  }

  free (out);

  return (EXIT_SUCCESS);
}
