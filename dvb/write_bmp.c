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

// Write one completed Display Set composition as an uncompressed 24-bit BMP.
// The input buffer contains RGBA pixels; BMP stores rows bottom-to-top and
// stores each pixel in BGR order.
int
write_bmp (STATE *s, PAGE *page, size_t p, uint8_t *buf) {

  size_t w = page[p].width, h = page[p].height, x, y, fy, row, img, idx, padn;
  uint8_t r, g, b, a, pad[3] = {0};
  char name[MAX_STRINGLEN];
  int n;
  FILE *fo;
  (void) s;

  // Validate dimensions before computing the padded BMP row size and total
  // image size. BMP dimensions are signed 32-bit values and file sizes are
  // represented by unsigned 32-bit fields in this header format.
  if (!w || !h || w > (size_t) INT32_MAX || h > (size_t) INT32_MAX || w > (SIZE_MAX - 3) / 3) return (EXIT_FAILURE);
  row = (w * 3 + 3) & ~(size_t) 3;
  if (h > SIZE_MAX / row) return (EXIT_FAILURE);
  img = row * h;
  if (img > UINT32_MAX - 54U) return (EXIT_FAILURE);

  // Filename is the start and end timestamps of the subtitle.
  n = snprintf (name, sizeof (name), "%02d_%02d_%02d_%03d__%02d_%02d_%02d_%03d.bmp", page[p].start.h, page[p].start.m, page[p].start.s, page[p].start.ms, page[p].end.h, page[p].end.m, page[p].end.s, page[p].end.ms);
  if (n < 0 || (size_t) n >= sizeof (name)) return (EXIT_FAILURE);

  // Do not silently overwrite an existing bitmap.
  fo = fopen (name, "rb");
  if (fo) {
    fclose (fo);
    fprintf (stderr, "Output file %s already exists.\n", name);
    return (EXIT_FAILURE);
  }
  fo = fopen (name, "wb");
  if (!fo) return (EXIT_FAILURE);

  // BMP file header
  write_u16_le (fo, 0x4d42);                // File type, should be "BM"
  write_u32_le (fo, 54u + (uint32_t) img);  // Size of the file (bytes)
  write_u16_le (fo, 0);                     // Reserved (set to 0)
  write_u16_le (fo, 0);                     // Reserved (set to 0)
  write_u32_le (fo, 54);                    // Offset (bytes) to the start of the pixel data

  // BMP information header
  write_u32_le (fo, 40);                    // Size of this header (40 bytes)
  write_s32_le (fo, (int32_t) w);           // Width of the image (px)
  write_s32_le (fo, (int32_t) h);           // Height of the image (px)
  write_u16_le (fo, 1);                     // Number of color planes (always 1)
  write_u16_le (fo, 24);                    // Bits per pixel (24 for RGB)
  write_u32_le (fo, 0);                     // Compression method (0 for none)
  write_u32_le (fo, (uint32_t) img);        // Size of the image data (bytes)
  write_s32_le (fo, 7874);                  // Horizontal resolution (in pixels per meter) (200 DPI)
  write_s32_le (fo, 7874);                  // Vertical resolution (in pixels per meter) (200 DPI)
  write_u32_le (fo, 0);                     // Number of colors used (0 for 2^24)
  write_u32_le (fo, 0);                     // Important colors (0 for all)

  // Each BMP scan line is padded to a 4-byte boundary.
  padn = row - w * 3;
  for (y = 0; y < h; y++) {

    // Positive BMP height means the file stores rows from bottom to top.
    fy = h - 1 - y;
    for (x = 0; x < w; x++) {
      idx = (fy * w + x) * 4;
      r = buf[idx];
      g = buf[idx + 1];
      b = buf[idx + 2];
      a = buf[idx + 3];

      // This program writes a 24-bit BMP with no alpha channel. Apply the
      // subtitle alpha value as intensity against black before discarding it.
      // Internally alpha is 255 = opaque and 0 = transparent.
      r = (uint8_t) (((uint16_t) r * a) / 255U);
      g = (uint8_t) (((uint16_t) g * a) / 255U);
      b = (uint8_t) (((uint16_t) b * a) / 255U);

      // RGB bitmaps are written in BGR byte order.
      if (fputc (b, fo) == EOF || fputc (g, fo) == EOF || fputc (r, fo) == EOF) {
        fclose (fo);
        return (EXIT_FAILURE);
      }
    }

    if (padn && fwrite (pad, 1, padn, fo) != padn) {
      fclose (fo);
      return (EXIT_FAILURE);
    }
  }

  return (fclose (fo) == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
