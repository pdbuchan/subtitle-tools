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

// Add the pixels represented by one decoded RLE command to an object's
// CLUT-entry buffer. parse_ods() calls this only during its second pass,
// after the final object width and height have already been established.
int
emit_pixels (PAGE **page, size_t page_idx, size_t object_idx, RLE *rle, size_t *x, size_t y) {

  OBJECT *object;
  size_t i, n, index;

  object = &(*page)[page_idx].object[object_idx];

  // Most commands specify an explicit run length. Two special DVB RLE
  // commands instead request one or two pixels having CLUT entry 0.
  n = rle->runlength;
  if (rle->emit_one_00_pixel) n = 1;
  if (rle->emit_two_00_pixels) n = 2;

  // A zero-length command is an end-of-string indication and emits no pixel.
  if (n == 0) return (EXIT_SUCCESS);

  // Width and height are fixed by the first ODS pass. These checks also
  // protect the multiplication used to form a linear buffer index.
  if (*x > object->width || n > object->width - *x) return (EXIT_FAILURE);
  if (y >= object->height || object->width > IMG_PIXEL_COUNT / object->height) {
    return (EXIT_FAILURE);
  }

  index = y * object->width + *x;
  if (index > IMG_PIXEL_COUNT || n > IMG_PIXEL_COUNT - index) {
    return (EXIT_FAILURE);
  }

  // Mark each emitted pixel as coded. A DVB object may have a ragged right
  // edge; pixels beyond the end of a shorter code string are not background
  // pixels and must leave the underlying region unchanged during composition.
  for (i = 0; i < n; i++) {
    object->buffer[index + i] = rle->color;
    object->coded[index + i] = 1;
  }
  *x += n;

  return (EXIT_SUCCESS);
}
