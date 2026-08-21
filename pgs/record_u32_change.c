/*  Copyright (C) 2024-2026 P. David Buchan (pdbuchan@gmail.com)

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

#include "pgs.h"

// Record a big-endian uint32_t change in the change array.
void
record_u32_change (OPTIONS *options, size_t index, uint32_t value) {

  if (options->nchanges > MAX_CHANGES - 4u) {
    fprintf (stderr, "Exceeded MAX_CHANGES while recording timestamp revisions.\n");
    exit (EXIT_FAILURE);
  }

  options->change[options->nchanges++] = (CHANGE) {index,     (uint8_t) ((value >> 24) & 0xffu)};
  options->change[options->nchanges++] = (CHANGE) {index + 1, (uint8_t) ((value >> 16) & 0xffu)};
  options->change[options->nchanges++] = (CHANGE) {index + 2, (uint8_t) ((value >> 8)  & 0xffu)};
  options->change[options->nchanges++] = (CHANGE) {index + 3, (uint8_t) (value & 0xffu)};
}
