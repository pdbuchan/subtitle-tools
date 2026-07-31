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

// Decode next field of RLE-encoded pixel data.
int
decode_rle (uint8_t *spu_buffer, size_t *bitpos, RLE *rle) {

  uint16_t bits;

  // Format of RLE-encoded pixel data (PXD)
  // Val Range  Bits     Format
  // 1-3        4        nncc
  // 4-15       8        00nn nncc
  // 16-63      12       0000 nnnn nncc
  // 64-255     16       0000 00nn nnnn nncc *
  // If at end of a line and bit count is not a multiple of 8, four fill bits of 0 are added.
  // * Special case: If nn nnnn nn equals 0, then use same pixel value until end of line.

  // Obtain the next 16 bits, store in uint16_t bits.
  get_16bits (spu_buffer, bitpos, &bits);

  rle->to_eol = 0;  // Default to not running the same color of pixel to end of line.

  // 0000 00nn nnnn nncc (16 bits)
  if ((bits >> 10) == 0) {
    rle->runlength = (size_t) (bits >> 2);
    rle->color = (uint8_t) (bits & 0x03);  // 3 = 0000 0000 0000 0011
    if (rle->runlength == 0) rle->to_eol = 1;  // * Special case: same pixel type to end of line
    (*bitpos) += 16;

  // 0000 nnnn nncc xxxx (12 bits)
  } else if ((bits >> 12) == 0) {
    rle->runlength = (size_t) (bits >> 6);
    rle->color = (uint8_t) ((bits >> 4) & 0x03);
    (*bitpos) += 12;

  // 00nn nncc xxxx xxxx (8 bits)
  } else if ((bits >> 14) == 0) {
    rle->runlength = (size_t) ((bits >> 10) & 15);  // 15 = 0000 0000 0000 1111
    rle->color = (uint8_t) ((bits >> 8) & 0x03);
    (*bitpos) += 8;

  // nncc xxxx xxxx xxxx (4 bits)
  } else {
    rle->runlength = (size_t) (bits >> 14);
    rle->color = (uint8_t) ((bits >> 12) & 0x03);
    (*bitpos) += 4;
  }

  return (EXIT_SUCCESS);
}
