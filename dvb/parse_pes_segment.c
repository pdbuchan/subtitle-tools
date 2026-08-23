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

// Parse a Transport Stream PES packet which has been reassembled from TS
// payloads and, when it is a DVB subtitle PES, dispatch each contained DVB
// subtitle segment to the appropriate parser.
// Reference: ETSI EN 300 743.
int
parse_pes_segment (STATE *state, PAGE **page, SEGMENT *segment, PES *pes, FILE *fo) {

  size_t offset, seglen;
  uint8_t data_identifier, subtitle_stream_id, segment_type;
  uint16_t pid;
  int rc;

  pid = state->pid;

  // A normal MPEG-2 PES header requires at least the six-byte fixed prefix and
  // three bytes which begin the optional PES header extension.
  if (segment[pid].length < 9) {
    fprintf (stderr, "Truncated PES packet.\n");
    return (EXIT_FAILURE);
  }

  // parse_pes_header() leaves offset pointing at the first PES payload byte
  // and stores the Stream ID in state->stream_id.
  offset = 0;
  if (parse_pes_header (state, page, &offset, segment, pes, fo) != EXIT_SUCCESS) {
    return (EXIT_FAILURE);
  }

  fprintf (fo, "\nReassembled PES DVB subtitle stream (PID: 0x%04x):\n", pid);

  // DVB subtitles are carried in private_stream_1.
  if (state->stream_id != 0xbd) {
    return (EXIT_SUCCESS);
  }

  // DVB subtitle PES payload begins with data_identifier and
  // subtitle_stream_id.
  if (!bytes_available (offset, 2, segment[pid].length)) {
    fprintf (stderr, "Truncated DVB subtitle PES payload.\n");
    return (EXIT_FAILURE);
  }

  // PES Data Identifier (1 byte). DVB subtitles use 0x20.
  // Reference: ETSI EN 301 192.
  data_identifier = segment[pid].buffer[offset++];
  data_ids (state, data_identifier, fo);
  if (data_identifier != 0x20) {
    return (EXIT_SUCCESS);
  }

  // Subtitle Stream ID (1 byte). EN 300 743 specifies 0x00.
  subtitle_stream_id = segment[pid].buffer[offset++];
  fprintf (fo, "  Subtitle Stream ID (1 byte): 0x%02x\n", subtitle_stream_id);
  if (subtitle_stream_id != 0x00) {
    return (EXIT_SUCCESS);
  }

  // Parse consecutive DVB subtitle segments. Each begins with sync_byte 0x0f
  // and has a six-byte segment header containing segment_type and length.
  while (offset < segment[pid].length) {

    // end_of_PES_data_field_marker
    if (segment[pid].buffer[offset] == 0xff) {
      offset++;
      break;
    }

    // Test the bound before dereferencing offset+1. The original expression
    // dereferenced buffer[offset] before establishing that offset was valid.
    if (!bytes_available (offset, 6, segment[pid].length)) {
      fprintf (stderr, "Truncated DVB subtitle segment header.\n");
      return (EXIT_FAILURE);
    }
    if (segment[pid].buffer[offset] != 0x0f) {
      fprintf (stderr, "Expected DVB subtitle sync byte at PES offset %zu.\n", offset);
      return (EXIT_FAILURE);
    }

    segment_type = segment[pid].buffer[offset + 1];
    seglen = (size_t) (((uint16_t) segment[pid].buffer[offset + 4] << 8) |
                       segment[pid].buffer[offset + 5]);
    if (!bytes_available (offset + 6, seglen, segment[pid].length)) {
      fprintf (stderr, "DVB segment length exceeds available PES data.\n");
      return (EXIT_FAILURE);
    }

    // Dispatch according to the EN 300 743 segment_type value.
    switch (segment_type) {

      case 0x10:
        rc = parse_pcs (state, page, &offset, segment, pes, fo);
        break;

      case 0x11:
        rc = parse_rcs (state, page, &offset, segment, fo);
        break;

      case 0x12:
        rc = parse_cds (state, page, &offset, segment, fo);
        break;

      case 0x13:
        rc = parse_ods (state, page, &offset, segment, fo);
        break;

      case 0x14:
        rc = parse_dds (state, page, &offset, segment, fo);
        break;

      case 0x15:
        rc = parse_dss (state, page, &offset, segment, fo);
        break;

      case 0x80:
        rc = parse_end (state, page, &offset, segment, fo);
        break;

      default:
        // Reserved/private segment types still have the ordinary six-byte
        // DVB segment header. Skip the declared body so the loop progresses.
        if ((segment_type >= 0x16 && segment_type <= 0x7f) ||
            (segment_type >= 0x81 && segment_type <= 0xef)) {
          fprintf (fo, "\n  Skipping %s DVB segment type 0x%02x (%zu bytes)\n", segment_type <= 0x7f ? "reserved" : "private", segment_type, seglen);
          offset += 6 + seglen;
          rc = EXIT_SUCCESS;
        } else {
          fprintf (stderr, "Unknown DVB subtitle segment type: 0x%02x\n", segment_type);
          return (EXIT_FAILURE);
        }
        break;
    }

    if (rc != EXIT_SUCCESS) {
      return (EXIT_FAILURE);
    }
  }

  return (EXIT_SUCCESS);
}
