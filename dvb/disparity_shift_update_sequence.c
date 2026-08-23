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

// Disparity Shift Update Sequence
// "limit" is the end of the containing DSS and prevents a malformed sequence
// length from consuming bytes belonging to the next DVB structure.
// Reference: ETSI EN 300 743
int
disparity_shift_update_sequence (STATE *state, size_t *offset, size_t *consumed, size_t limit, SEGMENT *segment, FILE *fo) {

  size_t i, seq_len, seq_end, division_period_count, interval_count;
  uint8_t disparity_shift_update_integer_part;
  uint16_t pid = state->pid;
  uint32_t interval_duration;

  // Disparity Shift Update Sequence Length (1 byte)
  if (!bytes_available (*offset, 1, limit)) {
    return (EXIT_FAILURE);
  }
  seq_len = segment[pid].buffer[(*offset)++];
  (*consumed)++;
  fprintf (fo, "    Disparity Shift Update Sequence Length: %zu bytes\n", seq_len);

  if (!bytes_available (*offset, seq_len, limit) || seq_len < 4) {
    fprintf (stderr, "Invalid disparity shift update sequence length.\n");
    return (EXIT_FAILURE);
  }
  seq_end = *offset + seq_len;

  // Interval Duration (3 bytes), expressed in 90 kHz clock ticks.
  interval_duration = ((uint32_t) segment[pid].buffer[*offset] << 16) |
                      ((uint32_t) segment[pid].buffer[*offset + 1] << 8) |
                      segment[pid].buffer[*offset + 2];
  // Convert 90 kHz ticks to milliseconds using rounded integer arithmetic.
  interval_duration = (interval_duration + 45) / 90;
  *offset += 3;
  *consumed += 3;
  fprintf (fo, "    Interval Duration: %" PRIu32 " ms\n", interval_duration);

  // Division Period Count (1 byte)
  division_period_count = segment[pid].buffer[(*offset)++];
  (*consumed)++;
  fprintf (fo, "    Division Period Count: %zu\n", division_period_count);

  // Each loop entry occupies at least two bytes: interval_count and the
  // signed disparity-shift integer part. Check the count before entering it.
  if (division_period_count > (seq_end - *offset) / 2) {
    fprintf (stderr, "Disparity division-period loop exceeds sequence length.\n");
    return (EXIT_FAILURE);
  }

  // Division Period Loop
  for (i = 0; i < division_period_count; i++) {

    // Interval Count (1 byte)
    interval_count = segment[pid].buffer[(*offset)++];
    // Disparity Shift Update Integer Part (1 byte, two's complement)
    disparity_shift_update_integer_part = segment[pid].buffer[(*offset)++];
    *consumed += 2;
    fprintf (fo, "    Interval Count: %zu  Disparity Shift Update Integer Part: %d\n", interval_count, (int8_t) disparity_shift_update_integer_part);
  }

  // Any bytes permitted by a future extension stay within the declared
  // sequence rather than being mistaken for the next DSS structure.
  if (*offset < seq_end) {
    *consumed += seq_end - *offset;
    *offset = seq_end;
  }

  return (EXIT_SUCCESS);
}
