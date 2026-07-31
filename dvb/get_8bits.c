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

// Get 8 bits from segment buffer at requested bit offset.
int
get_8bits (STATE *state, SEGMENT *segment, size_t *bitpos, uint8_t *bits) {

  size_t byteoffset = (*bitpos) / 8;
  size_t bit_offset = (*bitpos) % 8;

  uint16_t value;

  // Read enough bytes to include all 8 bits (up to 2 bytes to get 8 bits starting anywhere).
  // Memory for segment->buffer is allocated in build_pes_segment() with dimension MAX_PIDS + 1
  // to prevent overflow here when we get to the end of the buffer.
  value = (segment[state->pid].buffer[byteoffset] << 8) |
          segment[state->pid].buffer[byteoffset + 1];

  // Shift right appropriate bits and mask with 0xff to capture just the 8-bit value we want.
  (*bits) = (uint8_t) ((value >> (16 - bit_offset - 8)) & 0xff);

  return (EXIT_SUCCESS);
}
