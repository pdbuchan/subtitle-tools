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

// Decode RLE-encoded image object to red, green, blue, and alpha (RGBA) per pixel,
// and save in current subtitle's image buffer.
int
decode_rle (STATE *state, PALETTE *palette, uint8_t *rle_data, size_t rle_data_len, uint8_t *sub_buffer) {

  size_t rle_data_indx, sub_buf_indx, i, l;

  // Decode RLE image data.
  // CCCCCCCC                                One pixel of color C (1 byte) (1 <= C <= 255)
  // 00000000  00LLLLLL                      L pixels of color 0 (1 <= L <= 63)
  // 00000000  01LLLLLL  LLLLLLLL            L pixels of color 0 (64 <= L <= 16383)
  // 00000000  10LLLLLL  CCCCCCCC            L pixels of color C (3 <= L <= 63), (1 <= C <= 255)
  // 00000000  11LLLLLL  LLLLLLLL  CCCCCCCC  L pixels of color C (64 <= L >= 16383), (1 <= C <= 255)
  // 00000000  00000000                      End of Line

  // Note that bitmaps are drawn with origin at lower left,
  // whereas these images have origin at top-left; write_bmp() will flip image.
  rle_data_indx = 0;  // Index of RLE-compressed image data
  sub_buf_indx = 0;  // Index of buffer containing decoded data

  while (rle_data_indx < rle_data_len) {

    // One pixel of color C
    if (rle_data_indx >= rle_data_len) {
      fprintf (stderr, "Unexpectedly reached end of rle-encoded data in decode_rle().\n");
      exit (EXIT_FAILURE);
    }
    if (rle_data[rle_data_indx] != 0u) {
      sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].r;
      sub_buf_indx++;
      sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].g;
      sub_buf_indx++;
      sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].b;
      sub_buf_indx++;
      sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].alpha;
      sub_buf_indx++;
      rle_data_indx++;

    // Have multi-byte RLE pixels, or EOL.
    } else {

      rle_data_indx++;

      // End of line (EOL)
      // Do nothing.
      if (rle_data[rle_data_indx] == 0u) {
        rle_data_indx++;

      // L pixels of color 0 (1 <= L >= 63)
      } else if ((rle_data[rle_data_indx] >> 6) == 0u) {
        if (rle_data_indx >= rle_data_len) {
          fprintf (stderr, "Unexpectedly reached end of rle-encoded data in decode_rle().\n");
          exit (EXIT_FAILURE);
        }
        l = rle_data[rle_data_indx];  // Upper 2 bits are 0; no need to mask.
        rle_data_indx++;
        for (i = 0; i < l; i++) {
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[0].r;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[0].g;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[0].b;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[0].alpha;
          sub_buf_indx++;
        }

      // L pixels of color 0 (64 <= L >= 16383)
      // Use 255 in order to make background black.
      } else if ((rle_data[rle_data_indx] >> 6) == 1u) {
        if ((rle_data_indx + 1) >= rle_data_len) {
          fprintf (stderr, "Unexpectedly reached end of rle-encoded data in decode_rle().\n");
          exit (EXIT_FAILURE);
        }
        l = ((rle_data[rle_data_indx] & 63u) << 8) | rle_data[rle_data_indx + 1];  // mask 63 = 00111111
        rle_data_indx += 2;
        for (i = 0; i < l; i++) {
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[0].r;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[0].g;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[0].b;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[0].alpha;
          sub_buf_indx++;
        }

      // L pixels of color C (3 <= L >= 63)
      // Use greyscale. i.e., R=G=B
      // Subtract from 255 to make background black.
      } else if ((rle_data[rle_data_indx] >> 6) == 2u) {
        if (rle_data_indx >= rle_data_len) {
          fprintf (stderr, "Unexpectedly reached end of rle-encoded data in decode_rle().\n");
          exit (EXIT_FAILURE);
        }
        l = (rle_data[rle_data_indx] & 63u);
        rle_data_indx++;
        for (i = 0; i < l; i++) {
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].r;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].g;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].b;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].alpha;
          sub_buf_indx++;
        }
        rle_data_indx++;

      // L pixels of color C (64 <= L >= 16383)
      // Use greyscale. i.e., R=G=B
      // Subtract from 255 to make background black.
      } else if ((rle_data[rle_data_indx] >> 6) == 3u) {
        if ((rle_data_indx + 1) >= rle_data_len) {
          fprintf (stderr, "Unexpectedly reached end of rle-encoded data in decode_rle().\n");
          exit (EXIT_FAILURE);
        }
        l = ((rle_data[rle_data_indx] & 63u) << 8) | rle_data[rle_data_indx + 1];  // mask 63 = 00111111
        rle_data_indx += 2;
        for (i = 0; i < l; i++) {
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].r;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].g;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].b;
          sub_buf_indx++;
          sub_buffer[sub_buf_indx] = palette[state->current_palette].entry[rle_data[rle_data_indx]].alpha;
          sub_buf_indx++;
        }
        rle_data_indx++;
      } else {
        fprintf (stderr, "RLE decoding failed in decode_rle().\n");
        exit (EXIT_FAILURE);
      }
    }
  }  // End while

  return (EXIT_SUCCESS);
}
