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

#include "webvtt2srt.h"

// Append one input line to a WebVTT block.
//
// The line text is already dynamically allocated by readline(). Ownership is
// transferred to the block when this function succeeds. The block's LINE
// array is enlarged as required.
int
append_line (BLOCK *block, char *text, unsigned long number) {

  size_t new_capacity;
  LINE *tmp;

  // Both the destination block and the line to append must be valid.
  if ((block == NULL) || (text == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  // Grow the line array whenever all currently allocated slots are in use.
  if (block->count == block->capacity) {
    if (block->capacity == 0u) {
      new_capacity = INITIAL_BLOCK_SIZE;
    } else {
      new_capacity = block->capacity * 2u;
    }

    // Detect both arithmetic wraparound and multiplication overflow before
    // calculating the byte count passed to realloc().
    if ((new_capacity < block->capacity) || (new_capacity > (SIZE_MAX / sizeof (*block->line)))) {
      errno = ENOMEM;
      return (-1);
    }

    // Keep the original allocation unchanged if realloc() fails.
    tmp = realloc (block->line, new_capacity * sizeof (*block->line));
    if (tmp == NULL) {
      return (-1);
    }

    block->line = tmp;
    block->capacity = new_capacity;
  }

  // Transfer ownership of text to the new LINE entry and retain its source
  // line number for useful diagnostics if the block later proves invalid.
  block->line[block->count].text = text;
  block->line[block->count].number = number;
  block->count++;

  return (0);
}
