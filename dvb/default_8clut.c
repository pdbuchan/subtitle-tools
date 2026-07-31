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

// 8-bit (256-entry) CLUT default RGBA values
// Return RGBA values for a given entry value.
// We use alpha a rather than transparency t.
RGBA
default_8clut (uint8_t entry) {

  size_t i;
  uint8_t b[8];
  RGBA color = {0, 0, 0, 0};

  const uint32_t FF = 0xFFU;

  // Extract separate bits from entry value.
  for (i = 0; i < 8; i++) {
    b[i] = (entry >> i) & 0x01;
  }

  // Section 1
  if ((b[0] == 0x00) && (b[4] == 0x00)) {
    if ((b[1] == 0x00) && (b[2] == 0x00) && (b[3] == 0x00)) {
      if ((b[5] == 0x00) && (b[6] == 0x00) && (b[7] == 0x00)) {
        color.r = 0x00;  // R
        color.g = 0x00;  // G
        color.b = 0x00;  // B
        color.a = 0x00;  // 100% transparent (equivalent to t = 0xff)

      // Section 2
      // 192 = 3/4 * 256
      } else {
        color.r = (uint8_t) (FF * b[7]);  // R
        color.g = (uint8_t) (FF * b[6]);  // G
        color.b = (uint8_t) (FF * b[5]);  // B
        color.a = 63;                     // 75% transparent (equivalent to t = 192)
      }

    // Section 3
    // 1/3 * FF ≈ 85, 2/3 * FF ≈ 170
    } else {
      color.r = (uint8_t) ((85 * b[7] + 170 * b[3]));  // R
      color.g = (uint8_t) ((85 * b[6] + 170 * b[2]));  // G
      color.b = (uint8_t) ((85 * b[5] + 170 * b[1]));  // B
      color.a = 0xff;                                  // 100% opaque (equivalent to t = 0x00)
    }
  }

  // Section 4
  // 127 ≈ 1/2 * FF
  if ((b[0] == 0x00) && (b[4] == 0x01)) {
    color.r = (uint8_t) ((85 * b[7] + 170 * b[3]));  // R
    color.g = (uint8_t) ((85 * b[6] + 170 * b[2]));  // G
    color.b = (uint8_t) ((85 * b[5] + 170 * b[1]));  // B
    color.a = (uint8_t) 127;                         // 50% transparent
  }

  // Section 5
  if ((b[0] == 0x01) && (b[4] == 0x00)) {
    // 1/6 * FF ≈ 43, 1/3 * FF ≈ 85, 127 ≈ 1/2 * FF
    color.r = (uint8_t) ((43 * b[7] + 85 * b[3] + 127));  // R
    color.g = (uint8_t) ((43 * b[6] + 85 * b[2] + 127));  // G
    color.b = (uint8_t) ((43 * b[5] + 85 * b[1] + 127));  // B
    color.a = 0xff;                                       // 100% opaque (equivalent to t = 0x00)
  }

  // Section 6
  if ((b[0] == 0x01) && (b[4] == 0x01)) {
    color.r = (uint8_t) ((43 * b[7] + 85 * b[3]));  // R
    color.g = (uint8_t) ((43 * b[6] + 85 * b[2]));  // G
    color.b = (uint8_t) ((43 * b[5] + 85 * b[1]));  // B
    color.a = 0xff;                                 // 100% opaque (equivalent to t = 0x00)
  }

  // No sRGB gamma-correction is applied here.

  return (color);
}
