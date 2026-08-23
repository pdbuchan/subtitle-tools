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

static int
get_bits (const uint8_t *buffer, size_t buffer_size, size_t *bitpos,
          unsigned int nbits, uint32_t *value) {

  unsigned int i;
  uint32_t v;

  if (nbits == 0 || nbits > 32 || *bitpos > buffer_size * 8 ||
      (size_t) nbits > buffer_size * 8 - *bitpos) {
    return (EXIT_FAILURE);
  }

  v = 0;
  for (i = 0; i < nbits; i++) {
    size_t pos = *bitpos + i;
    v = (v << 1) | ((buffer[pos / 8] >> (7 - (pos % 8))) & 1u);
  }

  *bitpos += nbits;
  *value = v;
  return (EXIT_SUCCESS);
}

static int
decode_rle_2bit (const uint8_t *buffer, size_t buffer_size, size_t *bitpos,
                 RLE *rle) {

  unsigned int threshold;
  uint32_t nibble, v;

  v = 0;
  for (threshold = 1; v < threshold && threshold <= 0x40; threshold <<= 2) {
    if (get_bits (buffer, buffer_size, bitpos, 4, &nibble) != EXIT_SUCCESS) {
      return (EXIT_FAILURE);
    }
    v = (v << 4) | nibble;
  }

  rle->color = (uint8_t) (v & 3u);
  if (v < 4) {
    rle->to_eol = 1;
    rle->runlength = 0;
  } else {
    rle->to_eol = 0;
    rle->runlength = (size_t) (v >> 2);
  }

  return (EXIT_SUCCESS);
}

static int
decode_rle_8bit (const uint8_t *buffer, size_t buffer_size, size_t *bitpos,
                 RLE *rle) {

  uint32_t has_run, wide_color, long_run, value;

  if (get_bits (buffer, buffer_size, bitpos, 1, &has_run) != EXIT_SUCCESS ||
      get_bits (buffer, buffer_size, bitpos, 1, &wide_color) != EXIT_SUCCESS ||
      get_bits (buffer, buffer_size, bitpos, wide_color ? 8u : 2u, &value) != EXIT_SUCCESS) {
    return (EXIT_FAILURE);
  }

  rle->color = (uint8_t) value;
  rle->to_eol = 0;

  if (!has_run) {
    rle->runlength = 1;
    return (EXIT_SUCCESS);
  }

  if (get_bits (buffer, buffer_size, bitpos, 1, &long_run) != EXIT_SUCCESS) {
    return (EXIT_FAILURE);
  }

  if (long_run) {
    if (get_bits (buffer, buffer_size, bitpos, 7, &value) != EXIT_SUCCESS) {
      return (EXIT_FAILURE);
    }
    if (value == 0) {
      rle->to_eol = 1;
      rle->runlength = 0;
    } else {
      rle->runlength = (size_t) value + 9;
    }
  } else {
    if (get_bits (buffer, buffer_size, bitpos, 3, &value) != EXIT_SUCCESS) {
      return (EXIT_FAILURE);
    }
    rle->runlength = (size_t) value + 2;
  }

  return (EXIT_SUCCESS);
}

int
decode_rle (const uint8_t *buffer, size_t buffer_size, size_t *bitpos,
            uint8_t is_8bit, RLE *rle) {

  if (buffer == NULL || bitpos == NULL || rle == NULL) return (EXIT_FAILURE);

  if (is_8bit) {
    return decode_rle_8bit (buffer, buffer_size, bitpos, rle);
  }

  return decode_rle_2bit (buffer, buffer_size, bitpos, rle);
}
