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

// 2-bit (4-entry) CLUT default RGBA values
// Return RGBA values for a given entry value.
// We use alpha a rather than transparency t.
RGBA
default_2clut (uint8_t entry) {

  uint8_t b[2];
  RGBA color = {0, 0, 0, 0};

  // Extract separate bits from entry value.
  b[0] = entry & 0x01;
  b[1] = (entry >> 1) & 0x01;

  // Section 1
  if ((b[0] == 0x00) && (b[1] == 0x00)) {
    color.r = 0x00;  // R
    color.g = 0x00;  // G
    color.b = 0x00;  // B
    color.a = 0x00;  // 100% transparent (equivalent to t = 0xff)
  }

  // Section 2
  if ((b[0] == 0x00) && (b[1] == 0x01)) {
    color.r = 0xff;  // R
    color.g = 0xff;  // G
    color.b = 0xff;  // B
    color.a = 0xff;  // 100% opaque (equivalent to t = 0x00)
  }

  // Section 3
  if ((b[0] == 0x01) && (b[1] == 0x00)) {
    color.r = 0x00;  // R
    color.g = 0x00;  // G
    color.b = 0x00;  // B
    color.a = 0xff;  // 100% opaque (equivalent to t = 0x00)
  }

  // Section 4
  // 127 ≈ 1/2 * 0xff
  if ((b[0] == 0x01) && (b[1] == 0x01)) {
    color.r = 127;   // R
    color.g = 127;   // G
    color.b = 127;   // B
    color.a = 0xff;  // 100% opaque (equivalent to t = 0x00)
  }

  // No sRGB gamma-correction is applied here.

  return (color);
}
