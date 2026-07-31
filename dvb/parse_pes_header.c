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

// Parse an Packetized Elementary Stream (PES) packet header.
// Reference: ISO/IEC 13818-1
int
parse_pes_header (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, PES *pes, FILE *fo) {

  size_t i, pes_hdr_data_len, orig_stuff_len, ext2_len;
  uint8_t scrambling_ctrl, pes_priority, alignment_indicator, copyright, copy_orig, ptsdts_flag, escr_flag, esrate_flag, dsmtrickmode_flag, additional_copy_info_flag, crc_flag, pes_extension_flag;
  uint8_t private_data_flag, pack_header_field_length_flag, seq_counter_flag, pstd_buffer_flag, pes_extension2_flag, seq_counter_val, mpeg1or2, pstd_buffer_scale;
  uint16_t pid, crc, escr_ext;
  uint32_t esrate_raw, esrate, kbps, pstd_buffer_size;
  uint64_t escr_27mhz, escr_base, escr_ms, pts_ticks, pts_ms, dts_ticks, dts_ms;

  pid = state->pid;

  fprintf (fo, "\n  PACKETIZED ELEMENTARY STREAM (PES) HEADER\n");

  // Start Code (3 bytes)
  if (((*offset) + 2) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
    exit (EXIT_FAILURE);
  }
  fprintf (fo, "    Start Code (3 bytes): %02x %02x %02x\n", segment[pid].buffer[*offset], segment[pid].buffer[(*offset) + 1], segment[pid].buffer[(*offset) + 2]);
  (*offset) += 3;

  // Stream ID (1 byte);
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
    exit (EXIT_FAILURE);
  }
  state->stream_id = segment[pid].buffer[*offset];
  stream_ids (state, fo);  // Will print appropriate description of Stream ID to output file.
  (*offset)++;

  // PES Packet Length (2 bytes);
  if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
    exit (EXIT_FAILURE);
  }
  pes->packet_length = (size_t) ((segment[pid].buffer[*offset] << 8) | segment[pid].buffer[(*offset) + 1]);
  (*offset) += 2;
  fprintf (fo, "    PES packet length (2 bytes): %zu bytes\n", pes->packet_length);

  // Packetized Elementary Stream (PES) Header Extension
  fprintf (fo, "\n  PACKETIZED ELEMENTARY STREAM (PES) HEADER EXTENSION\n");

  // PES scrambling control, priority, data alignment, copyright, original/copy (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
    exit (EXIT_FAILURE);
  }
  if ((segment[pid].buffer[*offset] >> 6) != 2) {
    fprintf (stderr, "First byte 0x%02x of PES Header Extension doesn't have 2 MSBs as binary 10\n", segment[pid].buffer[*offset] >> 6);
    exit (EXIT_FAILURE);
  }

  // PES scrambling control (2 bits)
  scrambling_ctrl = (segment[pid].buffer[*offset] >> 4) & 3;
  if (!scrambling_ctrl) {
    fprintf (fo, "    PES Scrambling Control (2 bits): 0x%02x (not scrambled)\n", scrambling_ctrl);
  } else {
    fprintf (fo, "    PES Scrambling Control (2 bits): 0x%02x (user defined)\n", scrambling_ctrl);
  }

  // PES priority (1 bit)
  pes_priority = (segment[pid].buffer[*offset] >> 3) & 1;
  fprintf (fo, "    PES priority (1 bit): %u\n", pes_priority);

  // Data alignment indicator (1 bit)
  alignment_indicator = (segment[pid].buffer[*offset] >> 2) & 1;
  if (!alignment_indicator) {
    fprintf (fo, "    Data alignment indicator (1 bit): %u\n", alignment_indicator);
  } else {
    fprintf (fo, "    Data alignment indicator (1 bit): %u (video start code or audio syncword starts immediately after PES packet header)\n", alignment_indicator);
  }

  // Copyright indicator (1 bit)
  copyright = (segment[pid].buffer[*offset] >> 1) & 1;
  if (!copyright) {
    fprintf (fo, "    Copyright (1 bit): %u (not copyrighted)\n", copyright);
  } else {
    fprintf (fo, "    Copyright (1 bit): %u (copyrighted)\n", copyright);
  }

  // Original or Copy indicator (1 bit)
  copy_orig = segment[pid].buffer[*offset] & 1;
  if (copy_orig) {
    fprintf (fo, "    Copy/Original (1 bit): %u (original)\n", copy_orig);
  } else {
    fprintf (fo, "    Copy/Original (1 bit): %u (copy)\n", copy_orig);
  }

  (*offset)++;

  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
    exit (EXIT_FAILURE);
  }

  // Presentation Time Stamp (PTS) / Decode Time Stamp (DTS) flag (2 bits)
  ptsdts_flag = segment[pid].buffer[*offset] >> 6;
  switch (ptsdts_flag) {

    case 0:
      fprintf (fo, "    PTS/DTS flag (2 bits): %u (no PTS or DTS data)\n", ptsdts_flag);
      break;

    case 2:
      fprintf (fo, "    PTS/DTS flag (2 bits): %u (PTS only)\n", ptsdts_flag);
      break;

    case 3:
      fprintf (fo, "    PTS/DTS flag (2 bits): %u (PTS and DTS)\n", ptsdts_flag);
      break;

    default:
      fprintf (stderr, "Invalid PTS/DTS flag value of 1 in PES Header Extension\n");
      exit (EXIT_FAILURE);

  }  // End switch

  // Elementary Stream Clock Reference (ESCR) flag (1 bit)
  escr_flag = (segment[pid].buffer[*offset] >> 5) & 1;
  if (!escr_flag) {
    fprintf (fo, "    Elementary Stream Clock Reference (ESCR) flag (1 bit): %u (no ESCR reference included)\n", escr_flag);
  } else {
    fprintf (fo, "    Elementary Stream Clock Reference (ESCR) flag (1 bit): %u (ESCR reference included)\n", escr_flag);
  }

  // Elementary Stream Rate flag (1 bit)
  esrate_flag = (segment[pid].buffer[*offset] >> 4) & 1;
  if (!esrate_flag) {
    fprintf (fo, "    Elementary Stream Rate flag (1 bit): %u (no ES rate provided)\n", esrate_flag);
  } else {
    fprintf (fo, "    Elementary Stream Rate flag (1 bit): %u (ES rate provided)\n", esrate_flag);
  }

  // DSM Trick Mode flag (1 bit)
  dsmtrickmode_flag = (segment[pid].buffer[*offset] >> 3) & 1;
  fprintf (fo, "    DSM Trick Mode flag (1 bit): %u\n", dsmtrickmode_flag);

  // Additional Copy Info flag (1 bit)
  additional_copy_info_flag = (segment[pid].buffer[*offset] >> 2) & 1;
  if (!additional_copy_info_flag) {
    fprintf (fo, "    Additional Copy Info flag (1 bit): %u (no additional copy info included)\n", additional_copy_info_flag);
  } else {
    fprintf (fo, "    Additional Copy Info flag (1 bit): %u (additional copy info included)\n", additional_copy_info_flag);
  }

  // CRC flag (1 bit)
  crc_flag = (segment[pid].buffer[*offset] >> 1) & 1;
  if (!crc_flag) {
    fprintf (fo, "    CRC flag (1 bit): %u (previous PES packet CRC not included)\n", crc_flag);
  } else {
    fprintf (fo, "    CRC flag (1 bit): %u (previous PES packet CRC included)\n", crc_flag);
  }

  // PES Extension flag (1 bit)
  pes_extension_flag = segment[pid].buffer[*offset] & 1;
  if (!pes_extension_flag) {
    fprintf (fo, "    PES Extension flag (1 bit): %u (no further PES Header extension)\n", pes_extension_flag);
  } else {
    fprintf (fo, "    PES Extension flag (1 bit): %u (further PES Header extension)\n", pes_extension_flag);
  }
  (*offset)++;

  // PES Header Data Length (1 byte)
  if ((*offset) >= (MAX_BUFFERLEN + 1)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
    exit (EXIT_FAILURE);
  }
  pes->hdr_data_len = (size_t) segment[pid].buffer[*offset];
  pes_hdr_data_len = pes->hdr_data_len;
  fprintf (fo, "    PES header data length (1 byte): %zu bytes\n", pes->hdr_data_len);
  (*offset)++;

  // Process PES flags.

  // Presentation Timestamp (PTS) only (33 bits)
  if ((ptsdts_flag == 2) || (ptsdts_flag == 3)) {  // In either case we need PTS.

    // Extract PTS.
    if (((*offset) + 4) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
    pts_ticks =
        ((int64_t)(segment[pid].buffer[*offset]     & 0x0e) << 29) |  // 0x0e = 1110
        ((int64_t)(segment[pid].buffer[(*offset) + 1]) << 22) |
        ((int64_t)(segment[pid].buffer[(*offset) + 2] & 0xfe) << 14) |  // 0xfe = 1111 1110
        ((int64_t)(segment[pid].buffer[(*offset) + 3]) << 7) |
        ((int64_t)(segment[pid].buffer[(*offset) + 4] & 0xfe) >> 1);

    // Convert to ms via integer math.
    pts_ms = (pts_ticks + 45) / 90;

    pes->pts.totalms = (int64_t) pts_ms;
    mstotime (&pes->pts);
    pes->dts.totalms = 0;  // Dummy value
    fprintf (fo, "    PTS: %02d:%02d:%02d,%03d totalms: %" PRId64 "\n", pes->pts.h, pes->pts.m, pes->pts.s, pes->pts.ms, pes->pts.totalms);
    (*offset) += 5;  // Move past PTS data.
    pes_hdr_data_len -= 5;
    if (pes_hdr_data_len < 0) {
      fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
  }

  // PTS and DTS (33 bits each)
  // Note that we have already extracted PTS.
  if (ptsdts_flag == 3) {  // 3 = 11 in binary

    // Extract Decoding Timestamp (DTS).
    if (((*offset) + 4) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
    dts_ticks =
      ((int64_t)(segment[pid].buffer[*offset]  & 0x0e)) << 29 |       // Bits 32..30; 0x0e = 1110
      ((int64_t) segment[pid].buffer[(*offset) + 1]) << 22 |          // Bits 29..22
      ((int64_t)(segment[pid].buffer[(*offset) + 2] & 0xfe)) << 14 |  // Bits 21..15; 0xfe = 1111 1110
      ((int64_t) segment[pid].buffer[(*offset) + 3]) << 7 |           // Bits 14..7
      ((int64_t)(segment[pid].buffer[(*offset) + 4] & 0xfe)) >> 1;    // Bits 6..0

    // Convert to ms via integer math.
    dts_ms = (dts_ticks + 45) / 90;

    pes->dts.totalms = (int64_t) dts_ms;
    mstotime (&pes->dts);
    fprintf (fo, "    DTS: %02d:%02d:%02d,%03d totalms: %" PRId64 "\n", pes->dts.h, pes->dts.m, pes->dts.s, pes->dts.ms, pes->dts.totalms);
    (*offset) += 5;
    pes_hdr_data_len -= 5;
    if (pes_hdr_data_len < 0) {
      fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
  }

  // Elementary Stream Clock Reference (ESCR) (6 bytes)
  if (escr_flag) {

    // Elementary Stream Clock Reference (ESCR) Base (33 bits)
    if (((*offset) + 5) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
    escr_base =
      ((uint64_t) (segment[pid].buffer[*offset]       & 0x38) << 27) |  // base[32..30]; 0x38 = 0011 1000
      ((uint64_t) (segment[pid].buffer[*offset]       & 0x03) << 28) |  // base[29..28]; 0x03 = 0000 0011
      ((uint64_t) (segment[pid].buffer[(*offset) + 1])        << 20) |  // base[27..20]
      ((uint64_t) (segment[pid].buffer[(*offset) + 2] & 0xf8) << 12) |  // base[19..15]; 0xf8 = 1111 1000
      ((uint64_t) (segment[pid].buffer[(*offset) + 2] & 0x03) << 13) |  // base[14..13]
      ((uint64_t) (segment[pid].buffer[(*offset) + 3])        << 5)  |  // base[12..5]
      ((uint64_t) (segment[pid].buffer[(*offset) + 4])        >> 3);    // base[4..0]

    // Elementary Stream Clock Reference Extension (ESCR_ext) (9 bits)
    escr_ext =
      ((segment[pid].buffer[(*offset) + 4] & 0x03) << 7) |
      ((segment[pid].buffer[(*offset) + 5] & 0xfe) >> 1);  // 0xfe = 1111 1110

    // ESCR base ticks at 90 kHz.
    // 1 base tick = 300 × 27 MHz ticks
    // 27,000 ticks = 1 ms
    escr_27mhz = escr_base * 300 + escr_ext;
    escr_ms = (escr_27mhz + 13500) / 27000;  // Rounded
    fprintf (fo, "    Elementary Stream Clock Reference (ESCR) (6 bytes): %" PRIu64 " ms\n", escr_ms);

    (*offset) += 6;
    pes_hdr_data_len -= 6;
    if (pes_hdr_data_len < 0) {
      fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }

  }  // End if ptsdts_flag

  // Elementary Stream Rate (3 bytes)
  if (esrate_flag) {

    if (((*offset) + 2) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
    esrate_raw =
      ((uint32_t)segment[pid].buffer[*offset]     << 16) |
      ((uint32_t)segment[pid].buffer[(*offset) + 1] << 8)  |
       (uint32_t)segment[pid].buffer[(*offset) + 2];

    esrate = (esrate_raw >> 1) & 0x3fffff;  // 0x3fffff = 0011 1111  1111 1111  1111 1111

    kbps = (esrate * 50 + 500) / 1000;  // Rounded

    fprintf (fo, "    Elementary Stream Rate (3 bytes): %u kB/s\n", kbps);

    (*offset) += 3;
    pes_hdr_data_len -= 3;
    if (pes_hdr_data_len < 0) {
      fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
  }

  // Additional Copy Info (1 byte)
  if (additional_copy_info_flag) {
    if ((*offset) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
    fprintf (fo, "    Additional Copy Info (1 byte): 0x%02x\n", segment[pid].buffer[*offset]);
    (*offset)++;
    pes_hdr_data_len--;
    if (pes_hdr_data_len < 0) {
      fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
  }

  // Previous PES packet CRC (2 bytes)
  if (crc_flag) {
    if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
    crc = (segment[pid].buffer[*offset] << 8) | segment[pid].buffer[(*offset) + 1];
    (*offset) += 2;
    fprintf (fo, "    Previous PES packet's CRC (2 bytes): 0x%04x\n", crc);
    pes_hdr_data_len -= 2;
    if (pes_hdr_data_len < 0) {
      fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }
  }

  // PES Extension flags (1 byte)
  if (pes_extension_flag) {

    if ((*offset) >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }

    if (((segment[pid].buffer[*offset] >> 1) & 7) != 7) {
      fprintf (stderr, "PES Extension flags byte 0x%02x doesn't contain binary 1 in each of 2nd, 3rd, and 4th LSBs\n", segment[pid].buffer[*offset]);
      exit (EXIT_FAILURE);
    }

    // PES Private Data flag (1 bit)
    private_data_flag = segment[pid].buffer[*offset] >> 7;
    if (!private_data_flag) {
      fprintf (fo, "    PES Private Data flag (1 bit): %u (no PES private data included)\n", private_data_flag);
    } else {
      fprintf (fo, "    PES Private Data flag: %u (no PES private data)\n", private_data_flag);
    }

    // Pack Header Field Length flag (1 bit)
    pack_header_field_length_flag = (segment[pid].buffer[*offset] >> 6) & 1;
    if (!pack_header_field_length_flag) {
      fprintf (fo, "    Pack Header Field Length flag (1 bit): %u (no Pack Header Field Length included)\n", pack_header_field_length_flag);
    } else {
      fprintf (fo, "    Pack Header Field Length flag (1 bit): %u (Pack Header Field Length included)\n", pack_header_field_length_flag);
    }

    // Program Packet Sequence Counter flag (1 bit)
    seq_counter_flag = (segment[pid].buffer[*offset] >> 5) & 1;
    if (!seq_counter_flag) {
      fprintf (fo, "    Program Packet Sequence Counter flag (1 bit): %u (no Packet Sequence Counter data included)\n", seq_counter_flag);
    } else {
      fprintf (fo, "    Program Packet Sequence Counter flag (1 bit): %u (Packet Sequence Counter data included)\n", seq_counter_flag);
    }

    // P-STD Buffer flag (1 bit)
    pstd_buffer_flag = (segment[pid].buffer[*offset] >> 4) & 1;
    if (!pstd_buffer_flag) {
      fprintf (fo, "    P-STD Buffer flag (1 bit): %u (no P-STD buffer info included)\n", pstd_buffer_flag);
    } else {
      fprintf (fo, "    P-STD Buffer flag (1 bit): %u (P-STD buffer info included)\n", pstd_buffer_flag);
    }

    // Reserved (3 bits)

    // PES Extension 2 flag (1 bit)
    pes_extension2_flag = segment[pid].buffer[*offset] & 1;
    if (!pes_extension2_flag) {
      fprintf (fo, "    PES Extension 2 flag (1 bit): %u (no 2nd PES Header extension included)\n", pes_extension2_flag);
    } else {
      fprintf (fo, "    PES Extension 2 flag (1 bit): %u (2nd PES Header extension included)\n", pes_extension2_flag);
    }
    (*offset)++;
    pes_hdr_data_len--;
    if (pes_hdr_data_len < 0) {
      fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
      exit (EXIT_FAILURE);
    }

    // Process PES Extension flags

    fprintf (fo, "\n");

    // PES Private Data (16 bytes)
    if (private_data_flag) {

      if (((*offset) + 15) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }

      fprintf (fo, "    PES Private Data: ");
      for (i = 0; i < 16; i++) {
        fprintf (fo, "%02x ", segment[pid].buffer[*offset]);
        (*offset)++;
      }
      fprintf (fo, "\n");
      pes_hdr_data_len -= 16;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }
    }

    // Pack Header Field Length (1 byte)
    if (pack_header_field_length_flag) {

      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }

      fprintf (fo, "    Pack Header Field Length (1 byte): %u bytes\n", segment[pid].buffer[*offset]);
      (*offset)++;
      pes_hdr_data_len--;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }
    }

    // Program Packet Sequence Counter (2 bytes)
    if (seq_counter_flag) {

      if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }

      // 1 - Marker bit (1 bit)

      // Packet Sequence Counter value (7 bits)
      seq_counter_val = segment[pid].buffer[*offset] & 0x7f;  // 0x7f = 0111 1111
      (*offset)++;
      pes_hdr_data_len--;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }

      // 1 - Marker bit (1 bit)

      // MPEG-1 / MPEG-2 Identifier (1 bit)
      mpeg1or2 = (segment[pid].buffer[*offset] >> 6) & 1;

      // Original Stuffing Length (6 bits) 
      orig_stuff_len = (size_t) segment[pid].buffer[*offset] & 0x3f;  // 0x3f = 0011 1111

      (*offset)++;
      pes_hdr_data_len--;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }
      fprintf (fo, "    Program Packet Sequence Counter data:\n");
      fprintf (fo, "      Packet Sequence Counter value (7 bits): %u\n", seq_counter_val);
      fprintf (fo, "      MPEG-1 / MPEG-2 Identifier (1 bit): %u\n", mpeg1or2);
      fprintf (fo, "      Original Stuffing Length (6 bits): %zu bytes\n", orig_stuff_len);
    }

    // P-STD Buffer (2 bytes)
    if (pstd_buffer_flag) {

      if (((*offset) + 1) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }

      // 0 1 - Marker bits (2 bits)

      // P-STD Buffer Scale (1 bit)
      pstd_buffer_scale = (segment[pid].buffer[*offset] >> 5) & 1;

      // P-STD Buffer Size (13 bits)
      pstd_buffer_size = ((segment[pid].buffer[*offset] & 0x1f) << 8) | segment[pid].buffer[(*offset) + 1];  // 0x1f = 0001 1111

      fprintf (fo, "    P-STD Buffer:\n");
      if (!pstd_buffer_scale) {
        fprintf (fo, "      Scale (1 bit): 128 bytes\n");
      } else {
        fprintf (fo, "      Scale (1 bit): 1024 bytes\n");
      }
      fprintf (fo, "      Size (12 bits): %u bytes\n", pstd_buffer_size);
      (*offset) += 2;
      pes_hdr_data_len -= 2;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }
    }

    // PES Extension 2
    if (pes_extension2_flag) {

      // PES Extension 2 Length (1 byte)
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }
      ext2_len = (size_t) segment[pid].buffer[*offset] & 0x7f;  // 0x7f = 0111 1111
      fprintf (fo, "    PES Extension 2 length (1 byte): %zu bytes\n", ext2_len);
      (*offset)++;

      // Reserved (1 byte)
      if ((*offset) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }
      (*offset)++;

      // Move past PES Extension 2 Field Length byte and reserved byte.
      pes_hdr_data_len -= 2;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
        exit (EXIT_FAILURE);
      }

      // Move past PES extension 2 data.
      while (ext2_len > 0) {
        if ((*offset) >= (MAX_BUFFERLEN + 1)) {
          fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_header().\n");
          exit (EXIT_FAILURE);
        }
        (*offset)++;
        pes_hdr_data_len--;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_pes_header().\n");
          exit (EXIT_FAILURE);
        }
        ext2_len--;
      }
    }  // End if pes_extension2_flag
  }  // End if pes_extension_flag

  // Optional stuffing bytes (0xff)
  if (pes_hdr_data_len > 0) {
    fprintf (fo, "  PES header stuffing bytes: ");
    while (pes_hdr_data_len > 0) {
      fprintf (fo, "%02x ", segment[pid].buffer[*offset]);
      (*offset)++;
      pes_hdr_data_len--;
    }
    fprintf (fo, "\n");
  }

  return (EXIT_SUCCESS);
}
