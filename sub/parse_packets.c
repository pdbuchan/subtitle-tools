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
parse_packets (OPTIONS *options, uint8_t *subdata, size_t subdatalen, uint8_t *spu_buffer, int timestamp, IDX *idx, int lang, PES *pes_info, SUB *sub_info, FILE *fo) {

  int64_t temp;
  ssize_t pes_hdr_data_len, header_used;
  size_t i, pos, spu_pos, pack_stuffing_len, packet_top, spu_chunk_len;
  uint8_t pack_id, stream_id, scrambling_ctrl, pes_priority, alignment_indic, copyright, copy_orig;
  uint8_t ptsdts, escr, esrate_flag, dsmtrickmode, add_copyinfo, crc_flag, extension, private, pack_header, seq_counter, pstd_buffer, extension2;
  uint8_t seq_counter_val, mpeg1or2, orig_stuff_len, pack_header_field_len, pstd_buffer_scale, ext2_len, first_packet, substreamid, stream;
  uint16_t crc, scr_ext, escr_ext, spu_sz;
  uint32_t program_mux_raw, program_mux_rate, esrate_raw, esrate, kbps, pstd_buffer_size;
  uint64_t scr_27mhz, scr_base, scr_ms, escr_27mhz, escr_base, escr_ms, pts90, dts90;
  double ratio;

  fprintf (fo, "  BUILDING SPU BUFFER FROM MPEG-2 PES PACKET(S)\n\n");
  pos = idx->offset[lang][timestamp];  // First packet starts at index specified by .idx file.
  spu_pos = 0;  // Index of spu_buffer

  // Compute scaling ratio for sync option.
  ratio = (((double) options->newlastms - (double) options->newfirstms) / ((double) options->oldlastms - (double) options->oldfirstms));

  // Loop through all MPEG-2 PES packets needed to compose SPU.
  // If the SPU data size if larger than PES packet length then the SPU data spans multiple packets.
  // Obtain SPU_SZ from 1st packet, keep adding SPU data from packets to spu_buffer while reducing spu_sz accordingly, until spu_sz == 0.
  first_packet = 1;
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

    // Packetized Elementary Stream (PES) Header Extension
    fprintf (fo, "\n  PACKETIZED ELEMENTARY STREAM (PES) HEADER EXTENSION\n");

    // 10 (2 bits)
    if (pos >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
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
    if (pos >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
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
    if (pos >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    pes_hdr_data_len = (ssize_t) subdata[pos];
    fprintf (fo, "    PES header data length (1 byte): %zd bytes\n", pes_hdr_data_len);
    pos++;

    // Process PES flags.

    // PTS (33 bits); PTS only, or PTS & DTS
    if ((ptsdts == 2) || (ptsdts == 3)) {

      if ((pos + 4) >= subdatalen) {
        fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
        exit (EXIT_FAILURE);
      }

      // Extract PTS.
      pes_info->pts.totalms =
        ((int64_t) (subdata[pos]     & 0x0e) << 29) |  // 0x0e = 1110
        ((int64_t) (subdata[pos + 1]) << 22) |
        ((int64_t) (subdata[pos + 2] & 0xfe) << 14) |  // 0xfe = 1111 1110
        ((int64_t) (subdata[pos + 3]) << 7) |
        ((int64_t) (subdata[pos + 4] & 0xfe) >> 1);

      // Convert to ms via integer math.
      pes_info->pts.totalms = (pes_info->pts.totalms + 45) / 90;
      mstotime (&pes_info->pts);

      fprintf (fo, "    PTS: %02d:%02d:%02d,%03d totalms: %" PRId64 "\n", pes_info->pts.h, pes_info->pts.m, pes_info->pts.s, pes_info->pts.ms, pes_info->pts.totalms);

      // Apply offsets to PTS timestamp, if requested.
      if (options->offset_flag) {
        temp = pes_info->pts.totalms + options->offset.totalms;
        if (temp < 0) temp = 0;
        pes_info->pts.totalms = temp;
        mstotime (&pes_info->pts);

        // Record new PTS in changes array.
        pts90 = (uint64_t) pes_info->pts.totalms * 90u;
        record_pes_timestamp_change (options, pos, pts90, 0x20);

      // Synchronize PTS timestamp, if requested.
      } else if (options->sync_flag) {

        // Scale PTS.
        pes_info->pts.totalms = (int64_t) (((double) options->newfirstms) + ((((double) pes_info->pts.totalms) - ((double) options->oldfirstms)) * ratio));
        mstotime (&pes_info->pts);

        // Record new PTS in changes array.
        pts90 = (uint64_t) pes_info->pts.totalms * 90u;
        record_pes_timestamp_change (options, pos, pts90, 0x20);
      }

      sub_info->start = pes_info->pts;  // Make the subtitle start time equal to the PTS.
      pes_info->dts.totalms = 0;  // Dummy value
      mstotime (&pes_info->dts);
      pos += 5;  // Move past PTS data.
      pes_hdr_data_len -= 5;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        exit (EXIT_FAILURE);
      }

    }

    // PTS and DTS (33 bits each)
    // Already extracted PTS.
    if (ptsdts == 3) {

      if ((pos + 4) >= subdatalen) {
        fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
        exit (EXIT_FAILURE);
      }
      // Extract DTS.
      pes_info->dts.totalms =
        ((int64_t) (subdata[pos]  & 0x0e)) << 29 |     // Bits 32..30; 0x0e = 1110
        ((int64_t)  subdata[pos + 1]) << 22 |          // Bits 29..22
        ((int64_t) (subdata[pos + 2] & 0xfe)) << 14 |  // Bits 21..15; 0xfe = 1111 1110
        ((int64_t)  subdata[pos + 3]) << 7 |           // Bits 14..7
        ((int64_t) (subdata[pos + 4] & 0xfe)) >> 1;    // Bits 6..0

      // Convert to ms via integer math.
      pes_info->dts.totalms = (pes_info->dts.totalms + 45) / 90;
      mstotime (&pes_info->dts);

      fprintf (fo, "    DTS: %02d:%02d:%02d,%03d totalms: %" PRId64 "\n", pes_info->dts.h, pes_info->dts.m, pes_info->dts.s, pes_info->dts.ms, pes_info->dts.totalms);

      // Apply offsets to DTS timestamp, if requested.
      if (options->offset_flag) {
        temp = pes_info->dts.totalms + options->offset.totalms;
        if (temp < 0) temp = 0;
        pes_info->dts.totalms = temp;
        mstotime (&pes_info->dts);
    
        // Record new DTS in changes array.
        dts90 = (uint64_t) pes_info->dts.totalms * 90u;
        record_pes_timestamp_change (options, pos, dts90, 0x10);

      // Synchronize DTS timestamp, if requested. 
      } else if (options->sync_flag) {

        // Scale DTS.
        pes_info->dts.totalms = (int64_t) (((double) options->newfirstms) + ((((double) pes_info->dts.totalms) - ((double) options->oldfirstms)) * ratio));
        mstotime (&pes_info->dts);

        // Record new DTS in changes array.
        dts90 = (uint64_t) pes_info->dts.totalms * 90u;
        record_pes_timestamp_change (options, pos, dts90, 0x10);
      }

      pos += 5;  
      pes_hdr_data_len -= 5;
      if (pes_hdr_data_len < 0) {
        fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
        exit (EXIT_FAILURE);
      }
    }

    // Elementary Stream Clock Reference (ESCR) (6 bytes)
    if (escr) {

      if ((pos + 5) >= subdatalen) {
        fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
        exit (EXIT_FAILURE);
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

      if ((pos + 2) >= subdatalen) {
        fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
        exit (EXIT_FAILURE);
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

      if (pos >= subdatalen) {
        fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
        exit (EXIT_FAILURE);
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

      if ((pos + 1) >= subdatalen) {
        fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
        exit (EXIT_FAILURE);
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

      if (pos >= subdatalen) {
        fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
        exit (EXIT_FAILURE);
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

        if ((pos + 15) >= subdatalen) {
          fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
          exit (EXIT_FAILURE);
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

      // Pack Header Field Length (1 byte)
      if (pack_header) {

        if (pos >= subdatalen) {
          fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
          exit (EXIT_FAILURE);
        }
        pack_header_field_len = subdata[pos];
        fprintf (fo, "    Pack Header Field Length (1 byte): %u bytes\n", pack_header_field_len);
        pos++;
        pes_hdr_data_len--;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          exit (EXIT_FAILURE);
        }
      }

      // Program Packet Sequence Counter (2 bytes)
      if (seq_counter) {

        if ((pos + 1) >= subdatalen) {
          fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
          exit (EXIT_FAILURE);
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

        if ((pos + 1) >= subdatalen) {
          fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
          exit (EXIT_FAILURE);
        }

        // P-STD Buffer Scale (1 bit)
        pstd_buffer_scale = (subdata[pos] >> 5) & 1;

        // P-STD Buffer Size (12 bits)
        pstd_buffer_size = ((subdata[pos] & 31) << 12) | subdata[pos + 1];  // 0x1f = 0001 1111

        fprintf (fo, "    P-STD Buffer:\n");
        if (!pstd_buffer_scale) {
          fprintf (fo, "      Scale (1 bit): 128 bytes\n");
        } else {
          fprintf (fo, "      Scale (1 bit): 1024 bytes\n");
        }
        fprintf (fo, "      Size (12 bits): %u bytes\n", pstd_buffer_size);
        pos += 2;
        pes_hdr_data_len -= 2;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          exit (EXIT_FAILURE);
        }
      }

      // PES Extension 2
      if (extension2) {

        if (pos >= subdatalen) {
          fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
          exit (EXIT_FAILURE);
        }

        // PES Extension 2 Length (1 byte)
        ext2_len = subdata[pos] & 0x7f;  // 0x7f = 0111 1111
        fprintf (fo, "    PES Extension 2 length (1 byte): %u bytes\n", ext2_len);

        // Reserved (1 byte)

        // Move past PES Extension 2 Field Length byte and reserved byte.
        pos += 2;
        pes_hdr_data_len -= 2;
        if (pes_hdr_data_len < 0) {
          fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
          exit (EXIT_FAILURE);
        }

        // Move past PES extension 2 data.
        if ((pos + ext2_len - 1) >= subdatalen) {
          fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
          exit (EXIT_FAILURE);
        }
        while (ext2_len > 0) {
          pos++;
          pes_hdr_data_len--;
          if (pes_hdr_data_len < 0) {
            fprintf (stderr, "pes_hdr_data_len has gone negative in parse_packets().\n");
            exit (EXIT_FAILURE);
          }
          ext2_len--;
        }
      }
    }

    // Optional stuffing bytes (0xff)
    if (pes_hdr_data_len > 0) {
      fprintf (fo, "  PES header stuffing bytes: ");

      while (pes_hdr_data_len > 0) {
        if (pos >= subdatalen) {
          fprintf (stderr, "Unexpected end of PES packet in header stuffing.\n");
          exit (EXIT_FAILURE);
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
    if (pos >= subdatalen) {
      fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    substreamid = subdata[pos];
    if ((substreamid < 0x20) || (substreamid > 0x3f)) {
      fprintf (stderr, "Not a DVD subpicture stream. Substream ID is %02x but expected anything from 0x20 to 0x3f.\n", substreamid);
      exit (EXIT_FAILURE);
    } else {
      stream = substreamid - 0x20;
      fprintf (fo, "\n  Stream ID (1 byte): 0x%02x (DVD subpicture stream, Language Index: %u)\n\n", substreamid, stream);
    }
    pos++;

    // Total size in bytes of SPU data (2 bytes)
    // Only available in first packet of chain.
    if (first_packet) {
      if ((pos + 1) >= subdatalen) {
        fprintf (stderr, "Unexpectedly reached end of PES packet in parse_packets().\n");
        exit (EXIT_FAILURE);
      }

      spu_sz = (subdata[pos] << 8) | subdata[pos + 1];
      first_packet = 0;
    }

    // Append all available SPU data from this packet to the spu_buffer.
    header_used = pos - packet_top;
    if (header_used >= pes_info->pes_packet_len) {
      spu_chunk_len = 0;
    } else {
      if (pes_info->pes_packet_len == 0) {
        spu_chunk_len = spu_sz;
      } else {
        spu_chunk_len = pes_info->pes_packet_len - header_used;
      }
    }
    if (spu_chunk_len > spu_sz) spu_chunk_len = spu_sz;  // Protect against overrun for malformed packets.
    if ((spu_pos + spu_chunk_len) > MAX_SPU_SIZE) {
      fprintf (stderr, "spu_pos + spu_chunk_len > MAX_SPU_SIZE in parse_packets().\n");
      exit (EXIT_FAILURE);
    }
    if ((pos + spu_chunk_len) >= subdatalen) {
      fprintf (stderr, "Unexpected end of PES packet while copying SPU data.\n");
      exit(EXIT_FAILURE);
    }
    memcpy (spu_buffer + spu_pos, &subdata[pos], spu_chunk_len * sizeof (char));
    spu_pos += spu_chunk_len;  // Update the spu_buffer index.
    pos += spu_chunk_len;
    spu_sz -= spu_chunk_len;

  } while ((spu_sz > 0) && (pos < subdatalen));  // Next MPEG-2 packet in chain, if any more are needed.

  return (EXIT_SUCCESS);
}
