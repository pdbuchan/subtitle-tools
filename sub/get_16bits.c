/*  Copyright (C) 2025-2026 P. David Buchan (pdbuchan@gmail.com)

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

#include "sub.h"

// Get next 16 bits from buffer starting at requested bit offset.
int
get_16bits (uint8_t *spu_buffer, size_t *bitpos, uint16_t *bits) {

  size_t byte_offset = *bitpos / 8;
  size_t bit_offset  = *bitpos % 8;

  uint32_t value = 0;

  // Read enough bytes to include all 16 bits (up to 3 bytes to get 16 bits starting anywhere).
  // Memory for spu_buffer is allocated in extract_subs() with dimension MAX_SPU_SIZE + 2 to prevent overflow here when we get to end of PXD.
  value = (spu_buffer[byte_offset + 0] << 16) |
          (spu_buffer[byte_offset + 1] << 8) |
           spu_buffer[byte_offset + 2];

  // Shift left to remove leading bits before desired data.
  value <<= bit_offset;

  // Now desired bits start at Bit 15; shift right 8 bits and AND with 0xffff to capture just the 16-bit value we want.
  *bits = (uint16_t) ((value >> 8) & 0xffff);

  return (EXIT_SUCCESS);
}
