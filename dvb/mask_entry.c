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

// Mask CLUT entry value to ensure appropriate range for requested bit-depth.
uint8_t
mask_entry (uint8_t entry, uint8_t depth) {

  switch (depth) {

    // 2-bit depth
    case 0x01:
      return (entry & 0x03);

    // 4-bit depth
    case 0x02:
      return (entry & 0x0f);

    // 8-bit depth
    case 0x03:
      return (entry);

    default:
      fprintf (stderr, "Invalid CLUT depth in normalize_entry(): %u\n", depth);
      exit (EXIT_FAILURE);
  }
}
