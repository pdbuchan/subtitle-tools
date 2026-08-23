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

// Read up to 8 bits from an ODS field at an arbitrary bit position.
//
// limit is the end of the current top- or bottom-field data block in bits,
// rather than merely the end of the allocated PES buffer. This prevents a
// malformed RLE code string from consuming bytes belonging to the next DVB
// structure.
int
get_bits (STATE *state, SEGMENT *segment, size_t *pos, size_t limit, unsigned int n, uint8_t *out) {

  size_t total, p;
  unsigned int i;
  uint8_t v = 0;
  uint16_t pid = state->pid;

  if (n == 0 || n > 8 || segment[pid].length > SIZE_MAX / 8) return (EXIT_FAILURE);
  total = segment[pid].length * 8;
  if (limit > total || *pos > limit || (size_t) n > limit - *pos) {
    fprintf (stderr, "RLE data crosses its field boundary.\n");
    return (EXIT_FAILURE);
  }

  // Read one bit at a time so the caller may begin at any bit offset.
  for (i = 0; i < n; i++) {
    p = *pos + (size_t) i;
    v = (uint8_t) ((v << 1) | ((segment[pid].buffer[p / 8] >> (7 - (p % 8))) & 1U));
  }
  *pos += (size_t) n;
  *out = v;

  return (EXIT_SUCCESS);
}
