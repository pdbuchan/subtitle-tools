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

// Release all storage owned by a WebVTT block and reset it for reuse.
void
free_block (BLOCK *block) {

  size_t i;

  // A NULL block has no resources to release.
  if (block == NULL) {
    return;
  }

  // Each line string was allocated independently by readline(), so free all
  // strings before releasing the array that contains their LINE records.
  for (i = 0u; i < block->count; i++) {
    free (block->line[i].text);
  }

  free (block->line);

  // Restore the empty-block state so the same BLOCK can safely be reused.
  block->line = NULL;
  block->count = 0u;
  block->capacity = 0u;
}
