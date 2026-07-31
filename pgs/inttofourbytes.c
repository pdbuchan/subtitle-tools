/*  Copyright (C) 2024-2026 P. David Buchan (pdbuchan@gmail.com)

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

#include "pgs.h"

// Convert 32-bit decimal number into 4 bytes in big-endian.
uint8_t *
inttofourbytes (uint8_t *bytes, int32_t value) {
  
  bytes[0] = value >> 24;
  bytes[1] = (value >> 16) & 255;
  bytes[2] = (value >> 8) & 255;
  bytes[3] = value & 255;

  return (bytes);
}
