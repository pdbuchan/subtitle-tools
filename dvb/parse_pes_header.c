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

// Return the 33-bit MPEG PTS/DTS value stored in a five-byte timestamp.
// The caller has already established that all five bytes are available.
static int
read_timestamp (const uint8_t *p, uint8_t expected_prefix, int64_t *totalms) {

  uint64_t ticks;

  // The four high-order prefix bits distinguish PTS-only, PTS-with-DTS, and
  // DTS fields. MPEG also requires marker bits in bytes 0, 2, and 4.
  if (((p[0] >> 4) & 0x0f) != expected_prefix ||
      !(p[0] & 1) || !(p[2] & 1) || !(p[4] & 1)) {
    return (EXIT_FAILURE);
  }

  // Reassemble the 33-bit timestamp from its five-byte MPEG representation.
  ticks = ((uint64_t) (p[0] & 0x0e) << 29) |
          ((uint64_t) p[1] << 22) |
          ((uint64_t) (p[2] & 0xfe) << 14) |
          ((uint64_t) p[3] << 7) |
          ((uint64_t) (p[4] & 0xfe) >> 1);

  // PTS and DTS use a 90 kHz clock. Add half a millisecond before integer
  // division so the result is rounded to the nearest millisecond.
  *totalms = (int64_t) ((ticks + 45) / 90);
  return (EXIT_SUCCESS);
}

