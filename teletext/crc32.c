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

#include "teletext.h"

// Calculate the CRC-32/MPEG-2 value used by MPEG-2 PSI sections.
//
// Polynomial: 0x04c11db7
// Initial value: 0xffffffff
// Input and output are not reflected and there is no final XOR.
uint32_t
mpeg2_crc32 (const uint8_t *d, size_t n) {

  uint32_t c = UINT32_C (0xffffffff);
  size_t i;
  unsigned int b;

  // Feed each byte most-significant bit first. For each of its eight bits,
  // shift the register and apply the generator polynomial when the previous
  // top bit was set.
  for (i = 0; i < n; i++) {
    c ^= (uint32_t) d[i] << 24;
    for (b = 0; b < 8; b++) c = (c & UINT32_C (0x80000000)) ? (c << 1) ^ UINT32_C (0x04c11db7) : c << 1;
  }

  return (c);
}
