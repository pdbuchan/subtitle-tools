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

// Write a unsigned little-endian 16-bit value to file.
void
write_u16_le (FILE *fo, uint16_t val) {

  fputc (val & 0xff, fo);
  fputc ((val >> 8) & 0xff, fo);

}

// Write a unsigned little-endian 32-bit value to file.
void
write_u32_le (FILE *fo, uint32_t val) {

  fputc (val & 0xff, fo);
  fputc ((val >> 8) & 0xff, fo);
  fputc ((val >> 16) & 0xff, fo);
  fputc ((int) ((val >> 24) & 0xffU), fo);

}

// Write a signed little-endian 32-bit value to file.
void
write_s32_le (FILE *fo, int32_t val) {

  write_u32_le (fo, (uint32_t) val);  // Cast as unsigned in order to preserve bit pattern.

}
