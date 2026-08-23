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

// Reset the reassembly state for one PSI PID while retaining its allocated
// buffer for reuse by the next section.
static void
reset_section (SECTION *s) {

  s->length = 0;
  s->bytecount = 0;
}

// Add a TS packet payload to the current PID's PSI section reassembly buffer.
// A PSI section may span several TS packets, and one TS payload may contain
// the end of one section followed by one or more complete new sections.
int
build_psi_section (STATE *state, PAT *pat, uint8_t *tsdata, size_t tslen, size_t ts_payloadlen, SECTION *section, FILE *fo) {

  size_t end, pointer, n, need;
  uint16_t pid = state->pid;
  SECTION *s = &section[pid];

  // Compute the end of the TS payload only after validating its declared
  // length against the input buffer.
  if (!bytes_available (state->ts_index, ts_payloadlen, tslen)) {
    fprintf (stderr, "TS payload exceeds input in build_psi_section().\n");
    return (EXIT_FAILURE);
  }
  end = state->ts_index + ts_payloadlen;

  // Allocate one persistent PSI reassembly buffer per PID on first use.
  if (!s->buffer) {
    s->buffer = allocate_u8mem (MAX_BUFFERLEN);
  }

  // With PUSI set, pointer_field gives the number of bytes before the first
  // new section. Those bytes may complete a section begun in an earlier TS
  // packet.
  if (state->pusi) {
    if (state->ts_index >= end) {
      return (EXIT_FAILURE);
    }
    pointer = tsdata[state->ts_index++];
    if (!bytes_available (state->ts_index, pointer, end)) {
      fprintf (stderr, "PSI pointer_field exceeds TS payload.\n");
      return (EXIT_FAILURE);
    }

    // pointer_field is the number of bytes before the first new PSI section.
    // Those bytes may complete a section begun in the previous TS packet.
    while (pointer > 0 && s->bytecount > 0) {
      if (s->length == 0) {
        need = 3 - s->bytecount;
      } else {
        need = s->length - s->bytecount;
      }
      n = pointer < need ? pointer : need;
      memcpy (s->buffer + s->bytecount, tsdata + state->ts_index, n);
      s->bytecount += n;
      state->ts_index += n;
      pointer -= n;

      // Once the three-byte PSI prefix is available, section_length tells us
      // the exact total number of bytes to collect.
      if (s->length == 0 && s->bytecount == 3) {
        s->length = 3 + (size_t) (((s->buffer[1] & 0x0f) << 8) | s->buffer[2]);
        if (s->length > MAX_BUFFERLEN) {
          fprintf (stderr, "PSI section exceeds reassembly buffer.\n");
          return (EXIT_FAILURE);
        }
      }

      // If the pointer bytes completed the previous section, parse it before
      // moving to the first new section announced by this PUSI.
      if (s->length > 0 && s->bytecount == s->length) {
        if (parse_psi_section (state, pat, section, fo) != EXIT_SUCCESS) {
          return (EXIT_FAILURE);
        }
        reset_section (s);
        break;
      }
    }

    // Any remaining pointer bytes are stuffing. If a previous section still
    // is incomplete at the announced new-section boundary, discard it rather
    // than splicing two sections together.
    state->ts_index += pointer;
    if (s->bytecount != 0) {
      fprintf (stderr, "Incomplete PSI section at a new-section boundary.\n");
      reset_section (s);
    }
  } else if (s->bytecount == 0) {
    // A continuation packet without an existing section cannot be decoded.
    state->ts_index = end;
    return (EXIT_SUCCESS);
  }

  // Parse zero or more PSI sections beginning in the remaining TS payload.
  while (state->ts_index < end) {

    // 0xff after a completed section is PSI stuffing; no further sections are
    // present in this TS payload.
    if (s->bytecount == 0 && tsdata[state->ts_index] == 0xff) {
      state->ts_index = end;
      break;
    }

    // Reassemble the three-byte PSI prefix first so section_length becomes
    // known before copying the rest of the section.
    if (s->length == 0) {
      need = 3 - s->bytecount;
      n = end - state->ts_index;
      if (n > need) n = need;
      memcpy (s->buffer + s->bytecount, tsdata + state->ts_index, n);
      s->bytecount += n;
      state->ts_index += n;
      if (s->bytecount < 3) break;

      s->length = 3 + (size_t) (((s->buffer[1] & 0x0f) << 8) | s->buffer[2]);
      if (s->length > MAX_BUFFERLEN || s->length < 3) {
        fprintf (stderr, "Invalid PSI section length.\n");
        return (EXIT_FAILURE);
      }
    }

    // Copy as much of the known-length section as this payload contains.
    need = s->length - s->bytecount;
    n = end - state->ts_index;
    if (n > need) n = need;
    memcpy (s->buffer + s->bytecount, tsdata + state->ts_index, n);
    s->bytecount += n;
    state->ts_index += n;

    // A complete section is parsed immediately. The loop then permits another
    // section to begin later in the same TS payload.
    if (s->bytecount == s->length) {
      if (parse_psi_section (state, pat, section, fo) != EXIT_SUCCESS) {
        return (EXIT_FAILURE);
      }
      reset_section (s);
    }
  }

  // Preserve the current reassembly counters for diagnostic/state reporting.
  state->previous_section_length[pid] = s->length;
  state->previous_section_bytecount[pid] = s->bytecount;
  return (EXIT_SUCCESS);
}
