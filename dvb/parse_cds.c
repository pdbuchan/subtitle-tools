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

// Convert the DVB transparency value T to the conventional alpha direction
// used internally by this program: alpha 255 = opaque, alpha 0 = transparent.
//
// EN 300 743 defines T = 0 as no transparency and says that maximum T plus
// one would correspond to full transparency. Therefore an 8-bit T value is
// interpreted as T / 256 rather than T / 255.
static uint8_t
alpha_from_t (uint8_t t) {

  return ((uint8_t) ((((uint32_t) (256U - t) * 255U) + 128U) / 256U));
}

// Convert one DVB YCbCr/T CLUT entry to the RGBA representation used by the
// renderer. EN 300 743 gives Y = 0 a special meaning of full transparency, so
// no BT.601 conversion is performed for that value.
static RGBA
rgba_from_ycbcr (uint8_t y, uint8_t cb, uint8_t cr, uint8_t t) {

  RGBA a;
  int rgb[3];

  if (!y) {
    a.r = a.g = a.b = a.a = 0;
    return (a);
  }

  YCbCr2RGB_bt601 (y, cb, cr, rgb);
  a.r = (uint8_t) rgb[0];
  a.g = (uint8_t) rgb[1];
  a.b = (uint8_t) rgb[2];
  a.a = alpha_from_t (t);

  return (a);
}

