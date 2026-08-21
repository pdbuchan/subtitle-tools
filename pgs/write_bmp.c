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

// Create a bitmap file for the current rendered subtitle.
// Filename is start and end timestamps.
int
write_bmp (SUB *sub) {

  size_t width, height, x, y, flipped_y, row_size, image_size, index;
  uint8_t r, g, b, a, padding[3] = {0, 0, 0};
  char filename[MAX_STRINGLEN];
  FILE *fo2;
  int n;

  width = sub->width;
  height = sub->height;
  if (width == 0 || height == 0 || sub->buffer == NULL) {
    fprintf (stderr, "Cannot write an empty subtitle bitmap.\n");
    exit (EXIT_FAILURE);
  }

  n = snprintf (filename, sizeof (filename), "%02d_%02d_%02d_%03d__%02d_%02d_%02d_%03d.bmp",
                sub->start.h, sub->start.m, sub->start.s, sub->start.ms,
                sub->end.h, sub->end.m, sub->end.s, sub->end.ms);
  if (n < 0 || (size_t) n >= sizeof (filename)) {
    fprintf (stderr, "Bitmap filename is too long in write_bmp().\n");
    exit (EXIT_FAILURE);
  }

  fo2 = fopen (filename, "rb");
  if (fo2 != NULL) {
    fprintf (stderr, "Output file %s already exists.\n", filename);
    fclose (fo2);
    exit (EXIT_FAILURE);
  }
  fo2 = fopen (filename, "wb");
  if (fo2 == NULL) {
    fprintf (stderr, "Unable to open output file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  if (width > (SIZE_MAX - 3u) / 3u) {
    fprintf (stderr, "Bitmap row size overflow in write_bmp().\n");
    fclose (fo2);
    exit (EXIT_FAILURE);
  }
  row_size = (width * 3u + 3u) & ~(size_t) 3u;
  if (height > SIZE_MAX / row_size) {
    fprintf (stderr, "Bitmap image size overflow in write_bmp().\n");
    fclose (fo2);
    exit (EXIT_FAILURE);
  }
  image_size = row_size * height;
  if (image_size > UINT32_MAX - 54u || width > INT32_MAX || height > INT32_MAX) {
    fprintf (stderr, "Bitmap is too large for the BMP format used by write_bmp().\n");
    fclose (fo2);
    exit (EXIT_FAILURE);
  }

  // BMP file header.
  write_u16_le (fo2, 0x4d42);
  write_u32_le (fo2, 54u + (uint32_t) image_size);
  write_u16_le (fo2, 0);
  write_u16_le (fo2, 0);
  write_u32_le (fo2, 54);

  // BMP information header.
  write_u32_le (fo2, 40);
  write_s32_le (fo2, (int32_t) width);
  write_s32_le (fo2, (int32_t) height);
  write_u16_le (fo2, 1);
  write_u16_le (fo2, 24);
  write_u32_le (fo2, 0);
  write_u32_le (fo2, (uint32_t) image_size);
  write_s32_le (fo2, 7874);
  write_s32_le (fo2, 7874);
  write_u32_le (fo2, 0);
  write_u32_le (fo2, 0);

  // Write pixels. BMP rows are bottom-up and use BGR order.
  for (y = 0; y < height; y++) {
    flipped_y = height - 1u - y;
    for (x = 0; x < width; x++) {
      index = ((flipped_y * width) + x) * 4u;
      r = sub->buffer[index];
      g = sub->buffer[index + 1u];
      b = sub->buffer[index + 2u];
      a = sub->buffer[index + 3u];

      // Composite transparency against black for the 24-bit output bitmap.
      r = (uint8_t) (((uint16_t) r * a) / 255u);
      g = (uint8_t) (((uint16_t) g * a) / 255u);
      b = (uint8_t) (((uint16_t) b * a) / 255u);

      fputc (b, fo2);
      fputc (g, fo2);
      fputc (r, fo2);
    }
    if (fwrite (padding, 1, row_size - width * 3u, fo2) != row_size - width * 3u) {
      fprintf (stderr, "Failed while writing bitmap %s.\n", filename);
      fclose (fo2);
      exit (EXIT_FAILURE);
    }
  }

  if (fclose (fo2) != 0) {
    fprintf (stderr, "Failed to close bitmap %s cleanly.\n", filename);
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
