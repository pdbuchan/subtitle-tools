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

static void *
allocate_mem (size_t len, size_t item_size, const char *name) {

  void *tmp;

  if (len == 0 || item_size == 0 || len > (SIZE_MAX / item_size)) {
    fprintf (stderr, "Cannot allocate memory for %s: invalid size in allocate_mem().\n", name);
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, item_size);
  if (tmp == NULL) {
    fprintf (stderr, "Cannot allocate memory for %s in allocate_mem().\n", name);
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate memory for an array of uint8_t.
uint8_t *
allocate_u8mem (size_t len) {
  return (allocate_mem (len, sizeof (uint8_t), "array of uint8_t"));
}

// Allocate memory for an array of PROGRAM structs.
PROGRAM *
allocate_progmem (size_t len) {
  return (allocate_mem (len, sizeof (PROGRAM), "array of PROGRAM structs"));
}

// Allocate memory for an array of SECTION structs.
SECTION *
allocate_sectionmem (size_t len) {
  return (allocate_mem (len, sizeof (SECTION), "array of SECTION structs"));
}

// Allocate memory for an array of SEGMENT structs.
SEGMENT *
allocate_segmentmem (size_t len) {
  return (allocate_mem (len, sizeof (SEGMENT), "array of SEGMENT structs"));
}

// Allocate memory for an array PES structs.
PES *
allocate_pesmem (size_t len) {
  return (allocate_mem (len, sizeof (PES), "array of PES structs"));
}
