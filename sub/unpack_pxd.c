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

// Unpack RLE_encoded subpicture pixel data (PXD).
// Subpicture data is interleaved: Top Field are lines 0, 2, 4, etc. Bottom Field are line 1, 3, 5, etc.
// Line 0 is at top of screen.
int
unpack_pxd (uint8_t *spu_buffer, size_t spu_buffer_size, SPU_PARMS *spu_info, IDX *idx, uint8_t *buffer, SUB *sub) {

  size_t i, interlace, bitpos, x, y, width, height, pixel, color_idx, buffer_idx;
  RLE rle;

  // Subpicture Width (px)
  if ((spu_info->x_end - spu_info->x_start + 1) <= 0) {
    fprintf (stderr, "In valid horizontal start and/or end position in unpack_pxd().\n");
    fprintf (stderr, "x_start: %zu px, x_end: %zu px\n", spu_info->x_start, spu_info->x_end);
    exit (EXIT_FAILURE);
  }
  width = spu_info->x_end - spu_info->x_start + 1;  // +1 because includes both end points
  sub->width = width;

  // Subpicture Height (px)
  if ((spu_info->x_end - spu_info->x_start + 1) <= 0) {
    fprintf (stderr, "In valid vertical start and/or end position in unpack_pxd().\n");
    fprintf (stderr, "y_start: %zu px, y_end: %zu px\n", spu_info->y_start, spu_info->y_end);
    exit (EXIT_FAILURE);
  }
  height = spu_info->y_end - spu_info->y_start + 1;  // +1 because includes both end points
  sub->height = height;

  if (spu_info->pxd_tf == 0 || spu_info->pxd_bf == 0) {
    fprintf (stderr, "One or both pixel data field addresses are 0 in unpack_pxd().\n");
    exit (EXIT_FAILURE);
  }

  for (interlace = 0; interlace < 2; interlace++) {

    if ((spu_info->pxd_tf > spu_buffer_size) || (spu_info->pxd_bf > spu_buffer_size)) {
      fprintf (stderr, "One or both pixel data field addresses point outside SPU buffer in unpack_pxd().\n");
      exit (EXIT_FAILURE);
    }

    bitpos = (interlace == 0 ? spu_info->pxd_tf : spu_info->pxd_bf) * 8;
    y = interlace;

    while (y < height) {
      x = 0;

      while (x < width) {

        // Decode next field of RLE_encoded pixel data.
        decode_rle (spu_buffer, &bitpos, &rle);  // Runlength, color, and to_eol flag returned in rle struct.
        if (rle.to_eol) rle.runlength = width - x;  // Special case: same pixel type to end of line.

        // Color range check; color is 2 bits.
        if (rle.color > 3) {
          fprintf (stderr, "Invalid RLE color %u in unpack_pxd().\n", rle.color);
          exit (EXIT_FAILURE);
        }

        for (i = 0; (i < rle.runlength) && (x < width); i++, x++) {

          color_idx = (size_t) spu_info->clut[rle.color];

          pixel = (y * width) + x;
          if (pixel >= (width * height)) {
            fprintf (stderr, "pixel is outside of width * height bounds in unpack_pxd().\n");
            exit (EXIT_FAILURE);
          }
          buffer_idx = pixel * 4;

          // Gamma-correction could be applied here for sRGB bitmaps; we won't bother.

          // Store pixel color and Alpha.
          buffer[buffer_idx + 0] = idx->palette[color_idx].r;
          buffer[buffer_idx + 1] = idx->palette[color_idx].g;
          buffer[buffer_idx + 2] = idx->palette[color_idx].b;
          buffer[buffer_idx + 3] = spu_info->alpha[rle.color] * 17;  // Scale from 4-bit (0 to 15) to 8-bit (0 to 255)
        }
      }  // End while x < width

      // After a line is complete, always skip 4 fill bits, if they exist. See decode_rle() for more info.
      bitpos = (bitpos + 7) & ~7;

      y += 2;  // Next interlaced line

    }  // End while y < height
  }  // End for interlace

  return (EXIT_SUCCESS);
}
