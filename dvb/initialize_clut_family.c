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

// Initialize one DVB CLUT family with the standard default 2-bit, 4-bit, and
// 8-bit CLUTs. A later CLUT Definition Segment may overwrite any subset of
// these entries.
int
initialize_clut_family (PAGE *page, size_t page_idx, size_t clut_idx) {

  size_t i;

  // 2-bit CLUT: 4 entries.
  for (i = 0; i < 4; i++) {
    page[page_idx].clut[clut_idx].clut2[i] = default_2clut ((uint8_t) i);
  }
  page[page_idx].clut[clut_idx].state2 = 'd';

  // 4-bit CLUT: 16 entries.
  for (i = 0; i < 16; i++) {
    page[page_idx].clut[clut_idx].clut4[i] = default_4clut ((uint8_t) i);
  }
  page[page_idx].clut[clut_idx].state4 = 'd';

  // 8-bit CLUT: 256 entries.
  for (i = 0; i < 256; i++) {
    page[page_idx].clut[clut_idx].clut8[i] = default_8clut ((uint8_t) i);
  }
  page[page_idx].clut[clut_idx].state8 = 'd';

  return (EXIT_SUCCESS);
}
