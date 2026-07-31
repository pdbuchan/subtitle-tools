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

// Allocate memory for an array of size_t.
size_t *
allocate_sizemem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_sizemem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (size_t *) malloc (len * sizeof (size_t));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (size_t));
    return (tmp);
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_sizemem().\n");
    exit (EXIT_FAILURE);
  }
}
  
// Allocate memory for an array of PROGRAM structs.
PROGRAM * 
allocate_progmem (int len) {
    
  void *tmp;
  
  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_progmem().\n", len);
    exit (EXIT_FAILURE);
  }
  
  tmp = (PROGRAM *) malloc (len * sizeof (PROGRAM));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (PROGRAM));
    return (tmp);
  } else { 
    fprintf (stderr, "Cannot allocate memory for array in allocate_progmem().\n");
    exit (EXIT_FAILURE);
  } 
}

// Allocate memory for an array of SECTIONs.
SECTION *
allocate_sectionmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_sectionmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (SECTION *) malloc (len * sizeof (SECTION));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (SECTION));
    return (tmp);
  } else { 
    fprintf (stderr, "Cannot allocate memory for array in allocate_sectionmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of SEGMENTs.
SEGMENT *
allocate_segmentmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "Cannot allocate memory because len = %d in allocate_segmentmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (SEGMENT *) malloc (len * sizeof (SEGMENT));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (SEGMENT));
    return (tmp);
  } else {
    fprintf (stderr, "Cannot allocate memory for array in allocate_segmentmem().\n");
    exit (EXIT_FAILURE);
  }
}
