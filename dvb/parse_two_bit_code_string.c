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

// Parse one command from a 2-bit/pixel DVB code string.
// Reference: ETSI EN 300 743.
//
// The caller repeats this function until end_of_string_signal is set. Each
// call either describes a run of pixels, one of the special zero-colour runs,
// or the end-of-string marker.
int
parse_two_bit_code_string (STATE *s, SEGMENT *g, size_t *p, size_t lim, RLE *r) {

  uint8_t a, b, c, v;

  r->runlength = 0;
  r->color = 0;
  r->emit_one_00_pixel = 0;
  r->emit_two_00_pixels = 0;

  // Next 2 bits look-ahead. Any non-zero value is a single pixel whose colour
  // is the two-bit value itself.
  if (get_bits (s, g, p, lim, 2, &a)) return (EXIT_FAILURE);
  if (a) {
    r->runlength = 1;
    r->color = a;
    return (EXIT_SUCCESS);
  }

  // Multi-pixel RLE: nextbits == 00. switch_1 selects a runlength of 3-10
  // followed by an explicit two-bit colour.
  if (get_bits (s, g, p, lim, 1, &b)) return (EXIT_FAILURE);
  if (b) {
    if (get_bits (s, g, p, lim, 3, &v) || get_bits (s, g, p, lim, 2, &r->color)) return (EXIT_FAILURE);
    r->runlength = (size_t) v + 3;
    return (EXIT_SUCCESS);
  }

  // switch_2 == 1 emits one pixel of colour index 0.
  if (get_bits (s, g, p, lim, 1, &b)) return (EXIT_FAILURE);
  if (b) {
    r->emit_one_00_pixel = 1;
    return (EXIT_SUCCESS);
  }

  // switch_3 selects end-of-string, two pixels of colour 0, a runlength of
  // 12-27 pixels, or a runlength of 29-284 pixels.
  if (get_bits (s, g, p, lim, 2, &c)) return (EXIT_FAILURE);
  if (c == 0) r->end_of_string_signal = 1;
  else if (c == 1) r->emit_two_00_pixels = 1;
  else if (c == 2) {
    if (get_bits (s, g, p, lim, 4, &v) || get_bits (s, g, p, lim, 2, &r->color)) return (EXIT_FAILURE);
    r->runlength = (size_t) v + 12;
  }
  else {
    if (get_bits (s, g, p, lim, 8, &v) || get_bits (s, g, p, lim, 2, &r->color)) return (EXIT_FAILURE);
    r->runlength = (size_t) v + 29;
  }

  return (EXIT_SUCCESS);
}
