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

static void *
allocate_zeroed (size_t len, size_t element_size, const char *where) {

  void *tmp;

  if (len == 0 || element_size == 0 || len > SIZE_MAX / element_size) {
    fprintf (stderr, "Invalid allocation size in %s().\n", where);
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, element_size);
  if (tmp == NULL) {
    fprintf (stderr, "Cannot allocate memory in %s().\n", where);
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

uint8_t *
allocate_u8mem (size_t len) {
  return allocate_zeroed (len, sizeof (uint8_t), "allocate_u8mem");
}

char *
allocate_strmem (size_t len) {
  return allocate_zeroed (len, sizeof (char), "allocate_strmem");
}

char **
allocate_strmemp (size_t len) {
  return allocate_zeroed (len, sizeof (char *), "allocate_strmemp");
}

size_t *
allocate_sizetmem (size_t len) {
  return allocate_zeroed (len, sizeof (size_t), "allocate_sizetmem");
}

size_t **
allocate_sizetmemp (size_t len) {
  return allocate_zeroed (len, sizeof (size_t *), "allocate_sizetmemp");
}
