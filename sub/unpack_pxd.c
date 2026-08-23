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

static const COLCON_ENTRY *
find_colcon (const SPU_PARMS *spu, size_t x, size_t y) {

  size_t i;
  const COLCON_ENTRY *best;

  best = NULL;
  for (i = 0; i < spu->n_colcon; i++) {
    const COLCON_ENTRY *entry = &spu->colcon[i];
    if (y >= entry->y_start && y <= entry->y_end && x >= entry->x_start) {
      if (best == NULL || entry->x_start >= best->x_start) best = entry;
    }
  }

  return best;
}

int
unpack_pxd (const uint8_t *spu_buffer, size_t spu_buffer_size,
            SPU_PARMS *spu_info, IDX *idx, uint8_t **buffer, SUB *sub) {

  size_t i, interlace, bitpos, x, y, width, height, pixel_count;
  size_t pixel, buffer_idx, color_idx, x_abs, y_abs;
  uint8_t *image;
  uint8_t map[4], alpha4[4];
  const COLCON_ENTRY *colcon;
  RGB rgb;
  RLE rle;

  if (spu_info->x_end < spu_info->x_start) {
    fprintf (stderr, "Invalid horizontal display area in unpack_pxd().\n");
    return (EXIT_FAILURE);
  }
  if (spu_info->y_end < spu_info->y_start) {
    fprintf (stderr, "Invalid vertical display area in unpack_pxd().\n");
    return (EXIT_FAILURE);
  }

  width = spu_info->x_end - spu_info->x_start + 1;
  height = spu_info->y_end - spu_info->y_start + 1;
  if (width == 0 || height == 0 || width > SIZE_MAX / height) {
    fprintf (stderr, "Invalid or overflowing subtitle dimensions.\n");
    return (EXIT_FAILURE);
  }

  pixel_count = width * height;
  if (pixel_count > SIZE_MAX / 4) {
    fprintf (stderr, "Subtitle image allocation would overflow.\n");
    return (EXIT_FAILURE);
  }

  if (spu_info->pxd_tf >= spu_buffer_size || spu_info->pxd_bf >= spu_buffer_size) {
    fprintf (stderr, "Pixel data address points outside SPU buffer.\n");
    return (EXIT_FAILURE);
  }

  if (spu_info->is_8bit && !spu_info->has_hd_palette) {
    fprintf (stderr, "Cannot decode 8-bit pixel data without an HD palette.\n");
    return (EXIT_FAILURE);
  }
  if (!spu_info->is_8bit && idx->n_palette == 0) {
    fprintf (stderr, "Cannot decode classic VobSub pixels without an IDX palette.\n");
    return (EXIT_FAILURE);
  }

  image = calloc (pixel_count, 4);
  if (image == NULL) {
    fprintf (stderr, "Cannot allocate %zu-byte subtitle image buffer.\n", pixel_count * 4);
    return (EXIT_FAILURE);
  }

  sub->width = width;
  sub->height = height;

  for (interlace = 0; interlace < 2; interlace++) {
    bitpos = (interlace == 0 ? spu_info->pxd_tf : spu_info->pxd_bf) * 8;
    y = interlace;

    while (y < height) {
      x = 0;

      while (x < width) {
        if (decode_rle (spu_buffer, spu_buffer_size, &bitpos,
                        spu_info->is_8bit, &rle) != EXIT_SUCCESS) {
          fprintf (stderr, "Truncated or invalid RLE data at line %zu, column %zu.\n", y, x);
          free (image);
          return (EXIT_FAILURE);
        }

        if (rle.to_eol) {
          rle.runlength = width - x;
        } else if (rle.runlength == 0 || rle.runlength > width - x) {
          fprintf (stderr, "RLE run of %zu pixels exceeds remaining line width %zu.\n",
                   rle.runlength, width - x);
          free (image);
          return (EXIT_FAILURE);
        }

        for (i = 0; i < rle.runlength; i++, x++) {
          pixel = y * width + x;
          buffer_idx = pixel * 4;

          if (spu_info->is_8bit) {
            rgb = spu_info->hd_palette[rle.color];
            image[buffer_idx + 0] = rgb.r;
            image[buffer_idx + 1] = rgb.g;
            image[buffer_idx + 2] = rgb.b;
            image[buffer_idx + 3] = spu_info->alpha[rle.color];

          } else {
            if (rle.color > 3) {
              fprintf (stderr, "Invalid classic RLE color %u.\n", rle.color);
              free (image);
              return (EXIT_FAILURE);
            }

            memcpy (map, spu_info->clut, sizeof (map));
            memcpy (alpha4, spu_info->alpha, sizeof (alpha4));
            x_abs = spu_info->x_start + x;
            y_abs = spu_info->y_start + y;
            colcon = find_colcon (spu_info, x_abs, y_abs);
            if (colcon != NULL) {
              memcpy (map, colcon->clut, sizeof (map));
              memcpy (alpha4, colcon->alpha, sizeof (alpha4));
            }

            color_idx = map[rle.color];
            if (color_idx >= idx->n_palette) {
              fprintf (stderr, "Palette index %zu is outside the IDX palette.\n", color_idx);
              free (image);
              return (EXIT_FAILURE);
            }

            image[buffer_idx + 0] = idx->palette[color_idx].r;
            image[buffer_idx + 1] = idx->palette[color_idx].g;
            image[buffer_idx + 2] = idx->palette[color_idx].b;
            image[buffer_idx + 3] = alpha4[rle.color];
          }
        }
      }

      // Each encoded field line is padded to the next byte boundary.
      if (bitpos > SIZE_MAX - 7) {
        fprintf (stderr, "RLE bit position overflow.\n");
        free (image);
        return (EXIT_FAILURE);
      }
      bitpos = (bitpos + 7) & ~(size_t) 7;
      if (bitpos > spu_buffer_size * 8) {
        fprintf (stderr, "RLE line alignment passes the end of the SPU.\n");
        free (image);
        return (EXIT_FAILURE);
      }

      y += 2;
    }
  }

  free (*buffer);
  *buffer = image;
  return (EXIT_SUCCESS);
}
