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

// Apply bit reductions or map-table expansions as required.
RGBA
resolve_clut_color (CLUT_FAMILY *clut, uint8_t depth, uint8_t entry) {

  switch (depth) {

    case 0x01:  // 2-bit depth
      if (clut->state2 == 'c')
        return (clut->clut2[entry]);

      if (clut->state8 == 'c')
        return (clut->clut8[map_2to8 (entry)]);

      if (clut->state4 == 'c')
        return (clut->clut4[map_2to4 (entry)]);

      return (clut->clut2[entry]);

    case 0x02:  // 4-bit depth
      if (clut->state4 == 'c')
        return (clut->clut4[entry]);

      if (clut->state8 == 'c')
        return (clut->clut8[map_4to8 (entry)]);

      if (clut->state2 == 'c')
        return (clut->clut2[reduce_4to2 (entry)]);

      return (clut->clut4[entry]);

    case 0x03:  // 8-bit depth
      if (clut->state8 == 'c')
        return (clut->clut8[entry]);

      if (clut->state4 == 'c')
        return (clut->clut4[reduce_8to4 (entry)]);

      if (clut->state2 == 'c')
        return (clut->clut2[reduce_8to2 (entry)]);

      return (clut->clut8[entry]);

    default:
      fprintf (stderr, "Reserved region bit depth encountered in resolve_clut_color(): %u\n", depth);
      exit (EXIT_FAILURE);
  }
}

// 8-bit to 4-bit CLUT Index Reduction
uint8_t
reduce_8to4 (uint8_t input) {

  return (input >> 4);
}

// 8-bit to 2-bit CLUT Index Reduction
uint8_t
reduce_8to2 (uint8_t input) {

  return (input >> 6);
}

// 4-bit to 2-bit CLUT Index Reduction
uint8_t
reduce_4to2 (uint8_t input) {

  return (input >> 2);
}

// 2-bit to 4-bit CLUT Index Map Table
uint8_t
map_2to4 (uint8_t input) {

  uint8_t output[] = {0x00, 0x07, 0x08, 0x0f};

  return (output[input & 0x03]);
}

// 2-bit to 8-bit CLUT Index Map Table
uint8_t
map_2to8 (uint8_t input) {

  uint8_t output[] = {0x00, 0x77, 0x88, 0xff};

  return (output[input & 0x03]);
}

// 4-bit to 8-bit CLUT Index Map Table
uint8_t
map_4to8 (uint8_t input) {

  uint8_t output[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

  return (output[input & 0x0f]);
}
