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

// Clear palettes.
// This is performed when a new epoch or acquisition state begins.
// Set each palette entry to 100% transparent black.
int
clear_palettes (PALETTE *palette) {

  size_t i, j;

  for (i = 0; i < (size_t) MAX_PALETTES; i++) {
    palette[i].version = 0u;
    for (j = 0; j < (size_t) MAX_PALETTE_ENTRIES; j++) {
      palette[i].entry[j].r = 0u;
      palette[i].entry[j].g = 0u;
      palette[i].entry[j].b = 0u;
      palette[i].entry[j].alpha = 0u;
    }  // Next palette entry
  }  // Next palette

  return (EXIT_SUCCESS);
}
