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

// Decode PGS RLE data to one palette-index byte per pixel.
int
decode_rle (uint8_t *rle_data, size_t rle_data_len, size_t width, size_t height, uint8_t *pixels) {

  size_t i = 0, x = 0, y = 0, run;
  uint8_t color, flags;

  if (width == 0 || height == 0 || width > SIZE_MAX / height) {
    fprintf (stderr, "Invalid image dimensions in decode_rle().\n");
    exit (EXIT_FAILURE);
  }

  memset (pixels, 0, width * height);

  while (i < rle_data_len && y < height) {
    color = rle_data[i++];
    run = 1;

    if (color == 0) {
      if (i >= rle_data_len) {
        fprintf (stderr, "Unexpectedly reached end of RLE data after escape byte in decode_rle().\n");
        exit (EXIT_FAILURE);
      }

      flags = rle_data[i++];
      run = flags & 0x3fu;

      if (flags & 0x40u) {
        if (i >= rle_data_len) {
          fprintf (stderr, "Unexpectedly reached end of long RLE run in decode_rle().\n");
          exit (EXIT_FAILURE);
        }
        run = (run << 8) | rle_data[i++];
      }

      if (flags & 0x80u) {
        if (i >= rle_data_len) {
          fprintf (stderr, "Unexpectedly reached end of colored RLE run in decode_rle().\n");
          exit (EXIT_FAILURE);
        }
        color = rle_data[i++];
      } else {
        color = 0;
      }
    }

    // A run length of zero is the end-of-line marker (00 00).
    if (run == 0) {
      if (x != width) {
        fprintf (stderr, "RLE line %zu contains %zu pixels; expected %zu.\n", y, x, width);
        exit (EXIT_FAILURE);
      }
      x = 0;
      y++;
      continue;
    }

    if (run > (width - x)) {
      fprintf (stderr, "RLE run of %zu pixels crosses the end of line %zu in decode_rle().\n", run, y);
      exit (EXIT_FAILURE);
    }

    memset (pixels + (y * width) + x, color, run);
    x += run;
  }

  // The final line may either be followed by EOL or end exactly at the image boundary.
  if (y == height - 1 && x == width) {
    y++;
    x = 0;
  }

  if (y != height || x != 0) {
    fprintf (stderr, "Insufficient RLE data: decoded %zu of %zu rows in decode_rle().\n", y, height);
    exit (EXIT_FAILURE);
  }

  if (i != rle_data_len) {
    fprintf (stderr, "RLE data contains %zu trailing byte(s) after the image is complete.\n", rle_data_len - i);
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
