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

// Allocate a zero-filled block after first checking that count * size cannot
// overflow size_t. The small public allocation functions below all use this
// helper so allocation failures are handled consistently.
static void *
allocate_zeroed (size_t count, size_t size, const char *name) {

  void *p;

  if (!count || !size || count > SIZE_MAX / size) {
    fprintf (stderr, "Invalid allocation size for %s.\n", name);
    exit (EXIT_FAILURE);
  }

  p = calloc (count, size);
  if (!p) {
    fprintf (stderr, "Cannot allocate memory for %s.\n", name);
    exit (EXIT_FAILURE);
  }

  return (p);
}

// Allocate memory for an array of uint8_t.
uint8_t *
allocate_u8mem (size_t n) {

  return (allocate_zeroed (n, sizeof (uint8_t), "uint8_t array"));
}

// Allocate memory for an array of chars.
char *
allocate_strmem (size_t n) {

  return (allocate_zeroed (n, sizeof (char), "character string"));
}

// Allocate memory for an array of size_t.
size_t *
allocate_sizemem (size_t n) {

  return (allocate_zeroed (n, sizeof (size_t), "size_t array"));
}

// Allocate memory for an array of PROGRAM structs.
PROGRAM *
allocate_progmem (size_t n) {

  return (allocate_zeroed (n, sizeof (PROGRAM), "PROGRAM array"));
}

// Allocate memory for an array of SECTION structs.
SECTION *
allocate_sectionmem (size_t n) {

  return (allocate_zeroed (n, sizeof (SECTION), "SECTION array"));
}

// Allocate memory for an array of SEGMENT structs.
SEGMENT *
allocate_segmentmem (size_t n) {

  return (allocate_zeroed (n, sizeof (SEGMENT), "SEGMENT array"));
}
