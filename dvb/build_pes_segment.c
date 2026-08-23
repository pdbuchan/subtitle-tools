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

// Add a TS packet payload to the current PID's PES reassembly buffer.
// PES packets may span many TS packets. A bounded PES is complete when its
// declared PES_packet_length has arrived; PES_packet_length == 0 denotes an
// unbounded PES which instead terminates when the next PUSI is encountered.
int
build_pes_segment (STATE *state, PAGE **page, uint8_t *tsdata, size_t tslen, size_t ts_payloadlen, SEGMENT *segment, PES *pes, FILE *fo) {

  size_t payload_end, total;
  uint16_t pid = state->pid;

  // Validate the payload range before computing its end position.
  if (!bytes_available (state->ts_index, ts_payloadlen, tslen)) {
    fprintf (stderr, "TS payload exceeds input in build_pes_segment().\n");
    return (EXIT_FAILURE);
  }
  payload_end = state->ts_index + ts_payloadlen;

  // Allocate one persistent PES reassembly buffer per PID on first use.
  if (!segment[pid].buffer) {
    segment[pid].buffer = allocate_u8mem (MAX_BUFFERLEN);
  }

  // PUSI marks the start of a new PES packet. If an unbounded PES is already
  // being collected, the new start also marks the end of that previous PES.
  if (state->pusi) {
    if (pes->collecting && segment[pid].length > 0) {
      // packet_length == 0 is explicitly unbounded and terminates at the next
      // PES start indicator.
      if (parse_pes_segment (state, page, segment, pes, fo) != EXIT_SUCCESS) {
        return (EXIT_FAILURE);
      }
    }
    pes->collecting = 1;
    pes->total_length = PES_LEN_UNBOUNDED;
    pes->packet_length = 0;
    segment[pid].length = 0;
    state->pusi = 0;
  }

  // Payload received before the first PES start indicator cannot be assigned
  // safely to a PES packet, so ignore it.
  if (!pes->collecting) {
    state->ts_index = payload_end;
    return (EXIT_SUCCESS);
  }

  // Append this TS payload until it is exhausted or a bounded PES reaches its
  // declared total length.
  while (state->ts_index < payload_end) {
    if (segment[pid].length >= MAX_BUFFERLEN) {
      fprintf (stderr, "PES packet exceeds %d-byte reassembly buffer.\n",
               MAX_BUFFERLEN);
      return (EXIT_FAILURE);
    }

    segment[pid].buffer[segment[pid].length++] = tsdata[state->ts_index++];

    // PES_packet_length becomes available after the fixed six-byte prefix has
    // been collected. For a bounded PES the complete size is 6 plus that
    // length field.
    if (pes->total_length == PES_LEN_UNBOUNDED && segment[pid].length >= 6) {
      pes->packet_length = (size_t) (((uint16_t) segment[pid].buffer[4] << 8) | segment[pid].buffer[5]);
      if (pes->packet_length != 0) {
        total = 6 + pes->packet_length;
        if (total > MAX_BUFFERLEN) {
          fprintf (stderr, "Declared PES packet length is too large.\n");
          return (EXIT_FAILURE);
        }
        pes->total_length = (ssize_t) total;
      }
    }

    // A bounded PES can be parsed as soon as exactly the declared number of
    // bytes has been reassembled.
    if (pes->total_length != PES_LEN_UNBOUNDED &&
        segment[pid].length == (size_t) pes->total_length) {
      if (parse_pes_segment (state, page, segment, pes, fo) != EXIT_SUCCESS) {
        return (EXIT_FAILURE);
      }
      pes->collecting = 0;
      pes->total_length = PES_LEN_UNBOUNDED;
      segment[pid].length = 0;
      // A new PES may not begin without PUSI, so ignore remaining bytes in
      // this TS payload rather than treating stuffing as a new PES packet.
      state->ts_index = payload_end;
      break;
    }
  }

  return (EXIT_SUCCESS);
}
