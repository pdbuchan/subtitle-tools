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

// Parse one command from a 4-bit/pixel DVB code string.
// Reference: ETSI EN 300 743.
int
parse_four_bit_code_string (STATE *s, SEGMENT *g, size_t *p, size_t lim, RLE *r) {

  uint8_t a, b, c, v;

  r->runlength = 0;
  r->color = 0;
  r->emit_one_00_pixel = 0;
  r->emit_two_00_pixels = 0;

  // A non-zero four-bit value represents one pixel of that colour.
  if (get_bits (s, g, p, lim, 4, &a)) return (EXIT_FAILURE);
  if (a) {
    r->runlength = 1;
    r->color = a;
    return (EXIT_SUCCESS);
  }

  // First switch: 000 terminates the code string; other three-bit values
  // represent short runs of colour index 0.
  if (get_bits (s, g, p, lim, 1, &b)) return (EXIT_FAILURE);
  if (!b) {
    if (get_bits (s, g, p, lim, 3, &a)) return (EXIT_FAILURE);
    if (!a) r->end_of_string_signal = 1;
    else r->runlength = (size_t) a + 2;
    return (EXIT_SUCCESS);
  }

  // Second switch: runlength 4-7 followed by an explicit four-bit colour.
  if (get_bits (s, g, p, lim, 1, &b)) return (EXIT_FAILURE);
  if (!b) {
    if (get_bits (s, g, p, lim, 2, &v) || get_bits (s, g, p, lim, 4, &r->color)) return (EXIT_FAILURE);
    r->runlength = (size_t) v + 4;
    return (EXIT_SUCCESS);
  }

  // Final two-bit switch: emit one or two zero-colour pixels, or select one
  // of the two longer runlength encodings.
  if (get_bits (s, g, p, lim, 2, &c)) return (EXIT_FAILURE);
  if (c == 0) r->emit_one_00_pixel = 1;
  else if (c == 1) r->emit_two_00_pixels = 1;
  else if (c == 2) {
    if (get_bits (s, g, p, lim, 4, &v) || get_bits (s, g, p, lim, 4, &r->color)) return (EXIT_FAILURE);
    r->runlength = (size_t) v + 9;
  }
  else {
    if (get_bits (s, g, p, lim, 8, &v) || get_bits (s, g, p, lim, 4, &r->color)) return (EXIT_FAILURE);
    r->runlength = (size_t) v + 25;
  }

  return (EXIT_SUCCESS);
}