// Parse a Packetized Elementary Stream (PES) packet header.
// Reference: ISO/IEC 13818-1.
int
parse_pes_header (STATE *state, PAGE **page, size_t *offset, SEGMENT *segment, PES *pes, FILE *fo) {

  size_t header_end, pack_len, ext2_len, i;
  uint16_t pid, crc;
  uint8_t flags1, flags2, ptsdts_flag;
  uint8_t escr_flag, esrate_flag, dsm_flag, copy_info_flag;
  uint8_t crc_flag, extension_flag;
  uint8_t ext_flags, private_data_flag, pack_flag, seq_flag;
  uint8_t pstd_flag, ext2_flag;
  uint32_t esrate_raw, esrate;
  uint64_t escr_base, escr_27mhz;
  uint16_t escr_ext;

  (void) page;
  pid = state->pid;

  fprintf (fo, "\n  PACKETIZED ELEMENTARY STREAM (PES) HEADER\n");

  // A normal MPEG-2 PES header begins with a six-byte fixed prefix followed by
  // three bytes of optional-header control information.
  if (!bytes_available (*offset, 9, segment[pid].length)) {
    fprintf (stderr, "Truncated PES header.\n");
    return (EXIT_FAILURE);
  }

  // Packet Start Code Prefix (3 bytes): 0x000001.
  if (segment[pid].buffer[*offset] != 0x00 ||
      segment[pid].buffer[*offset + 1] != 0x00 ||
      segment[pid].buffer[*offset + 2] != 0x01) {
    fprintf (stderr, "Invalid PES packet start-code prefix.\n");
    return (EXIT_FAILURE);
  }
  fprintf (fo, "    Start Code (3 bytes): 00 00 01\n");
  *offset += 3;

  // Stream ID (1 byte).
  state->stream_id = segment[pid].buffer[(*offset)++];
  stream_ids (state, fo);

  // PES Packet Length (2 bytes). A value of zero is permitted for an
  // unbounded PES packet and is handled by build_pes_segment().
  pes->packet_length = (size_t) (((uint16_t) segment[pid].buffer[*offset] << 8) | segment[pid].buffer[*offset + 1]);
  *offset += 2;
  fprintf (fo, "    PES packet length (2 bytes): %zu bytes\n", pes->packet_length);

  // PES optional-header control bytes. flags1 contains scrambling, priority,
  // data-alignment, copyright, and copy/original bits. flags2 contains flags
  // indicating which variable-length optional fields follow.
  flags1 = segment[pid].buffer[(*offset)++];
  flags2 = segment[pid].buffer[(*offset)++];

  // PES Header Data Length (1 byte): number of bytes occupied by all optional
  // fields and stuffing which follow these three control bytes.
  pes->hdr_data_len = segment[pid].buffer[(*offset)++];

  // MPEG-2 PES optional headers begin with binary 10 in the two MSBs.
  if ((flags1 >> 6) != 2) {
    fprintf (stderr, "PES optional-header prefix is not binary 10.\n");
    return (EXIT_FAILURE);
  }

  // Establish one fixed end position for the complete optional-header area.
  // Every optional field below is checked against this boundary before being
  // consumed. This avoids unsigned length subtraction and possible wraparound.
  if (!bytes_available (*offset, pes->hdr_data_len, segment[pid].length)) {
    fprintf (stderr, "PES_header_data_length exceeds available PES data.\n");
    return (EXIT_FAILURE);
  }
  header_end = *offset + pes->hdr_data_len;

  fprintf (fo, "\n  PACKETIZED ELEMENTARY STREAM (PES) HEADER EXTENSION\n");

  // PES scrambling control (2 bits), PES priority (1 bit), data alignment
  // indicator (1 bit), copyright (1 bit), and original/copy (1 bit).
  fprintf (fo, "    PES Scrambling Control (2 bits): 0x%02x\n", (flags1 >> 4) & 3);
  fprintf (fo, "    PES priority (1 bit): %u\n", (flags1 >> 3) & 1);
  fprintf (fo, "    Data alignment indicator (1 bit): %u\n", (flags1 >> 2) & 1);
  fprintf (fo, "    Copyright (1 bit): %u\n", (flags1 >> 1) & 1);
  fprintf (fo, "    Copy/Original (1 bit): %u\n", flags1 & 1);

  ptsdts_flag = flags2 >> 6;
  escr_flag = (flags2 >> 5) & 1;
  esrate_flag = (flags2 >> 4) & 1;
  dsm_flag = (flags2 >> 3) & 1;
  copy_info_flag = (flags2 >> 2) & 1;
  crc_flag = (flags2 >> 1) & 1;
  extension_flag = flags2 & 1;

  // PTS_DTS_flags == binary 01 is reserved and therefore invalid.
  if (ptsdts_flag == 1) {
    fprintf (stderr, "Invalid PTS_DTS_flags value of binary 01.\n");
    return (EXIT_FAILURE);
  }
  fprintf (fo, "    PTS/DTS flag (2 bits): %u\n", ptsdts_flag);
  fprintf (fo, "    Elementary Stream Clock Reference flag (1 bit): %u\n", escr_flag);
  fprintf (fo, "    Elementary Stream Rate flag (1 bit): %u\n", esrate_flag);
  fprintf (fo, "    DSM Trick Mode flag (1 bit): %u\n", dsm_flag);
  fprintf (fo, "    Additional Copy Info flag (1 bit): %u\n", copy_info_flag);
  fprintf (fo, "    CRC flag (1 bit): %u\n", crc_flag);
  fprintf (fo, "    PES Extension flag (1 bit): %u\n", extension_flag);
  fprintf (fo, "    PES header data length (1 byte): %zu bytes\n", pes->hdr_data_len);

  // Presentation Timestamp (PTS), 33 bits stored in 5 bytes.
  if (ptsdts_flag == 2 || ptsdts_flag == 3) {
    if (!bytes_available (*offset, 5, header_end) || read_timestamp (&segment[pid].buffer[*offset], ptsdts_flag == 2 ? 2 : 3, &pes->pts.totalms) != EXIT_SUCCESS) {
      fprintf (stderr, "Invalid or truncated PTS field.\n");
      return (EXIT_FAILURE);
    }
    mstotime (&pes->pts);
    fprintf (fo, "    PTS: %02d:%02d:%02d,%03d totalms: %" PRId64 "\n", pes->pts.h, pes->pts.m, pes->pts.s, pes->pts.ms, pes->pts.totalms);
    *offset += 5;
  }

  // Decoding Timestamp (DTS), present only when PTS_DTS_flags == binary 11.
  // The PTS in that case used prefix 0011; the DTS uses prefix 0001.
  pes->dts.totalms = 0;
  if (ptsdts_flag == 3) {
    if (!bytes_available (*offset, 5, header_end) || read_timestamp (&segment[pid].buffer[*offset], 1, &pes->dts.totalms) != EXIT_SUCCESS) {
      fprintf (stderr, "Invalid or truncated DTS field.\n");
      return (EXIT_FAILURE);
    }
    mstotime (&pes->dts);
    fprintf (fo, "    DTS: %02d:%02d:%02d,%03d totalms: %" PRId64 "\n", pes->dts.h, pes->dts.m, pes->dts.s, pes->dts.ms, pes->dts.totalms);
    *offset += 5;
  }

  // Elementary Stream Clock Reference (ESCR), 33-bit base plus 9-bit
  // extension. The resulting clock runs at 27 MHz.
  if (escr_flag) {
    if (!bytes_available (*offset, 6, header_end)) {
      fprintf (stderr, "Truncated ESCR field.\n");
      return (EXIT_FAILURE);
    }
    escr_base =
        ((uint64_t) (segment[pid].buffer[*offset] & 0x38) << 27) |
        ((uint64_t) (segment[pid].buffer[*offset] & 0x03) << 28) |
        ((uint64_t) segment[pid].buffer[*offset + 1] << 20) |
        ((uint64_t) (segment[pid].buffer[*offset + 2] & 0xf8) << 12) |
        ((uint64_t) (segment[pid].buffer[*offset + 2] & 0x03) << 13) |
        ((uint64_t) segment[pid].buffer[*offset + 3] << 5) |
        ((uint64_t) segment[pid].buffer[*offset + 4] >> 3);
    escr_ext = (uint16_t)
        (((segment[pid].buffer[*offset + 4] & 3) << 7) |
         ((segment[pid].buffer[*offset + 5] & 0xfe) >> 1));

    // One 90 kHz base tick equals 300 ticks at 27 MHz.
    escr_27mhz = escr_base * 300 + escr_ext;
    fprintf (fo, "    ESCR: %" PRIu64 " ms\n", (escr_27mhz + 13500) / 27000);
    *offset += 6;
  }

  // Elementary Stream Rate (3 bytes). The 22-bit ES_rate value is expressed
  // in units of 50 bytes/second; report it here as kB/s.
  if (esrate_flag) {
    if (!bytes_available (*offset, 3, header_end)) {
      fprintf (stderr, "Truncated ES_rate field.\n");
      return (EXIT_FAILURE);
    }
    esrate_raw = ((uint32_t) segment[pid].buffer[*offset] << 16) |
                 ((uint32_t) segment[pid].buffer[*offset + 1] << 8) |
                 segment[pid].buffer[*offset + 2];
    esrate = (esrate_raw >> 1) & 0x3fffff;
    fprintf (fo, "    Elementary Stream Rate (3 bytes): %u kB/s\n", (esrate * 50 + 500) / 1000);
    *offset += 3;
  }

  // DSM Trick Mode (1 byte). DVB subtitles do not normally use this field,
  // but it must still be consumed when the flag is set so later fields remain
  // aligned correctly.
  if (dsm_flag) {
    if (!bytes_available (*offset, 1, header_end)) {
      fprintf (stderr, "Truncated DSM trick-mode field.\n");
      return (EXIT_FAILURE);
    }
    fprintf (fo, "    DSM Trick Mode byte: 0x%02x\n", segment[pid].buffer[*offset]);
    (*offset)++;
  }

  // Additional Copy Info (1 byte). The high bit is a marker bit; the lower
  // seven bits carry the additional-copy-info value.
  if (copy_info_flag) {
    if (!bytes_available (*offset, 1, header_end)) {
      fprintf (stderr, "Truncated additional-copy-info field.\n");
      return (EXIT_FAILURE);
    }
    fprintf (fo, "    Additional Copy Info: 0x%02x\n", segment[pid].buffer[*offset] & 0x7f);
    (*offset)++;
  }

  // Previous PES Packet CRC (2 bytes).
  if (crc_flag) {
    if (!bytes_available (*offset, 2, header_end)) {
      fprintf (stderr, "Truncated previous-PES CRC field.\n");
      return (EXIT_FAILURE);
    }
    crc = (uint16_t) (((uint16_t) segment[pid].buffer[*offset] << 8) | segment[pid].buffer[*offset + 1]);
    fprintf (fo, "    Previous PES packet CRC (2 bytes): 0x%04x\n", crc);
    *offset += 2;
  }

  // Packetized Elementary Stream Header Extension.
  if (extension_flag) {
    if (!bytes_available (*offset, 1, header_end)) {
      fprintf (stderr, "Truncated PES extension flags.\n");
      return (EXIT_FAILURE);
    }
    ext_flags = segment[pid].buffer[(*offset)++];
    private_data_flag = ext_flags >> 7;
    pack_flag = (ext_flags >> 6) & 1;
    seq_flag = (ext_flags >> 5) & 1;
    pstd_flag = (ext_flags >> 4) & 1;
    ext2_flag = ext_flags & 1;

    // The middle three bits of ext_flags are reserved; the remaining flags
    // announce the optional extension fields parsed below.
    fprintf (fo, "    PES Private Data flag (1 bit): %u\n", private_data_flag);
    fprintf (fo, "    Pack Header Field flag (1 bit): %u\n", pack_flag);
    fprintf (fo, "    Program Packet Sequence Counter flag (1 bit): %u\n", seq_flag);
    fprintf (fo, "    P-STD Buffer flag (1 bit): %u\n", pstd_flag);
    fprintf (fo, "    PES Extension 2 flag (1 bit): %u\n", ext2_flag);

    // PES Private Data (16 bytes).
    if (private_data_flag) {
      if (!bytes_available (*offset, 16, header_end)) {
        fprintf (stderr, "Truncated PES private data.\n");
        return (EXIT_FAILURE);
      }
      fprintf (fo, "    PES Private Data: ");
      for (i = 0; i < 16; i++) {
        fprintf (fo, "%02x%s", segment[pid].buffer[*offset + i], i == 15 ? "\n" : " ");
      }
      *offset += 16;
    }

    // Pack Header Field. The first byte gives the number of following pack
    // header bytes; skip exactly that amount rather than treating the length
    // byte itself as the entire field.
    if (pack_flag) {
      if (!bytes_available (*offset, 1, header_end)) {
        fprintf (stderr, "Truncated pack-header-field length.\n");
        return (EXIT_FAILURE);
      }
      pack_len = segment[pid].buffer[(*offset)++];
      fprintf (fo, "    Pack Header Field Length: %zu bytes\n", pack_len);
      if (!bytes_available (*offset, pack_len, header_end)) {
        fprintf (stderr, "Pack header exceeds PES_header_data_length.\n");
        return (EXIT_FAILURE);
      }
      *offset += pack_len;
    }

    // Program Packet Sequence Counter (2 bytes).
    if (seq_flag) {
      if (!bytes_available (*offset, 2, header_end)) {
        fprintf (stderr, "Truncated program-packet-sequence-counter field.\n");
        return (EXIT_FAILURE);
      }
      fprintf (fo, "    Packet Sequence Counter: %u\n", segment[pid].buffer[*offset] & 0x7f);
      fprintf (fo, "    MPEG-1 / MPEG-2 Identifier: %u\n", (segment[pid].buffer[*offset + 1] >> 6) & 1);
      fprintf (fo, "    Original Stuffing Length: %u bytes\n", segment[pid].buffer[*offset + 1] & 0x3f);
      *offset += 2;
    }

    // P-STD Buffer (2 bytes): scale flag plus 13-bit buffer size.
    if (pstd_flag) {
      uint16_t pstd_size;
      if (!bytes_available (*offset, 2, header_end)) {
        fprintf (stderr, "Truncated P-STD buffer field.\n");
        return (EXIT_FAILURE);
      }
      pstd_size = (uint16_t) (((segment[pid].buffer[*offset] & 0x1f) << 8) | segment[pid].buffer[*offset + 1]);
      fprintf (fo, "    P-STD Buffer Scale: %u\n", (segment[pid].buffer[*offset] >> 5) & 1);
      fprintf (fo, "    P-STD Buffer Size: %u\n", pstd_size);
      *offset += 2;
    }

    // PES Extension 2: one length byte followed by the declared number of
    // extension bytes.
    if (ext2_flag) {
      if (!bytes_available (*offset, 1, header_end)) {
        fprintf (stderr, "Truncated PES extension-2 length.\n");
        return (EXIT_FAILURE);
      }
      ext2_len = segment[pid].buffer[(*offset)++] & 0x7f;
      fprintf (fo, "    PES Extension 2 Length: %zu bytes\n", ext2_len);
      if (!bytes_available (*offset, ext2_len, header_end)) {
        fprintf (stderr, "PES extension 2 exceeds PES_header_data_length.\n");
        return (EXIT_FAILURE);
      }
      *offset += ext2_len;
    }
  }

  // Bytes not consumed by optional fields are PES stuffing bytes. By using
  // header_end rather than unsigned subtraction, malformed flag/length
  // combinations cannot wrap a size_t and run into the payload.
  if (*offset > header_end) {
    fprintf (stderr, "PES optional fields exceed PES_header_data_length.\n");
    return (EXIT_FAILURE);
  }
  if (*offset < header_end) {
    fprintf (fo, "    PES header stuffing bytes: ");
    while (*offset < header_end) {
      fprintf (fo, "%02x%s", segment[pid].buffer[*offset], (*offset + 1 == header_end) ? "\n" : " ");
      (*offset)++;
    }
  }

  return (EXIT_SUCCESS);
}
