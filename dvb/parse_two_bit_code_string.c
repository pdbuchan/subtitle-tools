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

// Parse a 2-bit/pixel Code String.
// Reference: ETSI EN 300 743
size_t
parse_two_bit_code_string (STATE *state, SEGMENT *segment, size_t *bitpos, RLE *rle) {

  size_t bits_processed, run_length_3_10, run_length_12_27, run_length_29_284;
  uint8_t byte, nextbits, switch_1, switch_2, switch_3;

  bits_processed = 0;
  rle->runlength = 0;
  rle->color = 0;
  rle->emit_one_00_pixel = 0;
  rle->emit_two_00_pixels = 0;

  // Next 2 bits look-ahead
  get_8bits (state, segment, bitpos, &byte);
  nextbits = (byte >> 6) & 0x03;
  (*bitpos) += 2;
  bits_processed += 2;

  // Single pixel (2 bits)
  if (nextbits != 0) {
    rle->runlength = 1;
    rle->color = nextbits;

    return (bits_processed);

  // Multi-pixel RLE
  } else {

    // nextbits == 0b00 (2 bits)

    // switch_1 (1 bit)
    get_8bits (state, segment, bitpos, &byte);
    switch_1 = (byte >> 7) & 0x01;
    (*bitpos)++;
    bits_processed++;

    // Runlength 3-10 (3 bits) + color (2 bits)
    if (switch_1) {

      // Runlength 3-10 (3 bits)
      get_8bits (state, segment, bitpos, &byte);
      run_length_3_10 = (size_t) ((byte >> 5) & 0x07);  // 0x07 = 0000 0111
      rle->runlength = run_length_3_10 + 3;
      (*bitpos) += 3;
      bits_processed += 3;

      // Color (2 bits)
      get_8bits (state, segment, bitpos, &byte);
      rle->color = (byte >> 6) & 0x03;
      (*bitpos) += 2;
      bits_processed += 2;

      return (bits_processed);

    } else {

      // switch_2 (1 bit)
      get_8bits (state, segment, bitpos, &byte);
      switch_2 = (byte >> 7) & 0x01;
      (*bitpos)++;
      bits_processed++;

      // Emit one pixel of color index 0.
      if (switch_2) {
        rle->emit_one_00_pixel = 1;
        return (bits_processed);

      } else {

        // switch_3 (2 bits)
        get_8bits (state, segment, bitpos, &byte);
        switch_3 = (byte >> 6) & 0x03;
        (*bitpos) += 2;
        bits_processed += 2;

        switch (switch_3) {

          case 0:  // End of string signal
            rle->end_of_string_signal = 1;
            break;

          case 1:  // Emit two pixels with color index 0.
            rle->emit_two_00_pixels = 1;
            break;

          case 2:  // Runlength 12-27 (4 bits) + color (2 bits)

            // Runlength 12-27 (4 bits)
            get_8bits (state, segment, bitpos, &byte);
            run_length_12_27 = (size_t) ((byte >> 4) & 0x0f);  // 0x0f = 0000 1111
            rle->runlength = run_length_12_27 + 12;
            (*bitpos) += 4;
            bits_processed += 4;

            // Color (2 bits)
            get_8bits (state, segment, bitpos, &byte);
            rle->color = (byte >> 6) & 0x03;
            (*bitpos) += 2;
            bits_processed += 2;
            break;

          case 3:  // Runlength 29-284 (8 bits) + color (2 bits)

            // Runlength 29-284 (8 bits)
            get_8bits (state, segment, bitpos, &byte);
            run_length_29_284 = (size_t) byte;
            rle->runlength = run_length_29_284 + 29;
            (*bitpos) += 8;
            bits_processed += 8;

            // Color (2 bits)
            get_8bits (state, segment, bitpos, &byte);
            rle->color = (byte >> 6) & 0x03;
            (*bitpos) += 2;
            bits_processed += 2;
            break;

        }  // End switch
        return (bits_processed);

      }  // End if !switch_2
    }  // End is switch_1
  }  // End if single or multi-pixel RLE
}
