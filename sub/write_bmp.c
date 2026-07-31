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

// Create a bitmap file for an image object.
// Filename is start and end timestamps.
int
write_bmp (uint8_t *data, IDX *idx, int lang, SUB *sub) {

  size_t width, height, x, y, flipped_y, row_size, image_size, index;
  uint8_t r, g, b, a, padding[3] = {0, 0, 0};  // Padding to make each row 4 bytes aligned
  char *filename;
  FILE *fo2;

  // Allocate memory for various arrays.
  filename = allocate_strmem (MAX_STRINGLEN);

  sprintf (filename, "%02d_%02d_%02d_%03d__%02d_%02d_%02d_%03d %s%d.bmp",
    sub->start.h, sub->start.m, sub->start.s, sub->start.ms,
    sub->end.h, sub->end.m, sub->end.s, sub->end.ms, idx->id[lang], lang);

  // Open output file.
  fo2 = fopen (filename, "r");
  if (fo2 != NULL) {
    fprintf (stderr, "Output file %s already exists.\n", filename);
    exit (EXIT_FAILURE);
  }
  fo2 = fopen (filename, "wb");
  if (fo2 == NULL) {
    fprintf (stderr, "Unable to open output file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  width = sub->width;
  height = sub->height;

  // Calculate the padding required for each row.
  row_size = (width * 3 + 3) & (~3);  // Each row must be a multiple of 4 bytes
  image_size = row_size * height;

  // BMP file header
  write_u16_le (fo2, 0x4d42);           // File type, should be "BM"
  write_u32_le (fo2, 54 + (uint32_t) image_size);  // Size of the file (bytes)
  write_u16_le (fo2, 0);                // Reserved (set to 0)
  write_u16_le (fo2, 0);                // Reserved (set to 0)
  write_u32_le (fo2, 54);               // Offset (bytes) to the start of the pixel data

  // BMP information header
  write_u32_le (fo2, 40);               // Size of this header (40 bytes)
  write_s32_le (fo2, (int32_t) width);   // Width of the image (px)
  write_s32_le (fo2, (int32_t) height);  // Height of the image (px)
  write_u16_le (fo2, 1);                // Number of color planes (always 1)
  write_u16_le (fo2, 24);               // Bits per pixel (24 for RGB)
  write_u32_le (fo2, 0);                // Compression method (0 for none)
  write_u32_le (fo2, (uint32_t) image_size);  // Size of the image data (bytes)
  write_s32_le (fo2, 7874);             // Horizontal resolution (in pixels per meter) (200 DPI)
  write_s32_le (fo2, 7874);             // Vertical resolution (in pixels per meter) (200 DPI)
  write_u32_le (fo2, 0);                // Number of colors used (0 for 2^24)
  write_u32_le (fo2, 0);                // Important colors (0 for all)

  // Write pixels to bitmap output file.
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {

      // Get from buffer the R, G, B, and A values for next pixel.
      flipped_y = height - 1 - y;
      index = ((flipped_y * width) + x) * 4;
      r = data[index + 0];  // Red
      g = data[index + 1];  // Green
      b = data[index + 2];  // Blue
      a = data[index + 3];  // Alpha

      // In a video setting alpha is used to adjust transparency, but here
      // we apply alpha as darkness: transparent = black, opaque = no change
      // alpha: 0 = transparent, 255 = opaque
      r = (uint8_t) ((r * (uint16_t) a) / 255);
      g = (uint8_t) ((g * (uint16_t) a) / 255);
      b = (uint8_t) ((b * (uint16_t) a) / 255);

      // Write pixel data to file.
      // Note that RGB bitmaps are written in order BGR.
      fputc (b, fo2);  // B
      fputc (g, fo2);  // G
      fputc (r, fo2);  // R

    }  // Next column

    // Write padding, if necessary.
    fwrite (padding, 1, row_size - width * 3, fo2);

  }  // Next row

  // Close output file.
  fclose (fo2);

  // Free allocated memory.
  free (filename);

  return (EXIT_SUCCESS);
}
