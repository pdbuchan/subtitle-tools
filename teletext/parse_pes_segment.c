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

#include "teletext.h"

// Parse a reassembled DVB Teletext PES packet.
// Reference: ETSI EN 300 472 clauses 4.2 to 4.4.
int
parse_pes_segment (STATE *state, TTX_CONTEXT *ttx, PAT *pat, SEGMENT *segment, PES *pes, FILE *fo) {

  size_t offset, unit_length;
  uint8_t data_identifier, data_unit_id;
  uint16_t pid;

  // state->pid identifies the PID whose PES reassembly buffer is being parsed.
  pid = state->pid;

  // This Teletext parsing path expects at least the 6-byte PES packet prefix
  // plus the first three bytes of the optional-header area examined by
  // parse_pes_header(). Reject obviously incomplete reassemblies up front.
  if (segment[pid].length < 9) {
    fprintf (stderr, "Truncated PES packet.\n");
    return (EXIT_FAILURE);
  }

  // Parse the generic MPEG-2 PES header first. On return, offset points to the
  // first byte of the PES payload, which for DVB Teletext is data_identifier.
  offset = 0;
  if (parse_pes_header (state, &offset, segment, pes, fo) != EXIT_SUCCESS) {
    return (EXIT_FAILURE);
  }

  fprintf (fo, "\nReassembled PES Teletext stream (PID: 0x%04x):\n", pid);

  // EN 300 472 requires private_stream_1 for Teletext.
  if (state->stream_id != 0xbd) {
    fprintf (fo, "  Not private_stream_1; ignoring this PES payload.\n");
    return (EXIT_SUCCESS);
  }

  // These are Teletext-specific constraints layered on top of ordinary MPEG-2
  // PES syntax. Report non-conformance but continue where decoding is safe so
  // the analyzer remains useful on imperfect recordings and real-world muxes.
  if (pes->packet_length != 0 && ((pes->packet_length + 6) % 184) != 0) {
    fprintf (fo, "  WARNING: EN 300 472 requires PES_packet_length = (N * 184) - 6.\n");
  }
  if (!pes->data_alignment_indicator) {
    fprintf (fo, "  WARNING: EN 300 472 requires data_alignment_indicator = 1.\n");
  }
  if (pes->hdr_data_len != 0x24) {
    fprintf (fo, "  WARNING: EN 300 472 requires PES_header_data_length = 0x24.\n");
  }

  // The Teletext payload begins with a one-byte data_identifier followed by
  // zero or more data units.
  if (!bytes_available (offset, 1, segment[pid].length)) {
    fprintf (stderr, "Truncated Teletext PES payload.\n");
    return (EXIT_FAILURE);
  }

  data_identifier = segment[pid].buffer[offset++];
  data_ids (data_identifier, fo);

  // EN 300 472 assigns 0x10..0x1f to EBU data. Other identifiers are not
  // decoded as Teletext, even if they happen to occur on a Teletext PID.
  if (data_identifier < 0x10 || data_identifier > 0x1f) {
    return (EXIT_SUCCESS);
  }

  // Each remaining payload item is framed as:
  //
  //   data_unit_id      1 byte
  //   data_unit_length  1 byte
  //   data_unit         data_unit_length bytes
  //
  // The loop also encounters stuffing and reserved/user-defined units, which
  // are reported and skipped without attempting Teletext decoding.
  while (offset < segment[pid].length) {
    if (!bytes_available (offset, 2, segment[pid].length)) {
      fprintf (stderr, "Truncated Teletext data-unit header.\n");
      return (EXIT_FAILURE);
    }

    // Read the two-byte data-unit header. ndata_units counts every unit in the
    // Teletext PES payload, not only the units that contain EBU Teletext data.
    data_unit_id = segment[pid].buffer[offset++];
    unit_length = segment[pid].buffer[offset++];
    ttx->ndata_units++;

    fprintf (fo, "\n  Data Unit %zu:\n", ttx->ndata_units);
    fprintf (fo, "    Data Unit ID (1 byte): 0x%02x", data_unit_id);
    switch (data_unit_id) {

      case 0x02:
        fprintf (fo, " EBU Teletext non-subtitle data\n");
        break;

      case 0x03:
        fprintf (fo, " EBU Teletext subtitle data\n");
        break;

      case 0xff:
        fprintf (fo, " stuffing\n");
        break;

      default:
        fprintf (fo, " reserved/user-defined\n");
        break;
    }  // End switch
    fprintf (fo, "    Data Unit Length (1 byte): %zu bytes\n", unit_length);

    // Validate the complete unit before interpreting or skipping its payload.
    if (!bytes_available (offset, unit_length, segment[pid].length)) {
      fprintf (stderr, "Teletext data-unit length exceeds PES payload.\n");
      return (EXIT_FAILURE);
    }

    // IDs 0x02 and 0x03 both contain the standard 44-byte EBU Teletext data
    // unit. The distinction is whether the unit is advertised as ordinary
    // Teletext data or subtitle data; both use the same packet-level decoder.
    if (data_unit_id == 0x02 || data_unit_id == 0x03) {
      if (unit_length != 0x2c) {
        fprintf (stderr, "EBU Teletext data unit has length %zu; expected 0x2c.\n", unit_length);
        return (EXIT_FAILURE);
      }
      if (parse_teletext_data_unit (state, ttx, pat, data_unit_id, segment[pid].buffer + offset, unit_length, pes, fo) != EXIT_SUCCESS) {
        return (EXIT_FAILURE);
      }
      // Count only successfully parsed EBU Teletext units here.
      ttx->nteletext_units++;
    }

    // Advance over the payload for every unit type, including stuffing and
    // reserved/user-defined units that were not passed to the Teletext parser.
    offset += unit_length;
  }

  return (EXIT_SUCCESS);
}
