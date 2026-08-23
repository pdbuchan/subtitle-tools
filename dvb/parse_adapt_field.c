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

// Parse Adaptation Field
// Reference: ISO/IEC 13818-1
int
parse_adapt_field (STATE *state, size_t *index, uint8_t *tsdata, size_t tslen, FILE *fo) {

  // These parameters are retained for a consistent parser interface. Packet
  // bounds are established by parse_ts_packet() before this function is called.
  (void) state;
  (void) tslen;

  size_t i, adapt_field_len, adapt_field_ext_len, af_bytes_left, af_ext_bytes_left, transp_priv_data_len;
  uint8_t pcr_flag, opcr_flag, splicing_point_flag, transp_priv_data_flag, adapt_field_ext_flag, ltw_valid_flag, splice_type;
  uint8_t splice_countdown, ltw_flag, piecewise_rate_flag, seamless_splice_flag;
  uint16_t pcr_ext, opcr_ext, ltw_offset;
  uint64_t pcr_base, opcr_base, piecewise_rate;
  TIME dts_next_au;

  fprintf (fo, "  Adaptation Field:\n");

  // Adaptation Field length (1 byte)
  adapt_field_len = (size_t) tsdata[*index];
  af_bytes_left = adapt_field_len;  // To keep track of byte consumption as we parse Adaptation Field

  // Check to ensure sufficient bytes remaining.
  if (af_bytes_left < 1) {
    fprintf (stderr, "Not enough bytes remaining in Adaptation Field for Adaptation Field length.\n");
    fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
    exit (EXIT_FAILURE);
  }

  fprintf (fo, "    Adaptation Field length (1 byte): %zu bytes\n", adapt_field_len);
  (*index)++;

  // Check to ensure sufficient bytes remaining.
  if (af_bytes_left < 1) {
    fprintf (stderr, "Not enough bytes remaining in Adaptation Field for DI, RAI, ESPI, PCR, OPCR, SP, TPD, and AFE flags.\n");
    fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
    exit (EXIT_FAILURE);
  }

  // Discontinuity Indicator (1 bit)
  fprintf (fo, "    Discontinuity Indicator (1 bit): %u\n", (tsdata[*index] >> 7));

  // Random Access Indicator (1 bit)
  fprintf (fo, "    Random Access Indicator (1 bit): %u\n", (tsdata[*index] >> 6) & 1);

  // Elementary Stream Priority Indicator (1 bit)
  fprintf (fo, "    Elementary Stream Priority Indicator (1 bit): %u\n", (tsdata[*index] >> 5) & 1);

  // Program Clock Reference (PCR) Flag (1 bit)
  pcr_flag = (tsdata[*index] >> 4) & 1;
  fprintf (fo, "    Program Clock Reference (PCR) Flag (1 bit): %u\n", pcr_flag);

  // Original Program Clock Reference (OPCR) Flag (1 bit)
  opcr_flag = (tsdata[*index] >> 3) & 1;
  fprintf (fo, "    Original Program Reference Clock (OPCR) Flag (1 bit): %u\n", opcr_flag);

  // Splicing Point Flag (1 bit)
  splicing_point_flag = (tsdata[*index] >> 2) & 1;
  fprintf (fo, "    Splicing Point Flag (1 bit): %u\n", splicing_point_flag);

  // Transport Private Data Flag (1 bit)
  transp_priv_data_flag = (tsdata[*index] >> 1) & 1;
  fprintf (fo, "    Transport Private Data Flag (1 bit): %u\n", transp_priv_data_flag);

  // Adaptation Field Extension Flag (1 bit)
  adapt_field_ext_flag = tsdata[*index] & 1;
  fprintf (fo, "    Adaptation Field Extension Flag (1 bit): %u\n", adapt_field_ext_flag);

  (*index)++;
  af_bytes_left--;

  // Program Clock Reference (PCR) (6 bytes)
  if (pcr_flag) {

    // Check to ensure sufficient bytes remaining.
    if (af_bytes_left < 6) {
      fprintf (stderr, "Not enough bytes remaining in Adaptation Field for Program Clock Reference (PCR).\n");
      fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
      exit (EXIT_FAILURE);
    }

    // PCR Base (33 bits)
    pcr_base = ((uint64_t) tsdata[*index] << 25) |
               ((uint64_t) tsdata[*index + 1] << 17) |
               ((uint64_t) tsdata[*index + 2] << 9) |
               ((uint64_t) tsdata[*index + 3] << 1) |
               ((uint64_t) tsdata[*index + 4] >> 7);
    fprintf (fo, "    PCR Base (33 bits): %" PRIu64 "\n", pcr_base);

    // Reserved (6 bits)

    // PCR Extension (9 bits)
    pcr_ext = ((uint16_t) ((tsdata[*index + 4]) & 1) << 8) |
              ((uint16_t) tsdata[*index + 5]);
    fprintf (fo, "    PCR extension (9 bits): %" PRIu16 "\n", pcr_ext);

    (*index) += 6;
    af_bytes_left -= 6;
  }

  // Original Program Clock Reference (OPCR) (6 bytes)
  if (opcr_flag) {

    // Check to ensure sufficient bytes remaining.
    if (af_bytes_left < 6) {
      fprintf (stderr, "Not enough bytes remaining in Adaptation Field for Original Program Clock Reference (OPCR).\n");
      fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
      exit (EXIT_FAILURE);
    }

    // OPCR Base (33 bits)
    opcr_base = ((uint64_t) tsdata[*index] << 25) |  
               ((uint64_t) tsdata[*index + 1] << 17) |
               ((uint64_t) tsdata[*index + 2] << 9) |
               ((uint64_t) tsdata[*index + 3] << 1) |
               ((uint64_t) tsdata[*index + 4] >> 7);
    fprintf (fo, "    OPCR Base (33 bits): %" PRIu64 "\n", opcr_base);

    // Reserved (6 bits)

    // OPCR Extension (9 bits)
    opcr_ext = ((uint16_t) ((tsdata[*index + 4]) & 1) << 8) |
              ((uint16_t) tsdata[*index + 5]);
    fprintf (fo, "    OPCR extension (9 bits): %" PRIu16 "\n", opcr_ext);

    (*index) += 6;
    af_bytes_left -= 6;
  }

  // Splicing Point (1 byte)
  if (splicing_point_flag) {

    // Check to ensure sufficient bytes remaining.
    if (af_bytes_left < 1) {
      fprintf (stderr, "Not enough bytes remaining in Adaptation Field for Splicing Countdown.\n");
      fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
      exit (EXIT_FAILURE);
    }

    splice_countdown = tsdata[*index];
    fprintf (fo, "    Splice Countdown (1 byte): %u transport packets\n", splice_countdown);
    (*index)++;
    af_bytes_left--;
  }

  // Transport Private Data
  if (transp_priv_data_flag) {

    // Check to ensure sufficient bytes remaining.
    if (af_bytes_left < 1) {
      fprintf (stderr, "Not enough bytes remaining in Adaptation Field for Transport Private Data Length.\n");
      fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
      exit (EXIT_FAILURE);
    }

    // Transport Private Data Length (1 byte)
    transp_priv_data_len = (size_t) tsdata[*index];

    // Check to ensure sufficient bytes remaining.
    if (af_bytes_left < (1 + transp_priv_data_len)) {
      fprintf (stderr, "Not enough bytes remaining in Adaptation Field for Transport Private Data.\n");
      fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
      exit (EXIT_FAILURE);
    }

    fprintf (fo, "    Transport Private Data Length (1 byte): %zu bytes\n", transp_priv_data_len);
    (*index)++;
    af_bytes_left--;

    // Skip Transport private data.
    (*index) += transp_priv_data_len;
    af_bytes_left -= transp_priv_data_len;
  }

  // Adaptation Field Extension
  if (adapt_field_ext_flag) {

    // Check to ensure sufficient bytes remaining.
    if (af_bytes_left < 1) {
      fprintf (stderr, "Not enough bytes remaining in Adaptation Field for Adaptation Field Extension Length.\n");
      fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
      exit (EXIT_FAILURE);
    }

    fprintf (fo, "    Adaptation Field Extension\n");

    // Adaptation Field Extension Length (1 byte)
    adapt_field_ext_len = (size_t) tsdata[*index];

    // All AF extension subfields must fit within adaptation_field_extension_length.
    if (adapt_field_ext_len > af_bytes_left) {
       fprintf (stderr, "Adaptation Field Extension length exceeds remaining Adaptation Field bytes\n");
       exit (EXIT_FAILURE);
    }
    af_ext_bytes_left = adapt_field_ext_len;

    fprintf (fo, "      Adaptation Field Extension Length (1 byte): %zu bytes\n", adapt_field_ext_len);
    (*index)++;
    af_bytes_left--;

    // Check to ensure sufficient bytes remaining.
    if (af_bytes_left < 1) {
      fprintf (stderr, "Not enough bytes remaining in Adaptation Field Extension for LTW, PR, and SS flags.\n");
      fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
      exit (EXIT_FAILURE);
    }

    // Legal Time Window (LTW) Flag (1 bit)
    ltw_flag = tsdata[*index] >> 7;
    fprintf (fo, "      Legal Time Window (LTW) Flag (1 bit): %u\n", ltw_flag);

    // Piecewise Rate Flag (1 bit)
    piecewise_rate_flag = (tsdata[*index] >> 6) & 1;
    fprintf (fo, "      Piecewise Rate Flag (1 bit): %u\n", piecewise_rate_flag);

    // Seamless Splice Flag (1 bit)
    seamless_splice_flag = (tsdata[*index] >> 5) & 1;
    fprintf (fo, "      Seamless Splice Flag (1 bit): %u\n", seamless_splice_flag);

    // Reserved (5 bits)

    (*index)++;
    af_bytes_left--;
    af_ext_bytes_left--;

    // Legal Time Window (LTW) (2 bytes)
    if (ltw_flag) {

      if (af_ext_bytes_left < 2) {
        fprintf (stderr, "Not enough bytes remaining in Adaptation Field Extension for Legal Time Window (LTW).\n");
        fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
        exit (EXIT_FAILURE);
      }

      ltw_valid_flag = tsdata[*index] >> 7;
      fprintf (fo, "      Legal Time Window (LTW) Valid Flag (1 bit): %u\n", ltw_valid_flag);
      ltw_offset = (uint16_t) ((tsdata[*index] & 127) << 8) | (uint16_t) tsdata[*index + 1];  // 127 = 0111 1111
      fprintf (fo, "      Legal Time Window (LTW) Offset (15 bits): %" PRIu16 "\n", ltw_offset);
      (*index) += 2;
      af_bytes_left -= 2;
      af_ext_bytes_left -= 2;
    }

    // Piecewise Rate (3 bytes)
    if (piecewise_rate_flag) {

      if (af_ext_bytes_left < 3) {
        fprintf (stderr, "Not enough bytes remaining in Adaptation Field Extension for Piecewise Rate.\n");
        fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
        exit (EXIT_FAILURE);
      }

      // Reserved (2 bits)

      // Piecewise Rate (22 bits)
      piecewise_rate = (uint64_t) ((tsdata[*index] & 63) << 16) |  // 63 = 0011 1111
                       (uint64_t) (tsdata[*index + 1] << 8) |
                       (uint64_t) tsdata[*index + 2];
      fprintf (fo, "      Piecewise Rate (3 bytes): %" PRIu64 "\n", piecewise_rate);
      (*index) += 3;
      af_bytes_left -= 3;
      af_ext_bytes_left -= 3;
    }

    // Seamless Splice (5 bytes)
    if (seamless_splice_flag) {

      if (af_ext_bytes_left < 5) {
        fprintf (stderr, "Not enough bytes remaining in Adaptation Field Extension for Seamless Splice.\n");
        fprintf (stderr, "Only %zu bytes remaining.\n", af_bytes_left);
        exit (EXIT_FAILURE);
      }

      fprintf (fo, "    Seamless Splice\n");

      // Splice Type (4 bits)
      splice_type = tsdata[*index] >> 4;
      fprintf (fo, "      Splice Type (4 bits): ");
      for (i = 0; i < 4; i++) {
        fprintf (fo, "%u", splice_type >> (3 - i));
        if (i == 1) fprintf (fo, " ");
      }
      fprintf (fo, "\n");

      // Decoding Time Stamp (DTS) Next Access Unit (33 bits)
      dts_next_au.totalms =
        ((uint64_t)(tsdata[*index]  & 0x0e)) << 29 |       // Bits 32..30; 0x0e = 1110
        ((uint64_t) tsdata[(*index) + 1]) << 22 |          // Bits 29..22
        ((uint64_t)(tsdata[(*index) + 2] & 0xfe)) << 14 |  // Bits 21..15; 0xfe = 1111 1110
        ((uint64_t) tsdata[(*index) + 3]) << 7 |           // Bits 14..7
        ((uint64_t)(tsdata[(*index) + 4] & 0xfe)) >> 1;    // Bits 6..0

      // Convert to ms via integer math.
      dts_next_au.totalms = (dts_next_au.totalms + 45) / 90;

      mstotime (&dts_next_au);
      fprintf (fo, "      Decoding Time Stamp (DTS) Next Access Unit (33 bits): %02i:%02i:%02i,%03i  totalms: %" PRIu64 " ms\n", dts_next_au.h, dts_next_au.m, dts_next_au.s, dts_next_au.ms, dts_next_au.totalms);

      // Reserved; Ignore and assume all remaining bytes are stuffing.

      (*index) += 5;
      af_bytes_left -= 5;
      af_ext_bytes_left -= 5;
    }

  }  // End if adaptation field extension flag

  // Adaptation Field Stuffing Bytes
  (*index) += af_bytes_left;
  af_bytes_left = 0;

  return (EXIT_SUCCESS);
}
