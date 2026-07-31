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

// Add pixel(s) to the object's buffer as CLUT entry(s).
int
emit_pixels (STATE *state, PAGE **page, RLE *rle, size_t *x, size_t y) {

  int temp;
  size_t i, page_idx, object_idx, stride, buffer_idx;

  // Retrieve page index from state->page_id.
  temp = find_page_index (state, *page, state->page_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find page index from state->page_id in emit_pixels()\n");
    fprintf (stderr, "page_id: 0x%04x\n", state->page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  // Retrieve object index from state->object_id.
  temp = find_object_index (state, *page, state->object_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find object index from state->object_id in emit_pixels().\n");
    fprintf (stderr, "page_id: 0x%04x, object_id: 0x%04x\n", state->page_id, state->object_id);
    exit (EXIT_FAILURE);
  } else {
    object_idx = temp;
  }

  // Line width of buffer; 0 for 1st line until object width is known.
  stride = (*page)[page_idx].object[object_idx].width;

  // Emit one pixel with color 0x00.
  if (rle->emit_one_00_pixel) {
    buffer_idx = (y * stride) + (*x);
    (*page)[page_idx].object[object_idx].buffer[buffer_idx] = rle->color;
    (*x)++;

  // Emit two pixels with color 0x00.
  } else if (rle->emit_two_00_pixels) {
    buffer_idx = (y * stride) + (*x);
    (*page)[page_idx].object[object_idx].buffer[buffer_idx] = rle->color;
    (*x)++;
    buffer_idx = (y * stride) + (*x);
    (*page)[page_idx].object[object_idx].buffer[buffer_idx] = rle->color;
    (*x)++;

  // Emit pixels according to RLE.
  } else {
    for (i = 0; i < rle->runlength; i++) {
      buffer_idx = (y * stride) + (*x);
      (*page)[page_idx].object[object_idx].buffer[buffer_idx] = rle->color;
      (*x)++;
    }
  }

  return (EXIT_SUCCESS);
}
