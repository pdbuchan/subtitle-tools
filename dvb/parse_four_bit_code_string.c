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

// Parse a 4-bit/pixel Code String
// Reference: ETSI EN 300 743
size_t
parse_four_bit_code_string (STATE *state, SEGMENT *segment, size_t *bitpos, RLE *rle) {

  size_t bits_processed, run_length_3_9, run_length_4_7, run_length_9_24, run_length_25_280;
  uint8_t byte, nextbits, switch_1, switch_2, switch_3;

  bits_processed = 0;
  rle->runlength = 0;
  rle->color = 0;
  rle->emit_one_00_pixel = 0;
  rle->emit_two_00_pixels = 0;

  // Next 4 bits look-ahead
  get_8bits (state, segment, bitpos, &byte);
  nextbits = (byte >> 4) & 0x0f;  // 0x0f = 0000 1111
  (*bitpos) += 4;
  bits_processed += 4;

  // Single pixel (4 bits)
  if (nextbits != 0) {
    rle->runlength = 1;
    rle->color = nextbits;
    return (bits_processed);

  // Multi-pixel RLE
  } else {

    // nextbits == 0b0000 (4 bits)

    // switch_1 (1 bit)
    get_8bits (state, segment, bitpos, &byte);
    switch_1 = (byte >> 7) & 0x01;
    (*bitpos)++;
    bits_processed++;

    // Either Runlength 3-9 or end-of-string.
    if (!switch_1) {

      // Next 3 bits look-ahead
      get_8bits (state, segment, bitpos, &byte);
      nextbits = (byte >> 5) & 0x07;  // 0x07 = 0000 0111
      (*bitpos) += 3;
      bits_processed += 3;

      // End of string signal (3 bits)
      // 0b000
      if (!nextbits) {
        rle->end_of_string_signal = 1;

      // Runlength 3-9 (3 bits)
      } else {

        // Runlength (3 bits)
        run_length_3_9 = (size_t) nextbits;
        rle->runlength = run_length_3_9 + 2;

        // Color (0 bits)
        rle->color = 0;  // Color index to be 0.
      }
      return (bits_processed);

    } else {

      // switch_2 (1 bit)
      get_8bits (state, segment, bitpos, &byte);
      switch_2 = (byte >> 7) & 0x01;
      (*bitpos)++;
      bits_processed++;

      if (!switch_2) {

        // Runlength 4-7 (2 bits)
        get_8bits (state, segment, bitpos, &byte);
        run_length_4_7 = (size_t) ((byte >> 6) & 0x03);
        rle->runlength = run_length_4_7 + 4;
        (*bitpos) += 2;
        bits_processed += 2;

        // Color (4 bits)
        get_8bits (state, segment, bitpos, &byte);
        rle->color = (byte >> 4) & 0x0f;  // 0x0f = 0000 1111
        (*bitpos) += 4;
        bits_processed += 4;
  
        return (bits_processed);

      } else {

        // switch_3 (2 bits)
        get_8bits (state, segment, bitpos, &byte);
        switch_3 = (byte >> 6) & 0x03;
        (*bitpos) += 2;
        bits_processed += 2;

        switch (switch_3) {

          case 0:  // Emit one pixel with color index 0.
            rle->emit_one_00_pixel = 1;
            break;

          case 1:  // Emit two pixels with color index 0.
            rle->emit_two_00_pixels = 1;
            break;

          case 2:  // Runlength 9-24 (4 bits) + color (4 bits)

            // Runlength 9-24 (4 bits)
            get_8bits (state, segment, bitpos, &byte);
            run_length_9_24 = (size_t) ((byte >> 4) & 0x0f);  // 0x0f = 0000 1111
            rle->runlength = run_length_9_24 + 9;
            (*bitpos) += 4;
            bits_processed += 4;

            // Color (4 bits)
            get_8bits (state, segment, bitpos, &byte);
            rle->color = (byte >> 4) & 0x0f;  // 0x0f = 0000 1111
            (*bitpos) += 4;
            bits_processed += 4;
            break;

          case 3:  // Runlength 25-280 (8 bits) + color (4 bits)

            // Runlength 25-280 (8 bits)
            get_8bits (state, segment, bitpos, &byte);
            run_length_25_280 = (size_t) byte;
            rle->runlength = run_length_25_280 + 25;
            (*bitpos) += 8;
            bits_processed += 8;

            // Color (4 bits)
            get_8bits (state, segment, bitpos, &byte);
            rle->color = (byte >> 4) & 0x0f;  // 0x0f = 0000 1111
            (*bitpos) += 4;
            bits_processed += 4;
            break;

          default:  // Should never happen.
            fprintf (stderr, "Invalid switch_3 value %u in parse_four_bit_code_string().\n", switch_3);
            exit (EXIT_FAILURE);

        }  // End switch
      }  // End if !switch2
    }  // End if !switch1
  }  // End if single or multi pixel RLE
  return (bits_processed);
}
