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

// Add a TS packet payload to the current PID's PES reassembly buffer.
// Each PID has independent PES state because a transport stream may contain
// more than one interleaved Teletext elementary stream.
int
build_pes_segment (STATE *state, TTX_CONTEXT *ttx, uint8_t *tsdata, size_t tslen, size_t ts_payloadlen, SEGMENT *segment, PES *pes, PAT *pat, FILE *fo) {

  size_t payload_end, total;
  uint16_t pid;
  PES *p;

  pid = state->pid;
  p = &pes[pid];

  if (!bytes_available (state->ts_index, ts_payloadlen, tslen)) {
    fprintf (stderr, "TS payload exceeds input in build_pes_segment().\n");
    return (EXIT_FAILURE);
  }
  payload_end = state->ts_index + ts_payloadlen;

  if (!segment[pid].buffer) {
    segment[pid].buffer = allocate_u8mem (MAX_BUFFERLEN);
  }

  // PUSI starts a new PES packet. A previously unbounded PES on this same PID
  // terminates at the new start and is parsed before the buffer is reused.
  if (state->pusi) {
    if (p->collecting && segment[pid].length > 0) {
      // A new PUSI legitimately terminates an unbounded PES packet. If the
      // previous PES declared a finite length but never reached it, however,
      // the stream is discontinuous/truncated; do not pass a partial packet
      // to the PES parser and risk interpreting the next packet as its tail.
      if (p->total_length == PES_LEN_UNBOUNDED) {
        if (parse_pes_segment (state, ttx, pat, segment, p, fo) != EXIT_SUCCESS) {
          return (EXIT_FAILURE);
        }
      } else if (segment[pid].length != (size_t) p->total_length) {
        fprintf (fo, "\n  WARNING: Discarding incomplete PES on PID 0x%04x (%zu of %zd bytes received).\n", pid, segment[pid].length, p->total_length);
      }
    }
    p->collecting = 1;
    p->total_length = PES_LEN_UNBOUNDED;
    p->packet_length = 0;
    segment[pid].length = 0;
    state->pusi = 0;
  }

  if (!p->collecting) {
    state->ts_index = payload_end;
    return (EXIT_SUCCESS);
  }

  while (state->ts_index < payload_end) {
    if (segment[pid].length >= MAX_BUFFERLEN) {
      fprintf (stderr, "PES packet exceeds %d-byte reassembly buffer.\n", MAX_BUFFERLEN);
      return (EXIT_FAILURE);
    }

    segment[pid].buffer[segment[pid].length++] = tsdata[state->ts_index++];

    if (p->total_length == PES_LEN_UNBOUNDED && segment[pid].length >= 6) {
      p->packet_length = (size_t) (((uint16_t) segment[pid].buffer[4] << 8) | segment[pid].buffer[5]);
      if (p->packet_length != 0) {
        total = 6 + p->packet_length;
        if (total > MAX_BUFFERLEN) {
          fprintf (stderr, "Declared PES packet length is too large.\n");
          return (EXIT_FAILURE);
        }
        p->total_length = (ssize_t) total;
      }
    }

    if (p->total_length != PES_LEN_UNBOUNDED &&
        segment[pid].length == (size_t) p->total_length) {
      if (parse_pes_segment (state, ttx, pat, segment, p, fo) != EXIT_SUCCESS) {
        return (EXIT_FAILURE);
      }
      p->collecting = 0;
      p->total_length = PES_LEN_UNBOUNDED;
      segment[pid].length = 0;
      state->ts_index = payload_end;
      break;
    }
  }

  return (EXIT_SUCCESS);
}
