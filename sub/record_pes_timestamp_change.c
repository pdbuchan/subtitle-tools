/*  Copyright (C) 2025-2026 P. David Buchan (pdbuchan@gmail.com)

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

#include "sub.h"

static void
ensure_change_capacity (OPTIONS *options, size_t additional) {

  size_t needed, new_capacity;
  CHANGE *tmp;

  if (additional > SIZE_MAX - options->nchanges) {
    fprintf (stderr, "Timestamp change list size overflow.\n");
    exit (EXIT_FAILURE);
  }

  needed = options->nchanges + additional;
  if (needed <= options->change_capacity) return;

  new_capacity = options->change_capacity == 0 ? 1024 : options->change_capacity;
  while (new_capacity < needed) {
    if (new_capacity > SIZE_MAX / 2) {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2;
  }

  if (new_capacity > SIZE_MAX / sizeof (CHANGE)) {
    fprintf (stderr, "Timestamp change list is too large.\n");
    exit (EXIT_FAILURE);
  }

  tmp = realloc (options->change, new_capacity * sizeof (CHANGE));
  if (tmp == NULL) {
    fprintf (stderr, "Cannot grow timestamp change list.\n");
    exit (EXIT_FAILURE);
  }

  options->change = tmp;
  options->change_capacity = new_capacity;
}

void
record_pes_timestamp_change (OPTIONS *options, size_t pos, uint64_t ts90, uint8_t prefix) {

  ensure_change_capacity (options, 5);
  ts90 &= PES_TS_MASK;

  options->change[options->nchanges++] = (CHANGE){pos,     (uint8_t) (prefix | ((ts90 >> 29) & 0x0e) | 0x01)};
  options->change[options->nchanges++] = (CHANGE){pos + 1, (uint8_t) ((ts90 >> 22) & 0xff)};
  options->change[options->nchanges++] = (CHANGE){pos + 2, (uint8_t) (((ts90 >> 14) & 0xfe) | 0x01)};
  options->change[options->nchanges++] = (CHANGE){pos + 3, (uint8_t) ((ts90 >> 7) & 0xff)};
  options->change[options->nchanges++] = (CHANGE){pos + 4, (uint8_t) (((ts90 & 0x7f) << 1) | 0x01)};
}

static int
compare_change (const void *a, const void *b) {

  const CHANGE *ca = a;
  const CHANGE *cb = b;

  if (ca->offset < cb->offset) return -1;
  if (ca->offset > cb->offset) return 1;
  return 0;
}

void
sort_and_compact_changes (OPTIONS *options) {

  size_t read_index, write_index;

  if (options->nchanges < 2) return;

  qsort (options->change, options->nchanges, sizeof (CHANGE), compare_change);

  write_index = 1;
  for (read_index = 1; read_index < options->nchanges; read_index++) {
    if (options->change[read_index].offset == options->change[write_index - 1].offset) {
      if (options->change[read_index].new_value != options->change[write_index - 1].new_value) {
        fprintf (stderr, "Conflicting timestamp rewrites at file offset 0x%zx.\n",
                 options->change[read_index].offset);
        exit (EXIT_FAILURE);
      }
      continue;
    }
    options->change[write_index++] = options->change[read_index];
  }

  options->nchanges = write_index;
}
