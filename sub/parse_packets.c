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

// Parse all MPEG-2 packetized elementary stream (PES) packets needed to compose one complete Subpicture Unit (SPU).
// Store complete SPU in spu_buffer array.
int
parse_packets (OPTIONS *options, uint8_t *subdata, size_t subdatalen,
               uint8_t **spu_buffer, size_t *spu_buffer_size, size_t timestamp,
               IDX *idx, size_t lang, PES *pes_info, SUB *sub_info, FILE *fo) {

  ssize_t pes_hdr_data_len;
  size_t header_used;
  size_t i, pos, spu_pos, pack_stuffing_len, packet_top, packet_end, spu_chunk_len, header_data_end;
  size_t prefix_len, remaining;
  size_t declared_spu_size;
  uint8_t pack_id, stream_id, scrambling_ctrl, pes_priority, alignment_indic, copyright, copy_orig;
  uint8_t ptsdts, escr, esrate_flag, dsmtrickmode, add_copyinfo, crc_flag, extension, private, pack_header, seq_counter, pstd_buffer, extension2;
  uint8_t seq_counter_val, mpeg1or2, orig_stuff_len, pack_header_field_len, pstd_buffer_scale, ext2_len, substreamid, stream;
  uint8_t have_start_pts, spu_prefix[6];
  uint16_t crc, scr_ext, escr_ext;
  uint32_t program_mux_raw, program_mux_rate, esrate_raw, esrate, kbps, pstd_buffer_size;
  uint64_t scr_27mhz, scr_base, scr_ms, escr_27mhz, escr_base, escr_ms, pts90, dts90;

  fprintf (fo, "  BUILDING SPU BUFFER FROM MPEG-2 PES PACKET(S)\n\n");
  pos = idx->offset[lang][timestamp];  // First packet starts at index specified by .idx file.
  spu_pos = 0;
  declared_spu_size = 0;
  *spu_buffer_size = 0;
  free (*spu_buffer);
  *spu_buffer = NULL;

  // Loop through all MPEG-2 PES packets needed to compose SPU.
  // If the SPU data size if larger than PES packet length then the SPU data spans multiple packets.
  // Obtain SPU_SZ from 1st packet, keep adding SPU data from packets to spu_buffer while reducing spu_sz accordingly, until spu_sz == 0.
  have_start_pts = 0;
  prefix_len = 0;
  memset (spu_prefix, 0, sizeof (spu_prefix));
  do {

    // MPEG-2 Pack header (The first one is at offset defined in .idx file) (4 bytes)
    fprintf (fo, "  MPEG-2 PACK HEADER\n");

    // Start Code (3 bytes)
    // Should be: 0x00 00 01
    if ((pos + 2) >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    if ((subdata[pos] != 0x00) || (subdata[pos + 1] != 0x00) || (subdata[pos + 2] != 0x01)) {
      fprintf (stderr, "Failed to find proper start code at offset 0x%08zx\n", pos);
      exit (EXIT_FAILURE);
    }
    fprintf (fo, "  Start Code (3 bytes): %02x %02x %02x\n", subdata[pos], subdata[pos + 1], subdata[pos + 2]);
    pos += 3;

    // Pack ID (1 byte)
    // Should be: 0xba
    if (pos >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    pack_id = subdata[pos];
    if (pack_id != 0xba) {
      fprintf (stderr, "Failed to find Pack ID for MPEG-2 Pack Header at offset 0x%08zx\n", pos);
      exit (EXIT_FAILURE);
    }
    fprintf (fo, "  Pack Identifier (1 byte): 0x%02x\n", pack_id);
    pos++;

    // 01 (2 bits)
    if ((pos + 6) >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    if ((((subdata[pos] & 0x80) >> 7) != 0) || (((subdata[pos] & 0x40) >> 6) != 1)) {  // 0x80 = 1000 0000, 0x40 = 0100 0000
      fprintf (stderr, "Missing bits: 01 before MPEG-2 SCR in parse_packets().\n");
      exit (EXIT_FAILURE);
    }

    // System Clock Reference (SCR) Base (33 bits)
    scr_base =
      ((uint64_t) (subdata[pos]     & 0x38) << 27) |  // base[32..30]; 0x38 = 0011 1000
      ((uint64_t) (subdata[pos]     & 0x03) << 28) |  // base[29..28]; 0x03 = 0000 0011
      ((uint64_t) (subdata[pos + 1])        << 20) |  // base[27..20]
      ((uint64_t) (subdata[pos + 2] & 0xf8) << 12) |  // base[19..15]; 0xf8 = 1111 1000
      ((uint64_t) (subdata[pos + 3])        << 5)  |  // base[12..5]
      ((uint64_t) (subdata[pos + 4])        >> 3);    // base[4..0]

    // System Clock Reference Extension (SCR_ext) (9 bits)
    scr_ext =
      ((subdata[pos + 4] & 0x03) << 7) |
      ((subdata[pos + 5] & 0xfe) >> 1);  // 0xfe = 1111 1110

    pos += 6;

    // SCR base ticks at 90 kHz.
    // 1 base tick = 300 × 27 MHz ticks
    // 27,000 ticks = 1 ms
    scr_27mhz = scr_base * 300 + scr_ext;
    scr_ms = (scr_27mhz + 13500) / 27000;  // Rounded
    fprintf (fo, "    System Clock Reference (SCR) (6 bytes): %" PRIu64 " ms\n", scr_ms);

    // Program MUX Rate (22 bits)
    if ((pos + 2) >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    program_mux_raw =
      ((uint32_t) subdata[pos]     << 16) |
      ((uint32_t) subdata[pos + 1] << 8)  |
       (uint32_t) subdata[pos + 2];
    program_mux_rate = (program_mux_raw >> 1) & 0x3fffff;  // 22 bits
    kbps = (program_mux_rate * 50 + 500) / 1000;
    fprintf (fo, "    Program Mux Rate (22 bits): %u kB/s\n", kbps);
    pos += 3;

    // Pack Stuffing Length (3 bits)
    if (pos >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    pack_stuffing_len = subdata[pos] & 0x07;  // 0x07 = 0000 0111
    if ((pos + 1 + pack_stuffing_len) >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    fprintf (fo, "    Pack Stuffing Length: %zu bytes\n", pack_stuffing_len);
    pos++;
    pos += pack_stuffing_len;  // Move past Pack Stuffing Bytes (would be 0xff).

    // Packetized Elementary Stream (PES) header for Private Stream 1.
    fprintf (fo, "\n  PACKETIZED ELEMENTARY STREAM (PES) HEADER\n");

    // Start Code (3 bytes)
    // Should be 0x00 00 01
    if ((pos + 2) >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    if ((subdata[pos] != 0x00) || (subdata[pos + 1] != 0x00) || (subdata[pos + 2] != 0x01)) {
      fprintf (stderr, "Failed to find proper start code for PES header at offset 0x%08zx\n", pos);
      exit (EXIT_FAILURE);
    }
    fprintf (fo, "    Start Code (3 bytes): %02x %02x %02x\n", subdata[pos], subdata[pos+1], subdata[pos+2]);
    pos += 3;

    // Stream ID (1 byte)
    // Should be: 0xbd for Private Stream 1 (subpictures, non-MPEG audio)
    if (pos >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    if (subdata[pos] != 0xbd) {
      fprintf (stderr, "Failed to find proper stream ID for Private Stream 1 at offset 0x%08zx\n", pos);
      exit (EXIT_FAILURE);
    }
    stream_id = subdata[pos];
    fprintf (fo, "    Stream Identifier (1 byte): 0x%02x\n", stream_id);
    pos++;

    // PES Packet Length (2 bytes)
    if ((pos + 1) >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    pes_info->pes_packet_len = (subdata[pos] << 8) | subdata[pos + 1];
    fprintf (fo, "    PES Packet Length (2 bytes): %zu bytes\n", pes_info->pes_packet_len);
    pos += 2;
    packet_top = pos;
    if (pes_info->pes_packet_len != 0) {
      if (pes_info->pes_packet_len > subdatalen - packet_top) {
        fprintf (stderr, "PES packet length extends beyond end of .sub file.\n");
        return (EXIT_FAILURE);
      }
      packet_end = packet_top + pes_info->pes_packet_len;
    } else {
      packet_end = subdatalen;
    }

    // Packetized Elementary Stream (PES) Header Extension
    fprintf (fo, "\n  PACKETIZED ELEMENTARY STREAM (PES) HEADER EXTENSION\n");

    // 10 (2 bits)
    if (pos >= packet_end) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      return (EXIT_FAILURE);
    }
    if ((subdata[pos] >> 6) != 2) {
      fprintf (stderr, "First byte 0x%02x of PES Header Extension doesn't have 2 MSBs as binary 10\n", subdata[pos] >> 6);
      exit (EXIT_FAILURE);
    }

    // PES scrambling control (2 bits)
    scrambling_ctrl = (subdata[pos] >> 4) & 3;
    if (!scrambling_ctrl) {
      fprintf (fo, "    PES Scrambling Control (2 bits): 0x%02x (not scrambled)\n", scrambling_ctrl);
    } else {
      fprintf (fo, "    PES Scrambling Control (2 bits): 0x%02x (user defined)\n", scrambling_ctrl);
    }

    // PES priority (1 bit)
    pes_priority = (subdata[pos] >> 3) & 1;
    fprintf (fo, "    PES Priority (1 bit): %u\n", pes_priority);

    // Data alignment indicator (1 bit)
    alignment_indic = (subdata[pos] >> 2) & 1;
    if (!alignment_indic) {
      fprintf (fo, "    Data alignment indicator (1 bit): %u\n", alignment_indic);
    } else {
      fprintf (fo, "    Data alignment indicator (1 bit): %u (video start code or audio syncword starts immediately after PES packet header)\n", alignment_indic);
    }

    // Copyright indicator (1 bit)
    copyright = (subdata[pos] >> 1) & 1;
    if (!copyright) {
      fprintf (fo, "    Copyright (1 bit): %u (not copyrighted)\n", copyright);
    } else {
      fprintf (fo, "    Copyright (1 bit): %u (copyrighted)\n", copyright);
    }

    // Original or Copy indicator (1 bit)
    copy_orig = subdata[pos] & 1;
    if (copy_orig) {
      fprintf (fo, "    Copy/Original (1 bit): %u (original)\n", copy_orig);
    } else {
      fprintf (fo, "    Copy/Original (1 bit): %u (copy)\n", copy_orig);
    }
    pos++;

    // Presentation Time Stamp (PTS) / Decode Time Stamp (DTS) flag (2 bits)
    if (pos >= packet_end) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      return (EXIT_FAILURE);
    }
    ptsdts = subdata[pos] >> 6;
    if (ptsdts == 0) {
      fprintf (fo, "    PTS/DTS flag (2 bits): %u (no PTS or DTS data)\n", ptsdts);
    } else if (ptsdts == 1) {
      fprintf (stderr, "Invalid PTS/DTS flag value of 1 in PES Header Extension\n");
      exit (EXIT_FAILURE);
    } else if (ptsdts == 2) {
      fprintf (fo, "    PTS/DTS flag (2 bits): %u (PTS only)\n", ptsdts);
    } else if (ptsdts == 3) {
      fprintf (fo, "    PTS/DTS flag (2 bits): %u (PTS and DTS)\n", ptsdts);
    }

    // Elementary Stream Clock Reference (ESCR) flag (1 bit)
    escr = (subdata[pos] >> 5) & 1;
    if (!escr) {
      fprintf (fo, "    Elementary Stream Clock Reference (ESCR) flag (1 bit): %u (no ESCR reference included)\n", escr);
    } else {
      fprintf (fo, "    Elementary Stream Clock Reference (ESCR) flag (1 bit): %u (ESCR reference included)\n", escr);
    }

    // Elementary Stream Rate flag (1 bit)
    esrate_flag = (subdata[pos] >> 4) & 1;
    if (!esrate_flag) {
      fprintf (fo, "    Elementary Stream Rate flag (1 bit): %u (no ES rate provided)\n", esrate_flag);
    } else {
      fprintf (fo, "    Elementary Stream Rate flag (1 bit): %u (ES rate provided)\n", esrate_flag);
    }

    // DSM Trick Mode flag (not used for DVD) (1 bit)
    dsmtrickmode = (subdata[pos] >> 3) & 1;
    fprintf (fo, "    DSM Trick Mode flag (1 bit): %u (not applicable to DVD)\n", dsmtrickmode);

    // Additional Copy Info flag (1 bit)
    add_copyinfo = (subdata[pos] >> 2) & 1;
    if (!add_copyinfo) {
      fprintf (fo, "    Additional Copy Info flag (1 bit): %u (no additional copy info included)\n", add_copyinfo);
    } else {
      fprintf (fo, "    Additional Copy Info flag (1 bit): %u (additional copy info included)\n", add_copyinfo);
    }

    // CRC flag (1 bit)
    crc_flag = (subdata[pos] >> 1) & 1;
    if (!crc_flag) {
      fprintf (fo, "    CRC flag (1 bit): %u (previous PES packet CRC not included)\n", crc_flag);
    } else {
      fprintf (fo, "    CRC flag (1 bit): %u (previous PES packet CRC included)\n", crc_flag);
    }

    // PES Extension flag (1 bit)
    extension = subdata[pos] & 1;
    if (!extension) {
      fprintf (fo, "    PES Extension flag (1 bit): %u (no further PES Header extension)\n", extension);
    } else {
      fprintf (fo, "    PES Extension flag (1 bit): %u (further PES Header extension)\n", extension);
    }
    pos++;

    // PES Header Data Length (1 byte)
    if (pos >= packet_end) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      return (EXIT_FAILURE);
    }
    pes_hdr_data_len = (ssize_t) subdata[pos];
    fprintf (fo, "    PES header data length (1 byte): %zd bytes\n", pes_hdr_data_len);
    pos++;
    if ((size_t) pes_hdr_data_len > packet_end - pos) {
      fprintf (stderr, "PES header data length exceeds PES packet boundary.\n");
      return (EXIT_FAILURE);
    }
    header_data_end = pos + (size_t) pes_hdr_data_len;

    // Process PES flags.

    // PTS (33 bits); PTS only, or PTS & DTS.
    if ((ptsdts == 2) || (ptsdts == 3)) {

      uint64_t adjusted90;
      uint8_t pts_prefix;

      if (pos > header_data_end || 5 > header_data_end - pos) {
        fprintf (stderr, "Unexpectedly reached end of PES packet while reading PTS.\n");
        return (EXIT_FAILURE);
      }

      pts90 =
        ((uint64_t) (subdata[pos]     & 0x0e) << 29) |
        ((uint64_t)  subdata[pos + 1]          << 22) |
        ((uint64_t) (subdata[pos + 2] & 0xfe) << 14) |
        ((uint64_t)  subdata[pos + 3]          << 7)  |
        ((uint64_t) (subdata[pos + 4] & 0xfe) >> 1);

      pes_info->pts90 = pts90;
      pes_info->pts.totalms = (int64_t) ((pts90 + 45u) / 90u);
      if (mstotime (&pes_info->pts) != EXIT_SUCCESS) return (EXIT_FAILURE);

      fprintf (fo, "    PTS: %02d:%02d:%02d,%03d totalms: %" PRId64 " (90-kHz ticks: %" PRIu64 ")\n",
               pes_info->pts.h, pes_info->pts.m, pes_info->pts.s, pes_info->pts.ms,
               pes_info->pts.totalms, pts90);

      if (options->offset_flag || options->sync_flag) {
        if (transform_timestamp90 (options, pts90, &adjusted90) != EXIT_SUCCESS) return (EXIT_FAILURE);
        pts_prefix = (ptsdts == 3) ? 0x30 : 0x20;
        record_pes_timestamp_change (options, pos, adjusted90, pts_prefix);
        pes_info->pts90 = adjusted90;
        pes_info->pts.totalms = (int64_t) ((adjusted90 + 45u) / 90u);
        if (mstotime (&pes_info->pts) != EXIT_SUCCESS) return (EXIT_FAILURE);
        fprintf (fo, "    Adjusted PTS: %02d:%02d:%02d,%03d (90-kHz ticks: %" PRIu64 ")\n",
                 pes_info->pts.h, pes_info->pts.m, pes_info->pts.s, pes_info->pts.ms, adjusted90);
      }

      if (!have_start_pts) {
        sub_info->start = pes_info->pts;
        have_start_pts = 1;
      }
      pes_info->dts90 = 0;
      memset (&pes_info->dts, 0, sizeof (pes_info->dts));

      pos += 5;
      pes_hdr_data_len -= 5;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        return (EXIT_FAILURE);
      }
    }

    // DTS (33 bits), when both PTS and DTS are present.
    if (ptsdts == 3) {

      uint64_t adjusted90;

      if (pos > header_data_end || 5 > header_data_end - pos) {
        fprintf (stderr, "Unexpectedly reached end of PES packet while reading DTS.\n");
        return (EXIT_FAILURE);
      }

      dts90 =
        ((uint64_t) (subdata[pos]     & 0x0e) << 29) |
        ((uint64_t)  subdata[pos + 1]          << 22) |
        ((uint64_t) (subdata[pos + 2] & 0xfe) << 14) |
        ((uint64_t)  subdata[pos + 3]          << 7)  |
        ((uint64_t) (subdata[pos + 4] & 0xfe) >> 1);

      pes_info->dts90 = dts90;
      pes_info->dts.totalms = (int64_t) ((dts90 + 45u) / 90u);
      if (mstotime (&pes_info->dts) != EXIT_SUCCESS) return (EXIT_FAILURE);

      fprintf (fo, "    DTS: %02d:%02d:%02d,%03d totalms: %" PRId64 " (90-kHz ticks: %" PRIu64 ")\n",
               pes_info->dts.h, pes_info->dts.m, pes_info->dts.s, pes_info->dts.ms,
               pes_info->dts.totalms, dts90);

      if (options->offset_flag || options->sync_flag) {
        if (transform_timestamp90 (options, dts90, &adjusted90) != EXIT_SUCCESS) return (EXIT_FAILURE);
        record_pes_timestamp_change (options, pos, adjusted90, 0x10);
        pes_info->dts90 = adjusted90;
        pes_info->dts.totalms = (int64_t) ((adjusted90 + 45u) / 90u);
        if (mstotime (&pes_info->dts) != EXIT_SUCCESS) return (EXIT_FAILURE);
        fprintf (fo, "    Adjusted DTS: %02d:%02d:%02d,%03d (90-kHz ticks: %" PRIu64 ")\n",
                 pes_info->dts.h, pes_info->dts.m, pes_info->dts.s, pes_info->dts.ms, adjusted90);
      }

      pos += 5;
      pes_hdr_data_len -= 5;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        return (EXIT_FAILURE);
      }
    }

    // Elementary Stream Clock Reference (ESCR) (6 bytes)
    if (escr) {

      if (pos > header_data_end || 6 > header_data_end - pos) {
        fprintf (stderr, "Unexpectedly reached end of PES header data while reading ESCR.\n");
        return (EXIT_FAILURE);
      }

      // Elementary Stream Clock Reference (ESCR) Base (33 bits)
      escr_base =
        ((uint64_t) (subdata[pos]     & 0x38) << 27) |  // base[32..30]; 0x38 = 0011 1000
        ((uint64_t) (subdata[pos]     & 0x03) << 28) |  // base[29..28]; 0x03 = 0000 0011
        ((uint64_t) (subdata[pos + 1])        << 20) |  // base[27..20]
        ((uint64_t) (subdata[pos + 2] & 0xf8) << 12) |  // base[19..15]; 0xf8 = 1111 1000
        ((uint64_t) (subdata[pos + 2] & 0x03) << 13) |  // base[14..13]
        ((uint64_t) (subdata[pos + 3])        << 5)  |  // base[12..5]
        ((uint64_t) (subdata[pos + 4])        >> 3);    // base[4..0]

      // Elementary Stream Clock Reference Extension (ESCR_ext) (9 bits)
      escr_ext =
        ((subdata[pos + 4] & 0x03) << 7) |
        ((subdata[pos + 5] & 0xfe) >> 1);  // 0xfe = 1111 1110

      // ESCR base ticks at 90 kHz.
      // 1 base tick = 300 × 27 MHz ticks
      // 27,000 ticks = 1 ms
      escr_27mhz = escr_base * 300 + escr_ext;
      escr_ms = (escr_27mhz + 13500) / 27000;  // Rounded
      fprintf (fo, "    Elementary Stream Clock Reference (ESCR) (6 bytes): %" PRIu64 " ms\n", escr_ms);

      pos += 6;
      pes_hdr_data_len -= 6;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        exit (EXIT_FAILURE);
      }
    }

    // Elementary Stream Rate (3 bytes)
    if (esrate_flag) {

      if (pos > header_data_end || 3 > header_data_end - pos) {
        fprintf (stderr, "Unexpectedly reached end of PES header data while reading ES rate.\n");
        return (EXIT_FAILURE);
      }
      esrate_raw =
        ((uint32_t)subdata[pos]     << 16) |
        ((uint32_t)subdata[pos + 1] << 8)  |
         (uint32_t)subdata[pos + 2];

      esrate = (esrate_raw >> 1) & 0x3fffff;  // 0x3fffff = 0011 1111  1111 1111  1111 1111

      kbps = (esrate * 50 + 500) / 1000;  // Rounded

      fprintf (fo, "    Elementary Stream Rate (3 bytes): %u kB/s\n", kbps);

      pos += 3;
      pes_hdr_data_len -= 3;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        exit (EXIT_FAILURE);
      }
    }

    // Additional Copy Info (1 byte)
    if (add_copyinfo) {

      if (pos >= header_data_end) {
        fprintf (stderr, "Unexpectedly reached end of PES header data while reading Additional Copy Info.\n");
        return (EXIT_FAILURE);
      }

      fprintf (fo, "    Additional Copy Info (1 byte): 0x%02x\n", subdata[pos]);
      pos++;
      pes_hdr_data_len--;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        exit (EXIT_FAILURE);
      }
    }

    // Previous PES packet CRC (2 bytes)
    if (crc_flag) {

      if (pos > header_data_end || 2 > header_data_end - pos) {
        fprintf (stderr, "Unexpectedly reached end of PES header data while reading CRC.\n");
        return (EXIT_FAILURE);
      }

      crc = (subdata[pos] << 8) | subdata[pos+1];
      fprintf (fo, "    Previous PES packet's CRC (2 bytes): 0x%04x\n", crc);
      pos += 2;
      pes_hdr_data_len -= 2;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        exit (EXIT_FAILURE);
      }
    }

    // PES Extension flags (1 byte)
    if (extension) {

      if (pos >= header_data_end) {
        fprintf (stderr, "Unexpectedly reached end of PES header data while reading PES extension flags.\n");
        return (EXIT_FAILURE);
      }

      if (((subdata[pos] >> 1) & 7) != 7) {
        fprintf (stderr, "PES Extension flags byte 0x%02x doesn't contain binary 1 in each of 2nd, 3rd, and 4th LSBs\n", subdata[pos]);
        exit (EXIT_FAILURE);
      }

      // PES Private Data flag (1 bit)
      private = subdata[pos] >> 7;
      if (!private) {
        fprintf (fo, "    PES Private Data flag (1 bit): %u (no PES private data included)\n", private);
      } else {
        fprintf (fo, "    PES Private Data flag (1 bit): %u (PES private data included)\n", private);
      }

      // Pack Header Field Length flag (1 bit)
      pack_header = (subdata[pos] >> 6) & 1;
      if (!pack_header) {
        fprintf (fo, "    Pack Header Field Length flag (1 bit): %u (no Pack Header Field Length included)\n", pack_header);
      } else {
        fprintf (fo, "    Pack Header Field Length flag (1 bit): %u (Pack Header Field Length included)\n", pack_header);
      }

      // Program Packet Sequence Counter flag (1 bit)
      seq_counter = (subdata[pos] >> 5) & 1;
      if (!seq_counter) {
        fprintf (fo, "    Program Packet Sequence Counter flag (1 bit): %u (no Packet Sequence Counter data included)\n", seq_counter);
      } else {
        fprintf (fo, "    Program Packet Sequence Counter flag (1 bit): %u (Packet Sequence Counter data included)\n", seq_counter);
      }

      // P-STD Buffer flag (1 bit)
      pstd_buffer = (subdata[pos] >> 4) & 1;
      if (!pstd_buffer) {
        fprintf (fo, "    P-STD Buffer flag (1 bit): %u (no P-STD buffer info included)\n", pstd_buffer);
      } else {
        fprintf (fo, "    P-STD Buffer flag (1 bit): %u (P-STD buffer info included)\n", pstd_buffer);
      }

      // PES Extension 2 flag (1 bit)
      extension2 = subdata[pos] & 1;
      if (!extension2) {
        fprintf (fo, "    PES Extension 2 flag (1 bit): %u (no 2nd PES Header extension included)\n", extension2);
      } else {
        fprintf (fo, "    PES Extension 2 flag (1 bit): %u (2nd PES Header extension included)\n", extension2);
      }
      pos++;  // Move past PES Extension flags byte
      pes_hdr_data_len--;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        exit (EXIT_FAILURE);
      }

      // Process PES Extension flags

      fprintf (fo, "\n");

      // PES Private Data (16 bytes)
      if (private) {

        if (pos > header_data_end || 16 > header_data_end - pos) {
          fprintf (stderr, "Truncated PES Private Data.\n");
          return (EXIT_FAILURE);
        }
        fprintf (fo, "    PES Private Data (16 bytes): ");
        for (i = 0; i < 16; i++) {
          fprintf (fo, "%02x ", subdata[pos]);
          pos++;
        }
        fprintf (fo, "\n");
        pes_hdr_data_len -= 16;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          exit (EXIT_FAILURE);
        }
      }

      // Pack Header Field Length (1 byte), followed by that many bytes of pack header data.
      if (pack_header) {

        if (pos >= header_data_end) {
          fprintf (stderr, "Truncated Pack Header Field Length.\n");
          return (EXIT_FAILURE);
        }
        pack_header_field_len = subdata[pos];
        fprintf (fo, "    Pack Header Field Length (1 byte): %u bytes\n", pack_header_field_len);
        pos++;
        pes_hdr_data_len--;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          return (EXIT_FAILURE);
        }
        if ((size_t) pack_header_field_len > header_data_end - pos) {
          fprintf (stderr, "Truncated Pack Header Field data.\n");
          return (EXIT_FAILURE);
        }
        fprintf (fo, "    Pack Header Field data (%u bytes):", pack_header_field_len);
        for (i = 0; i < pack_header_field_len; i++) {
          fprintf (fo, " %02x", subdata[pos + i]);
        }
        fprintf (fo, "\n");
        pos += pack_header_field_len;
        pes_hdr_data_len -= pack_header_field_len;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          return (EXIT_FAILURE);
        }
      }

      // Program Packet Sequence Counter (2 bytes)
      if (seq_counter) {

        if (pos > header_data_end || 2 > header_data_end - pos) {
          fprintf (stderr, "Truncated Program Packet Sequence Counter.\n");
          return (EXIT_FAILURE);
        }

        // 1 (1 bit)

        // Packet Sequence Counter value (7 bits)
        seq_counter_val = subdata[pos] & 0x7f;  // 0x7f = 0111 1111
        pos++;
        pes_hdr_data_len--;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          exit (EXIT_FAILURE);
        }

        // MPEG-1 / MPEG-2 Identifier (1 bit)
        mpeg1or2 = (subdata[pos] >> 6) & 1;

        // Original Stuffing Length (6 bits)
        orig_stuff_len = subdata[pos] & 0x3f;  // 0x3f = 0011 1111

        pos++;
        pes_hdr_data_len--;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          exit (EXIT_FAILURE);
        }

        fprintf (fo, "    Program Packet Sequence Counter data:\n");
        fprintf (fo, "      Packet Sequence Counter value (7 bits): %u\n", seq_counter_val);
        fprintf (fo, "      MPEG-1 / MPEG-2 Identifier (1 bit): %u\n", mpeg1or2);
        fprintf (fo, "      Original Stuffing Length (6 bits): %u bytes\n", orig_stuff_len);
      }

      // P-STD Buffer (2 bytes)
      if (pstd_buffer) {

        if (pos > header_data_end || 2 > header_data_end - pos) {
          fprintf (stderr, "Truncated P-STD Buffer data.\n");
          return (EXIT_FAILURE);
        }

        // P-STD Buffer Scale (1 bit)
        pstd_buffer_scale = (subdata[pos] >> 5) & 1;

        // P-STD Buffer Size (13 bits).  The encoded value is multiplied by
        // 128 or 1024 bytes according to P-STD_buffer_scale.
        pstd_buffer_size = ((uint32_t) (subdata[pos] & 0x1f) << 8) | subdata[pos + 1];

        fprintf (fo, "    P-STD Buffer:\n");
        if (!pstd_buffer_scale) {
          fprintf (fo, "      Scale (1 bit): 128 bytes\n");
          fprintf (fo, "      Size (13 bits): %u units = %u bytes\n",
                   pstd_buffer_size, pstd_buffer_size * 128u);
        } else {
          fprintf (fo, "      Scale (1 bit): 1024 bytes\n");
          fprintf (fo, "      Size (13 bits): %u units = %u bytes\n",
                   pstd_buffer_size, pstd_buffer_size * 1024u);
        }
        pos += 2;
        pes_hdr_data_len -= 2;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          exit (EXIT_FAILURE);
        }
      }

      // PES Extension 2
      if (extension2) {

        if (pos >= header_data_end) {
          fprintf (stderr, "Truncated PES Extension 2 length.\n");
          return (EXIT_FAILURE);
        }

        // PES Extension 2 Length (7 bits), following a mandatory marker bit.
        if ((subdata[pos] & 0x80) == 0) {
          fprintf (stderr, "Missing marker bit in PES Extension 2 length byte.\n");
          return (EXIT_FAILURE);
        }
        ext2_len = subdata[pos] & 0x7f;
        fprintf (fo, "    PES Extension 2 length (1 byte): %u bytes\n", ext2_len);
        pos++;
        pes_hdr_data_len--;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          return (EXIT_FAILURE);
        }

        // The length counts the bytes that follow the length byte; these are
        // reserved/extension data.  It does not include an extra fixed byte.
        if ((size_t) ext2_len > header_data_end - pos) {
          fprintf (stderr, "Truncated PES Extension 2 data.\n");
          return (EXIT_FAILURE);
        }
        if (ext2_len > 0) {
          fprintf (fo, "    PES Extension 2 data (%u bytes):", ext2_len);
          for (i = 0; i < ext2_len; i++) {
            fprintf (fo, " %02x", subdata[pos + i]);
          }
          fprintf (fo, "\n");
        }
        pos += ext2_len;
        pes_hdr_data_len -= ext2_len;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          return (EXIT_FAILURE);
        }
      }
    }

    // Optional stuffing bytes (0xff)
    if (pes_hdr_data_len > 0) {
      fprintf (fo, "  PES header stuffing bytes: ");

      while (pes_hdr_data_len > 0) {
        if (pos >= header_data_end) {
          fprintf (stderr, "Unexpected end of PES header data in stuffing bytes.\n");
          return (EXIT_FAILURE);
        }

        fprintf (fo, "%02x ", subdata[pos]);
        pos++;
        pes_hdr_data_len--;
      }
      fprintf (fo, "\n");
    }

    // Substream ID (1 byte) (occurs for all packets in a chain, not just the first)
    // Not part of SPU.
    // 0x20 is base number for DVD subpicture streams.
    // Range: 0x20 - 0x3f (i.e., Stream 0 to Stream 31)
    // 0x20 + stream number (e.g., 0x22 is Stream 2)
    // Maps to language in .idx. e.g., "id: nl, index: 0" means Stream 0 is Dutch
    if (pos >= packet_end) {
      fprintf (stderr, "PES packet contains no DVD subpicture substream ID.\n");
      return (EXIT_FAILURE);
    }
    substreamid = subdata[pos];
    if ((substreamid < 0x20) || (substreamid > 0x3f)) {
      fprintf (stderr, "Not a DVD subpicture stream. Substream ID is %02x but expected anything from 0x20 to 0x3f.\n", substreamid);
      exit (EXIT_FAILURE);
    } else {
      stream = substreamid - 0x20;
      fprintf (fo, "\n  Stream ID (1 byte): 0x%02x (DVD subpicture stream, Language Index: %u)\n\n", substreamid, stream);
      if ((size_t) stream != idx->id_index[lang]) {
        fprintf (stderr, "IDX filepos points to subpicture stream %u, but language %s expects stream %zu.\n",
                 stream, idx->id[lang], idx->id_index[lang]);
        return (EXIT_FAILURE);
      }
    }
    pos++;

    // Append this PES payload to the complete SPU.  The first 2 bytes are
    // enough to distinguish classic from extended format; an extended header
    // needs 6 bytes before its 32-bit total size is known.  Accumulating this
    // small prefix allows even the extended header itself to cross a PES
    // boundary.
    if (pos > packet_end) {
      fprintf (stderr, "PES header consumes more bytes than PES_packet_length permits.\n");
      return (EXIT_FAILURE);
    }
    header_used = pos - packet_top;
    if (pes_info->pes_packet_len == 0) {
      fprintf (stderr, "Private Stream 1 PES packet has zero PES_packet_length and cannot be safely delimited.\n");
      return (EXIT_FAILURE);
    }
    if (header_used > pes_info->pes_packet_len) {
      fprintf (stderr, "PES header exceeds PES_packet_length.\n");
      return (EXIT_FAILURE);
    }
    spu_chunk_len = pes_info->pes_packet_len - header_used;
    if (spu_chunk_len > packet_end - pos || spu_chunk_len > subdatalen - pos) {
      fprintf (stderr, "Unexpected end of PES packet while locating SPU payload.\n");
      return (EXIT_FAILURE);
    }

    while (declared_spu_size == 0 && spu_chunk_len > 0) {
      size_t need;

      if (prefix_len < 2) need = 2 - prefix_len;
      else if (spu_prefix[0] == 0 && spu_prefix[1] == 0 && prefix_len < 6) need = 6 - prefix_len;
      else need = 0;

      if (need != 0) {
        size_t take = need < spu_chunk_len ? need : spu_chunk_len;
        memcpy (spu_prefix + prefix_len, subdata + pos, take);
        prefix_len += take;
        pos += take;
        spu_chunk_len -= take;
      }

      if (prefix_len >= 2 && (spu_prefix[0] != 0 || spu_prefix[1] != 0)) {
        declared_spu_size = ((size_t) spu_prefix[0] << 8) | spu_prefix[1];
      } else if (prefix_len >= 6) {
        declared_spu_size = ((size_t) spu_prefix[2] << 24) |
                            ((size_t) spu_prefix[3] << 16) |
                            ((size_t) spu_prefix[4] << 8) |
                             (size_t) spu_prefix[5];
      }

      if (declared_spu_size != 0) {
        size_t minimum = (spu_prefix[0] == 0 && spu_prefix[1] == 0) ? 10u : 4u;
        if (declared_spu_size < minimum || declared_spu_size < prefix_len) {
          fprintf (stderr, "Invalid declared SPU size %zu.\n", declared_spu_size);
          return (EXIT_FAILURE);
        }
        *spu_buffer = malloc (declared_spu_size);
        if (*spu_buffer == NULL) {
          fprintf (stderr, "Cannot allocate %zu-byte SPU buffer.\n", declared_spu_size);
          return (EXIT_FAILURE);
        }
        *spu_buffer_size = declared_spu_size;
        memcpy (*spu_buffer, spu_prefix, prefix_len);
        spu_pos = prefix_len;
      } else if (prefix_len >= 6) {
        fprintf (stderr, "Extended SPU declares a zero total size.\n");
        return (EXIT_FAILURE);
      } else if (spu_chunk_len == 0) {
        break;
      }
    }

    if (declared_spu_size != 0 && spu_chunk_len > 0) {
      remaining = declared_spu_size - spu_pos;
      if (spu_chunk_len > remaining) spu_chunk_len = remaining;
      if (spu_chunk_len > packet_end - pos || spu_chunk_len > subdatalen - pos) {
        fprintf (stderr, "Unexpected end of PES packet while copying SPU data.\n");
        return (EXIT_FAILURE);
      }
      memcpy (*spu_buffer + spu_pos, subdata + pos, spu_chunk_len);
      spu_pos += spu_chunk_len;
      pos += spu_chunk_len;
    }

  } while ((declared_spu_size == 0 || spu_pos < declared_spu_size) && (pos < subdatalen));

  if (declared_spu_size == 0 || spu_pos != declared_spu_size) {
    fprintf (stderr, "Incomplete SPU: assembled %zu of %zu bytes.\n", spu_pos, declared_spu_size);
    return (EXIT_FAILURE);
  }
  if (!have_start_pts) {
    fprintf (stderr, "SPU PES chain contains no PTS.\n");
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
