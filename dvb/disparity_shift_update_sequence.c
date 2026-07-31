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
// Reference: ETSI EN 300 743
int
disparity_shift_update_sequence (STATE *state, size_t *offset, size_t *consumed, SEGMENT *segment, FILE *fo) {

  size_t i, disparity_shift_update_sequence_length, division_period_count, interval_count;
  uint8_t disparity_shift_update_integer_part;
  uint16_t pid;
  uint32_t interval_duration;

  pid = state->pid;

  // Disparity Shift Update Sequence Length (1 byte)
  disparity_shift_update_sequence_length = (size_t) segment[pid].buffer[*offset];
  fprintf (fo, "    Disparity Shift Update Sequence Length (1 byte): %zu bytes\n", disparity_shift_update_sequence_length);
  (*offset)++;
  (*consumed)++;

  // Interval Duration (3 bytes)
  interval_duration =
    ((uint32_t) (segment[pid].buffer[*offset] << 16)) |
    ((uint32_t) (segment[pid].buffer[(*offset) + 1] << 8)) |
    ((uint32_t)  segment[pid].buffer[(*offset) + 2]);

  // Convert to ms via integer math.
  interval_duration = (interval_duration + 45) / 90;
  fprintf (fo, "    Interval Duration (3 bytes): %" PRIu32 " ms\n", interval_duration);
  (*offset) += 3;
  (*consumed) += 3;

  // Division Period Count (1 byte)
  division_period_count = segment[pid].buffer[*offset];
  fprintf (fo, "    Division Period Count (1 byte): %zu\n", division_period_count);
  (*offset)++;
  (*consumed)++;

  // Division Period Loop
  for (i = 0; i < division_period_count; i++) {

    // Interval Count (1 byte)
    interval_count = (size_t) segment[pid].buffer[*offset];
    (*offset)++;
    (*consumed)++;

    // Disaparity Shift Update Integer Part (1 byte, two's complement)
    disparity_shift_update_integer_part = segment[pid].buffer[*offset];
    (*offset)++;
    (*consumed)++;

    fprintf (fo, "    Interval Count (1 byte): %zu     Disaparity Shift Update Integer Part (1 byte): %d\n", interval_count, (int8_t) disparity_shift_update_integer_part);

  }  // End loop

  return (EXIT_SUCCESS);
}
