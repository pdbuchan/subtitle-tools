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

// Parse one command from an 8-bit/pixel DVB code string.
// Reference: ETSI EN 300 743.
int
parse_eight_bit_code_string (STATE *s, SEGMENT *g, size_t *p, size_t lim, RLE *r) {

  uint8_t a, b, v;

  r->runlength = 0;
  r->color = 0;
  r->emit_one_00_pixel = 0;
  r->emit_two_00_pixels = 0;

  // A non-zero byte represents one pixel whose CLUT entry is that byte.
  if (get_bits (s, g, p, lim, 8, &a)) return (EXIT_FAILURE);
  if (a) {
    r->runlength = 1;
    r->color = a;
    return (EXIT_SUCCESS);
  }

  // A zero byte introduces an RLE command. The switch bit distinguishes a
  // run of colour 0 from a run followed by an explicit 8-bit colour value.
  if (get_bits (s, g, p, lim, 1, &b) || get_bits (s, g, p, lim, 7, &v)) return (EXIT_FAILURE);
  if (!b) {
    if (!v) r->end_of_string_signal = 1;
    else r->runlength = v;
  }
  else {
    if (v < 3) return (EXIT_FAILURE);
    r->runlength = v;
    if (get_bits (s, g, p, lim, 8, &r->color)) return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
