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

// Parse an 8-bit/pixel Code String
// Reference: ETSI EN 300 743
size_t
parse_eight_bit_code_string (STATE *state, SEGMENT *segment, size_t *bitpos, RLE *rle) {

  size_t bits_processed, run_length_1_127, run_length_3_127;
  uint8_t byte, nextbits, switch_1;

  bits_processed = 0;
  rle->runlength = 0;
  rle->color = 0;
  rle->emit_one_00_pixel = 0;
  rle->emit_two_00_pixels = 0;

  // Next 8 bits look-ahead
  get_8bits (state, segment, bitpos, &byte);
  nextbits = byte;
  (*bitpos) += 8;
  bits_processed += 8;

  // Single pixel (8 bits)
  if (nextbits != 0) {
    rle->runlength = 1;
    rle->color = nextbits;
    return (bits_processed);

  // Multi-byte RLE
  } else {

    // nextbits == 0b0000 0000 (8 bits)

    // switch_1 (1 bit)
    get_8bits (state, segment, bitpos, &byte);
    switch_1 = (byte >> 7) & 0x01;
    (*bitpos)++;
    bits_processed++;

    // Runlength 1-127 or End-of-String
    if (!switch_1) {

      // Next 7 bits look-ahead
      get_8bits (state, segment, bitpos, &byte);
      nextbits = (byte >> 1) & 0x7f;  // 0x7f = 0111 1111
      (*bitpos) += 7;
      bits_processed += 7;

      // Runlength 1-127 (7 bits)
      if (nextbits != 0) {
          run_length_1_127 = (size_t) nextbits;
          rle->runlength = run_length_1_127;
          rle->color = 0;

      // End of String Signal
      } else {
        rle->end_of_string_signal = 1;
      }

      return (bits_processed);

    } else {

      // Runlength 3-127 (7 bits)
      get_8bits (state, segment, bitpos, &byte);
      run_length_3_127 = (size_t) ((byte >> 1) & 0x7f);  // 0x7f = 0111 1111
      if (run_length_3_127 < 3) {
        fprintf (stderr, "run_length_3_127 < 3 in parse_eight_bit_code_string().\n");
        exit (EXIT_FAILURE);
      }
      rle->runlength = run_length_3_127;
      (*bitpos) += 7;
      bits_processed += 7;

      // Color (8 bits)
      get_8bits (state, segment, bitpos, &byte);
      rle->color = byte;
      (*bitpos) += 8;
      bits_processed += 8;
    }  // End if switch_1

    return (bits_processed);

  }  // End if single or multi-pixel RLE
}
