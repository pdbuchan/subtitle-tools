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

// Parse Transport Stream PES segment which was reassembled from TS payloads.
// Reference: ETSI EN 300 743 - Digital Video Broadcasting (DVB): Subtitling Systems
int
parse_pes_segment (STATE *state, PAGE **page, SEGMENT *segment, PES *pes, FILE *fo) {

  size_t offset;
  uint8_t data_identifier, subtitle_stream_id, segment_type;
  uint16_t pid;

  pid = state->pid;

  if (segment[pid].length < 9) {
    fprintf (stderr, "Truncated PES header upon entering parse_pes_segment().\n");
    exit (EXIT_FAILURE);
  }

  // Parse PES header.
  // This will set state->stream_id.
  // parse_pes_header() will return offset which points to DVB stream start within the buffer. i.e., without PES packet header
  offset = 0;
  parse_pes_header (state, page, &offset, segment, pes, fo);

  fprintf (fo, "\nReassembled PES DVB subtitle stream (PID: 0x%04x):\n", state->pid);

  // PES packet stream ID; already reported to output file via parse_pes_header() call above.
  // We want 0xbd for private_stream_1 for DVB subtitle streams.
  // Reference: ISO/IEC 13818-1 (Table 2-22)
  if (state->stream_id == 0xbd) {

    // PES Data Identifier (1 byte)
    // For DVB subtitles, data_identifier = 0x20.
    // Reference: ETSI EN 301 192 (Table 2)
    if (offset >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_segment().\n");
      exit (EXIT_FAILURE);
    }
    data_identifier = segment[pid].buffer[offset];
    data_ids (state, data_identifier, fo);  // Data Identifiers for DVB Transport Streams
    offset++;
    if (data_identifier != 0x20) {
      return (EXIT_SUCCESS);
    }

    // Subtitle stream ID (1 byte)
    // For DVB subtitles, subtitle_stream_id = 0x00.
    // Reference: ETSI EN 300 743 (Section 7.1)
    if (offset >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_segment().\n");
      exit (EXIT_FAILURE);
    }
    subtitle_stream_id = segment[pid].buffer[offset];
    if (subtitle_stream_id != 0x00) {
      fprintf (fo, "  Subtitle Stream ID (1 byte): 0x%02x (must be 0x00 for DVB subtitle streams)\n", subtitle_stream_id);
      return (EXIT_SUCCESS);
    }
    fprintf (fo, "  Subtitle Stream ID (1 byte): 0x%02x\n", subtitle_stream_id);
    offset++;

    // Parse DVB segments in PES segment.
    // Each individual DVB subtitle segment starts with sync_byte == 0x0f.
    // end_of_PES_data_field_marker == 0xff
    if (offset >= (MAX_BUFFERLEN + 1)) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_segment().\n");
      exit (EXIT_FAILURE);
    }
    while ((segment[pid].buffer[offset] == 0x0f) && (segment[pid].buffer[offset] != 0xff) && (offset < segment[pid].length)) {

      if ((offset + 1) >= (MAX_BUFFERLEN + 1)) {
        fprintf (stderr, "Unexpectedly reached end of segment in parse_pes_segment().\n");
        exit (EXIT_FAILURE);
      }
      segment_type = segment[pid].buffer[offset + 1];

      switch (segment_type) {

        case 0x10:  // Page Composition Segment (PCS)
          parse_pcs (state, page, &offset, segment, pes, fo);
          break;

        case 0x11:  // Region Composition Segment (RCS)
          parse_rcs (state, page, &offset, segment, fo);
          break;

        case 0x12:  // CLUT Definition Segment (CDS)
          parse_cds (state, page, &offset, segment, fo);
          break;

        case 0x13:  // Object Data Segment (ODS)
          parse_ods (state, page, &offset, segment, fo);
          break;

        case 0x14:  // Display Definition Segment (DDS)
          parse_dds (state, page, &offset, segment, fo);
          break;

        case 0x15:  // Disparity Signalling Segment (DSS)
          parse_dss (state, page, &offset, segment, fo);
          break;

        case 0x80:  // End of Display Set Segment (END)
          parse_end (state, page, &offset, segment, fo);
          break;

        default:
          if ((segment_type >= 0x16) && (segment_type <= 0x7f)) {
            fprintf (fo, "Reserved for future use\n");
            break;

          } else if ((segment_type >= 0x81) && (segment_type <= 0xef)) {
            fprintf (fo, "Private data\n");
            break;

          } else {
            fprintf (stderr, "Unknown DVB subtitle segment type: 0x%02x\n", segment_type);
            exit (EXIT_FAILURE);
          }

      }  // End switch segment_type

    }  // End while not end of buffer
  }  // End if Stream ID == 0xbd (private_stream_1)

  return (EXIT_SUCCESS);
}
