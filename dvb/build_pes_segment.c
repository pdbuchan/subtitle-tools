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

// Add packet payload to appropriate PES segment buffer, differentiated by PID.
// If current stream segment is complete, call segment parser.
int
build_pes_segment (STATE *state, PAGE **page, uint8_t *tsdata, size_t tslen, int ts_payloadlen, SEGMENT *segment, PES *pes, FILE *fo) {

  size_t payload_end;
  uint16_t pid;

  pid = state->pid;

  // Allocate memory for PES segment buffer, if not yet allocated.
  // Add 1 padding byte to MAX_BUFFERLEN to prevent overflow in get_8bits() function at end of buffer.
  if (!segment[pid].buffer) {  // Not allocated yet.
    segment[pid].buffer = allocate_u8mem (MAX_BUFFERLEN + 1);
  }

  // Compute end of TS payload
  payload_end = state->ts_index + (size_t) ts_payloadlen;
  if (payload_end > tslen) {
      fprintf (stderr, "TS payload length exceeds TS packet length in build_pes_segment().\n");
      exit (EXIT_FAILURE);
  }

  // Append PES payload bytes to segment buffer.
  while (state->ts_index < payload_end) {

    // PES start
    // If we were collecting packets for a segment, this indicates it is done, and new segment begins.
    if (state->pusi == 1) {

      // We were in the process of collectng packets for a segment.
      // Process that segment.
      if ((pes->collecting == 1) && (segment[pid].length > 0)) {
        parse_pes_segment (state, page, segment, pes, fo);
        pes->collecting = 0;
      }

      // Reset state for a new PES segment starting in current TS packet payload.
      pes->collecting = 1;
      pes->total_length = PES_LEN_UNBOUNDED;  // Default to unbounded
      memset (segment[pid].buffer, 0, MAX_BUFFERLEN * sizeof (uint8_t));
      segment[pid].length = 0;

      // Clear PUSI flag for this TS packet so we don't repeatedly trigger.
      state->pusi = 0;
    }

    // Stop if we are not collecting a PES.
    if (!pes->collecting) {
      break;
    }

    // Append PES payload bytes to segment buffer.
    if (segment[pid].length < MAX_BUFFERLEN) {
      segment[pid].buffer[segment[pid].length] = tsdata[state->ts_index];
      segment[pid].length++;
      state->ts_index++;
    } else {
      fprintf (fo, "PES segment length is longer than buffer size.\n");
      exit (EXIT_FAILURE);
    }

    // Extract PES_packet_length once we have at least 6 bytes.
    if ((pes->total_length == PES_LEN_UNBOUNDED) && (segment[pid].length >= 6)) {
      pes->packet_length = (segment[pid].buffer[4] << 8) | segment[pid].buffer[5];

      // Bounded PES
      if (pes->packet_length > 0) {
        pes->total_length = 6 + pes->packet_length;  // Include: Start Code (3 bytes), Stream ID (1 byte), PES_packet_length (2 bytes).

      // Unbounded (pes->packet_len == 0)
      } else {
        pes->total_length = PES_LEN_UNBOUNDED;  // Set to unbounded
      }
    }

    // If we have a bounded PES and it's complete, parse it.
    if ((pes->total_length != PES_LEN_UNBOUNDED) && (segment[pid].length == pes->total_length)) {

      parse_pes_segment (state, page, segment, pes, fo);

      // Reset state to allow another PES to begin in the same TS payload.
      pes->collecting = 0;
      pes->total_length = PES_LEN_UNBOUNDED;  // Default to unbounded
      segment[pid].length = 0;
    }

    // For unbounded PES, we just continue collecting until a new PUSI == 1 appears.
    // Handled automatically by the outer loop.
  }

  return (EXIT_SUCCESS);
}
