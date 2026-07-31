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

// Add packet payload to appropriate PSI section buffer, differentiated by PID.
// If current stream section is complete, call section parser.
int
build_psi_section (STATE *state, PAT *pat, uint8_t *tsdata, size_t tslen, int ts_payloadlen, SECTION *section, FILE *fo) {

  size_t pointer, total_len, payload_end, consumed, section_length;
  uint16_t pid;

  pid = state->pid;

  // Allocate memory for PSI section buffer, if not yet allocated.
  if (!section[pid].buffer) {  // Not allocated yet.
    section[pid].buffer = allocate_u8mem (MAX_BUFFERLEN);
  }

  payload_end = state->ts_index + ts_payloadlen;
  consumed = 0;

  // New section start; the last part of previous section may exist.
  // Expect: pointer_field, table_id, section_length
  if (state->pusi) {

    if (state->ts_index >= payload_end) return (EXIT_SUCCESS);

    // Pointer Field (1 byte)
    pointer = (size_t) tsdata[state->ts_index];
    state->ts_index++;
    consumed++;

    // Pointer bytes belong to previous section ONLY if one exists.
    while ((pointer > 0) && (state->previous_section_bytecount[pid] > 0) && (state->previous_section_bytecount[pid] < state->previous_section_length[pid]) && (state->ts_index < payload_end)) {
      section[pid].buffer[state->previous_section_bytecount[pid]] = tsdata[state->ts_index];
      state->previous_section_bytecount[pid]++;
      state->ts_index++;
      consumed++;
      pointer--;
    }

    // If previous section just completed, parse it, and clear buffer for next section.
    // Note that previous_section_length would == 0 if previous section was completed and sent to parse_psi_section().
    if ((state->previous_section_length[pid] > 0) && (state->previous_section_bytecount[pid] == state->previous_section_length[pid])) {

      parse_psi_section (state, pat, section, fo);

      section[pid].length = 0;
      section[pid].bytecount = 0;
      memset (section[pid].buffer, 0, MAX_BUFFERLEN * sizeof (uint8_t));
    }

    // Skip any remaining pointer stuffing.
    while ((pointer > 0) && (state->ts_index < payload_end)) {
      state->ts_index++;
      consumed++;
      pointer--;
    }
  }  // End if state->pusi

  // Parse sections in payload.
  while ((state->ts_index + 3) <= payload_end) {

    // Start of a new section.
    if (section[pid].bytecount == 0) {

      // Stuffing bytes; no more sections in this TS payload.
      if (tsdata[state->ts_index] == 0xff) {
        break;
      }

      section_length = (size_t) (((tsdata[state->ts_index + 1] & 0x0f) << 8) | tsdata[state->ts_index + 2]);
      total_len = 3 + section_length;
      section[pid].length = total_len;
    }

    // Copy bytes until section complete or payload ends.
    while ((state->ts_index < payload_end) && (section[pid].bytecount < section[pid].length)) {

      section[pid].buffer[section[pid].bytecount] = tsdata[state->ts_index];
      section[pid].bytecount++;
      state->ts_index++;
      consumed++;
    }

    // Section complete.
    if (section[pid].bytecount == section[pid].length) {

      parse_psi_section (state, pat, section, fo);

      section[pid].length = 0;
      section[pid].bytecount = 0;
      memset (section[pid].buffer, 0, MAX_BUFFERLEN * sizeof (uint8_t));

    // Loop continues: may be another section.
    } else {
      break;  // Need next TS packet.
    }
  }

  // Skip remaining payload.
  while (consumed < ts_payloadlen) {
    state->ts_index++;
    consumed++;
  }

  // Keep record of last bytecount and len.
  state->previous_section_length[pid] = section[pid].length;
  state->previous_section_bytecount[pid] = section[pid].bytecount;

  return (EXIT_SUCCESS);
}
