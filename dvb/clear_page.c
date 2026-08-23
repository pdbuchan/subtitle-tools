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

// Clear all data belonging to one completed Display Set while retaining the
// PAGE structure itself for reuse by a later Display Set having the same
// page_id.
void
clear_page (PAGE *page, size_t p) {

  size_t i;

  page[p].complete = 0;

  // Each object owns both a CLUT-entry buffer and a coded-pixel mask.
  for (i = 0; i < page[p].nobjects; i++) {
    free (page[p].object[i].buffer);
    free (page[p].object[i].coded);
  }
  free (page[p].object);
  page[p].object = NULL;
  page[p].nobjects = 0;

  free (page[p].clut);
  page[p].clut = NULL;
  page[p].ncluts = 0;

  free (page[p].region_pos);
  page[p].region_pos = NULL;
  page[p].nregion_pos = 0;

  free (page[p].region);
  page[p].region = NULL;
  page[p].nregions = 0;

  // Keep the PAGE-owned RGBA buffer in a known empty state between Display
  // Sets. Its lifetime is the lifetime of the PAGE slot itself.
  page[p].width = page[p].height = 0;
  if (page[p].buffer) memset (page[p].buffer, 0, IMG_BUFFER_SIZE);
}
