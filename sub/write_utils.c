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

void
write_u16_le (FILE *fo, uint16_t val) {
  fputc ((int) (val & UINT16_C(0xff)), fo);
  fputc ((int) ((val >> 8) & UINT16_C(0xff)), fo);
}

void
write_u32_le (FILE *fo, uint32_t val) {
  fputc ((int) (val & UINT32_C(0xff)), fo);
  fputc ((int) ((val >> 8) & UINT32_C(0xff)), fo);
  fputc ((int) ((val >> 16) & UINT32_C(0xff)), fo);
  fputc ((int) ((val >> 24) & UINT32_C(0xff)), fo);
}

void
write_s32_le (FILE *fo, int32_t val) {
  write_u32_le (fo, (uint32_t) val);
}
