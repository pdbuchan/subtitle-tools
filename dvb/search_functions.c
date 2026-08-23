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

// Search the pages to retrieve the compact array index associated with a
// page_id. DVB identifiers are not assumed to equal their array indexes.
// Return -1 if the page_id has not yet been seen.
int
find_page_index (STATE *state, PAGE *page, uint16_t id) {

  size_t i;

  if (page) for (i = 0; i < state->npages; i++) if (page[i].page_id == id) {
    if (i > (size_t) INT_MAX) exit (EXIT_FAILURE);
    return ((int) i);
  }

  return (-1);
}

// Search the regions of one page to retrieve the array index associated with
// a region_id. Return -1 if the region has not yet been defined.
int
find_region_index (PAGE *page, size_t p, uint8_t id) {

  size_t i;

  if (page[p].region) for (i = 0; i < page[p].nregions; i++) if (page[p].region[i].region_id == id) {
    if (i > (size_t) INT_MAX) exit (EXIT_FAILURE);
    return ((int) i);
  }

  return (-1);
}

// Search the objects of one page to retrieve the array index associated with
// an object_id. Return -1 if the object has not yet been defined.
int
find_object_index (PAGE *page, size_t p, uint16_t id) {

  size_t i;

  if (page[p].object) for (i = 0; i < page[p].nobjects; i++) if (page[p].object[i].object_id == id) {
    if (i > (size_t) INT_MAX) exit (EXIT_FAILURE);
    return ((int) i);
  }

  return (-1);
}

// Search the CLUT families of one page to retrieve the array index associated
// with a clut_id. Return -1 if that CLUT family has not yet been created.
int
find_clut_index (PAGE *page, size_t p, uint8_t id) {

  size_t i;

  if (page[p].clut) for (i = 0; i < page[p].ncluts; i++) if (page[p].clut[i].clut_id == id) {
    if (i > (size_t) INT_MAX) exit (EXIT_FAILURE);
    return ((int) i);
  }

  return (-1);
}

// Find the PAT program whose PMT is carried on pid. Return -1 when the PID is
// not one of the PMT PIDs listed in the current PAT.
int
find_program_by_pmt_pid (PAT *pat, uint16_t pid) {

  size_t i;

  for (i = 0; i < pat->nprograms; i++) if (pat->program[i].pmt_pid == pid) {
    if (i > (size_t) INT_MAX) exit (EXIT_FAILURE);
    return ((int) i);
  }

  return (-1);
}
