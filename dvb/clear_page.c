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

void
clear_page (STATE *state, PAGE *page) {

  int temp;
  size_t i, page_idx;

  // Retrieve page index from state->page_id.
  temp = find_page_index (state, page, state->page_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find page index from state->page_id in clear_page()\n");
    fprintf (stderr, "page_id: 0x%04x\n", state->page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  // Clear page for next Display Set.
  page[page_idx].complete = 0;

  for (i = 0; i < page[page_idx].nobjects; i++) {
    free (page[page_idx].object[i].buffer);
  }
  page[page_idx].nobjects = 0;
  free (page[page_idx].object);
  page[page_idx].object = NULL;

  free (page[page_idx].clut);
  page[page_idx].clut = NULL;
  page[page_idx].ncluts = 0;

  page[page_idx].nregion_pos = 0;
  free (page[page_idx].region_pos);
  page[page_idx].region_pos = NULL;
  page[page_idx].nregions = 0;
  free (page[page_idx].region);
  page[page_idx].region = NULL;

  page[page_idx].width = 0;
  page[page_idx].height = 0;

  memset (page[page_idx].buffer, 0, IMG_BUFFER_SIZE * sizeof (uint8_t));

  // State
  state->display_width = 0;
  state->display_height = 0;
}
