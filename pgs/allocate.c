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

static void *
allocate_zeroed (size_t len, size_t item_size, const char *name) {

  void *tmp;

  if (len == 0 || item_size == 0 || len > SIZE_MAX / item_size) {
    fprintf (stderr, "Cannot allocate memory for %s: invalid size.\n", name);
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, item_size);
  if (tmp == NULL) {
    fprintf (stderr, "Cannot allocate memory for %s.\n", name);
    exit (EXIT_FAILURE);
  }

  return tmp;
}

char *
allocate_strmem (size_t len) {
  return allocate_zeroed (len, sizeof (char), "character array");
}

PALETTE *
allocate_pdsmem (size_t len) {
  return allocate_zeroed (len, sizeof (PALETTE), "PALETTE array");
}

PALETTE_ENTRY *
allocate_palentrymem (size_t len) {
  return allocate_zeroed (len, sizeof (PALETTE_ENTRY), "PALETTE_ENTRY array");
}

SYNC *
allocate_syncmem (size_t len) {
  return allocate_zeroed (len, sizeof (SYNC), "SYNC array");
}

OBJECT *
allocate_objmem (size_t len) {
  return allocate_zeroed (len, sizeof (OBJECT), "OBJECT array");
}

CHANGE *
allocate_changemem (size_t len) {
  return allocate_zeroed (len, sizeof (CHANGE), "CHANGE array");
}
