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

// Search the npages to retrieve the page_id's index.
// Return -1 if not found.
int
find_page_index (STATE *state, PAGE *page, uint16_t page_id) {

  int index;

  if (page != NULL) {
    for (index = 0; index < state->npages; index++) {
      if (page[index].page_id == page_id) {
        return (index);
      }
    }
  }
  return (-1);  // Failed to find page_id.
}

// Search the nregions of page[page_id] to retrieve region_id's index. 
// Return -1 if not found.
int 
find_region_index (STATE *state, PAGE *page, uint8_t region_id) {

  size_t page_idx;
  int temp, index;

  // Find page index for page_id.
  temp = find_page_index (state, page, state->page_id);
  if (temp < 0) { 
    fprintf (stderr, "Cannot find index for page_id: 0x%04x in find_region_index().\n", state->page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  if (page[page_idx].region != NULL) {
    for (index = 0; index < page[page_idx].nregions; index++) {
      if (page[page_idx].region[index].region_id == region_id) {
        return (index);
      }
    }   
  }
  return (-1);  // Failed to find region_id.
}

// Search the nobjects of page[page_id] to retrieve object_id's index.
// Return -1 if not found.
int
find_object_index (STATE *state, PAGE *page, uint16_t object_id) {

  size_t page_idx;
  int temp, index;

  temp = find_page_index (state, page, state->page_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find state->page_id: 0x%04x in find_object_index().\n", state->page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  if (page[page_idx].object != NULL) {
    for (index = 0; index < page[page_idx].nobjects; index++) {
      if (page[page_idx].object[index].object_id == object_id) {
        return (index);
      }
    }
  }
  return (-1);  // Failed to find object_id.
}

// Search the ncluts of page[page_idx] to retrieve clut_id's index.
int
find_clut_index (STATE *state, PAGE *page, uint8_t clut_id) {

  size_t page_idx;
  int temp, index;

  temp = find_page_index (state, page, state->page_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find state->page_id: 0x%04x in find_clut_index().\n", state->page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  if (page[page_idx].clut != NULL) {
    for (index = 0; index < page[page_idx].ncluts; index++) {
      if (page[page_idx].clut[index].clut_id == clut_id) {
        return (index);
      }
    }
  }
  return (-1);  // Failed to find clut_id.
}

// Find a Program index associated with a given PID, as defined by PMT.
// Return -1 if not found.
int
find_program_by_pmt_pid (PAT *pat, uint16_t pid) {

  int index;
  
  for (index = 0; index < pat->nprograms; index++) {
    if (pat->program[index].pmt_pid == pid) {
      return (index);
    }
  } 
  
  return (-1);  // Not found
}