// CLUT Definition Segment (CDS)
// Reference: ETSI EN 300 743.
int
parse_cds (STATE *s, PAGE **page, size_t *off, SEGMENT *seg, FILE *fo) {

  int k;
  size_t body, end, len, pi, ci, old, need;
  uint8_t sync, type, id, ver, eid, full, y, cr, cb, t, f2, f4, f8, ry, rr, rb, rt;
  uint16_t pid = s->pid, page_id;
  void *tmp;
  RGBA rgba;

  fprintf (fo, "\n  CLUT Definition Segment (CDS)\n");

  // Segment header: Sync Byte, Segment Type, Page ID, Segment Length.
  if (!bytes_available (*off, 6, seg[pid].length)) return (EXIT_FAILURE);

  // Sync Byte (1 byte)
  sync = seg[pid].buffer[(*off)++];
  if (sync != 0x0f) return (EXIT_FAILURE);

  // Segment Type (1 byte)
  type = seg[pid].buffer[(*off)++];
  if (type != 0x12) return (EXIT_FAILURE);
  segment_types (s, type, fo);

  // Page ID (2 bytes)
  page_id = (uint16_t) (((uint16_t) seg[pid].buffer[*off] << 8) | seg[pid].buffer[*off + 1]);
  *off += 2;
  s->page_id = page_id;

  // Segment Length (2 bytes)
  len = (size_t) (((uint16_t) seg[pid].buffer[*off] << 8) | seg[pid].buffer[*off + 1]);
  *off += 2;
  body = *off;
  if (!bytes_available (body, len, seg[pid].length) || len < 2) return (EXIT_FAILURE);
  end = body + len;
  fprintf (fo, "    Page ID (2 bytes): 0x%04x\n    Segment Length (2 bytes): %zu bytes\n", page_id, len);

  // Obtain the compact Page array index from page_id.
  k = find_page_index (s, *page, page_id);
  if (k < 0) return (EXIT_FAILURE);
  pi = (size_t) k;

  // CLUT ID (1 byte). One clut_id identifies a CLUT family consisting of a
  // 2-bit, 4-bit, and 8-bit CLUT representing the same palette.
  id = seg[pid].buffer[(*off)++];

  // Allocate this CLUT family if it has not been seen before. Start with the
  // ETSI default CLUTs; incoming CDS entries overwrite only the entries they
  // explicitly define.
  k = find_clut_index (*page, pi, id);
  if (k < 0) {
    old = (*page)[pi].ncluts;
    tmp = realloc ((*page)[pi].clut, (old + 1) * sizeof (CLUT_FAMILY));
    if (!tmp) return (EXIT_FAILURE);
    (*page)[pi].clut = tmp;
    memset (&(*page)[pi].clut[old], 0, sizeof (CLUT_FAMILY));
    ci = old;
    (*page)[pi].clut[ci].clut_id = id;
    (*page)[pi].ncluts++;
    initialize_clut_family (*page, pi, ci);
  }
  else ci = (size_t) k;

  // CLUT Family Version Number (4 bits), followed by 4 reserved bits.
  ver = (seg[pid].buffer[*off] >> 4) & 0x0f;
  (*page)[pi].clut[ci].version = ver;
  (*off)++;
  fprintf (fo, "    CLUT ID: 0x%02x Version: 0x%x\n", id, ver);

  // CLUT Entry Loop. Each entry states which member of the CLUT family it
  // updates, followed by Y, Cr, Cb, and transparency T.
  while (*off < end) {
    if (!bytes_available (*off, 2, end)) return (EXIT_FAILURE);

    // CLUT Entry ID (1 byte)
    eid = seg[pid].buffer[(*off)++];

    // 2-bit/entry, 4-bit/entry, and 8-bit/entry CLUT flags.
    f2 = (seg[pid].buffer[*off] >> 7) & 1;
    f4 = (seg[pid].buffer[*off] >> 6) & 1;
    f8 = (seg[pid].buffer[*off] >> 5) & 1;

    // full_range_flag describes the RESOLUTION of the following component
    // fields. It does not select full-range versus limited-range BT.601.
    full = seg[pid].buffer[(*off)++] & 1;
    need = full ? 4 : 2;
    if (!bytes_available (*off, need, end)) return (EXIT_FAILURE);

    // Full-resolution form: 8 bits each for Y, Cr, Cb, and T.
    if (full) {
      y = seg[pid].buffer[*off];
      cr = seg[pid].buffer[*off + 1];
      cb = seg[pid].buffer[*off + 2];
      t = seg[pid].buffer[*off + 3];
      *off += 4;
    }

    // Reduced-resolution form: the transmitted fields contain only the most
    // significant 6, 4, 4, and 2 bits respectively. Restore their bit
    // positions by appending zero low-order bits before conversion.
    else {
      ry = (seg[pid].buffer[*off] >> 2) & 0x3f;
      rr = (uint8_t) (((seg[pid].buffer[*off] & 3) << 2) | ((seg[pid].buffer[*off + 1] >> 6) & 3));
      rb = (seg[pid].buffer[*off + 1] >> 2) & 0x0f;
      rt = seg[pid].buffer[*off + 1] & 3;
      *off += 2;
      y = (uint8_t) (ry << 2);
      cr = (uint8_t) (rr << 4);
      cb = (uint8_t) (rb << 4);
      t = (uint8_t) (rt << 6);
    }

    fprintf (fo, "      CLUT Entry 0x%02x flags 2/4/8=%u/%u/%u full_range=%u Y/Cr/Cb/T=%u/%u/%u/%u\n", eid, f2, f4, f8, full, y, cr, cb, t);
    rgba = rgba_from_ycbcr (y, cb, cr, t);

    // Load the converted colour into whichever CLUT member(s) the entry flags
    // identify, and mark those CLUTs as customized rather than default.
    if (f2) {
      (*page)[pi].clut[ci].clut2[eid & 3] = rgba;
      (*page)[pi].clut[ci].state2 = 'c';
    }
    if (f4) {
      (*page)[pi].clut[ci].clut4[eid & 0x0f] = rgba;
      (*page)[pi].clut[ci].state4 = 'c';
    }
    if (f8) {
      (*page)[pi].clut[ci].clut8[eid] = rgba;
      (*page)[pi].clut[ci].state8 = 'c';
    }
  }

  return (EXIT_SUCCESS);
}
