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

// Allocate memory for an array of chars.
char *
allocate_strmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_strmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (char *) malloc (len * sizeof (char));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (char));
    return (tmp);
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of uint8_t.
uint8_t *
allocate_u8mem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_u8mem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (uint8_t *) malloc (len * sizeof (uint8_t));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (uint8_t));
    return (tmp);
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_u8mem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of PALETTE structs.
PALETTE *
allocate_pdsmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_pdsmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (PALETTE *) malloc (len * sizeof (PALETTE));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (PALETTE));
    return (tmp);
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_pdsmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of PALETTE_ENTRY structs.
PALETTE_ENTRY *
allocate_palentrymem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_palentrymem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (PALETTE_ENTRY *) malloc (len * sizeof (PALETTE_ENTRY));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (PALETTE_ENTRY));
    return (tmp);
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_palentrymem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of SYNC structs.
SYNC *
allocate_syncmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_syncmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (SYNC *) malloc (len * sizeof (SYNC));
  if (tmp != NULL) { 
    memset (tmp, 0, len * sizeof (SYNC));
    return (tmp);
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_syncmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of OBJECT structs.
OBJECT *
allocate_objmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_objmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (OBJECT *) malloc (len * sizeof (OBJECT));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (OBJECT));
    return (tmp); 
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_objmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of CHANGE structs.
CHANGE *
allocate_changemem (int len) {

  void *tmp; 

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_changemem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (CHANGE *) malloc (len * sizeof (CHANGE));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (CHANGE));
    return (tmp);
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_chagemem().\n");
    exit (EXIT_FAILURE);
  }
}
