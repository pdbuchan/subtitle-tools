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

// Initialize a single CLUT family with the default entries.
// The CLUT family is identified by page_id and clut_id.
// ETSI EN 300 743
int
initialize_clut_family (STATE *state, PAGE *page, size_t clut_idx) {

  int temp;
  size_t page_idx, entry;

  // Find page index for state->page_id.
  temp = find_page_index (state, page, state->page_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find index for state->page_id: 0x%04x in initialize_clut_family().\n", state->page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }
  
  // 2-bit (4-entry) CLUT
  for (entry = 0; entry < 4; entry++) {
    page[page_idx].clut[clut_idx].clut2[entry] = default_2clut (entry);
  }
  page[page_idx].clut[clut_idx].state2 = 'd';  // Mark this CLUT as having default contents.

  // 4-bit (16-entry) CLUT
  for (entry = 0; entry < 16; entry++) {
    page[page_idx].clut[clut_idx].clut4[entry] = default_4clut (entry);
  }
  page[page_idx].clut[clut_idx].state4 = 'd';  // Mark this CLUT as having default contents.

  // 8-bit (256-entry) CLUT
  for (entry = 0; entry < 256; entry++) {
    page[page_idx].clut[clut_idx].clut8[entry] = default_8clut (entry);
  }
  page[page_idx].clut[clut_idx].state8 = 'd';  // Mark this CLUT as having default contents.

  return (EXIT_SUCCESS);
}
