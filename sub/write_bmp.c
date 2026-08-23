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

// Create a 24-bit bitmap file.  Transparency is composited against black so
// that the extracted subtitle remains easy to inspect in ordinary BMP viewers.
int
write_bmp (const uint8_t *data, IDX *idx, size_t lang, SUB *sub) {

  size_t width, height, x, y, flipped_y, row_size, image_size, index, padding_size;
  uint8_t r, g, b, a, padding[3] = {0, 0, 0};
  char filename[MAX_STRINGLEN];
  int n;
  FILE *fo;

  if (data == NULL || lang >= idx->n_id || sub->width == 0 || sub->height == 0) {
    fprintf (stderr, "Invalid bitmap parameters in write_bmp().\n");
    return EXIT_FAILURE;
  }

  n = snprintf (filename, sizeof (filename),
                "%02d_%02d_%02d_%03d__%02d_%02d_%02d_%03d %s%zu.bmp",
                sub->start.h, sub->start.m, sub->start.s, sub->start.ms,
                sub->end.h, sub->end.m, sub->end.s, sub->end.ms,
                idx->id[lang], lang);
  if (n < 0 || (size_t) n >= sizeof (filename)) {
    fprintf (stderr, "Bitmap output filename is too long.\n");
    return EXIT_FAILURE;
  }

  fo = fopen (filename, "rb");
  if (fo != NULL) {
    fclose (fo);
    fprintf (stderr, "Output file %s already exists.\n", filename);
    return EXIT_FAILURE;
  }

  fo = fopen (filename, "wb");
  if (fo == NULL) {
    fprintf (stderr, "Unable to open output file %s.\n", filename);
    return EXIT_FAILURE;
  }

  width = sub->width;
  height = sub->height;
  if (width > INT32_MAX || height > INT32_MAX || width > (SIZE_MAX - 3) / 3) {
    fprintf (stderr, "Bitmap dimensions are too large.\n");
    fclose (fo);
    return EXIT_FAILURE;
  }

  row_size = (width * 3 + 3) & ~(size_t) 3;
  if (height > SIZE_MAX / row_size) {
    fprintf (stderr, "Bitmap size overflow.\n");
    fclose (fo);
    return EXIT_FAILURE;
  }
  image_size = row_size * height;
  if (image_size > UINT32_MAX - 54u) {
    fprintf (stderr, "Bitmap exceeds the 32-bit BMP file-size limit.\n");
    fclose (fo);
    return EXIT_FAILURE;
  }
  padding_size = row_size - width * 3;

  // BMP file header
  write_u16_le (fo, 0x4d42);                // File type, should be "BM"
  write_u32_le (fo, 54u + (uint32_t) image_size);  // Size of the file (bytes)
  write_u16_le (fo, 0);                     // Reserved (set to 0)
  write_u16_le (fo, 0);                     // Reserved (set to 0)
  write_u32_le (fo, 54);                    // Offset (bytes) to the start of the pixel data

  // BMP information header
  write_u32_le (fo, 40);                    // Size of this header (40 bytes)
  write_s32_le (fo, (int32_t) width);       // Width of the image (px)
  write_s32_le (fo, (int32_t) height);      // Height of the image (px)
  write_u16_le (fo, 1);                     // Number of color planes (always 1)
  write_u16_le (fo, 24);                    // Bits per pixel (24 for RGB)
  write_u32_le (fo, 0);                     // Compression method (0 for none)
  write_u32_le (fo, (uint32_t) image_size);  // Size of the image data (bytes)
  write_s32_le (fo, 7874);                  // Horizontal resolution (in pixels per meter) (200 DPI)
  write_s32_le (fo, 7874);                  // Vertical resolution (in pixels per meter) (200 DPI)
  write_u32_le (fo, 0);                     // Number of colors used (0 for 2^24)
  write_u32_le (fo, 0);                     // Important colors (0 for all)

  for (y = 0; y < height; y++) {
    flipped_y = height - 1 - y;
    for (x = 0; x < width; x++) {
      index = ((flipped_y * width) + x) * 4;
      r = data[index];
      g = data[index + 1];
      b = data[index + 2];
      a = data[index + 3];

      r = (uint8_t) (((uint16_t) r * a) / 255u);
      g = (uint8_t) (((uint16_t) g * a) / 255u);
      b = (uint8_t) (((uint16_t) b * a) / 255u);

      if (fputc (b, fo) == EOF || fputc (g, fo) == EOF || fputc (r, fo) == EOF) {
        fprintf (stderr, "Error writing bitmap %s.\n", filename);
        fclose (fo);
        return EXIT_FAILURE;
      }
    }

    if (padding_size != 0 && fwrite (padding, 1, padding_size, fo) != padding_size) {
      fprintf (stderr, "Error writing bitmap padding to %s.\n", filename);
      fclose (fo);
      return EXIT_FAILURE;
    }
  }

  if (fclose (fo) != 0) {
    fprintf (stderr, "Unable to finalize bitmap %s.\n", filename);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
