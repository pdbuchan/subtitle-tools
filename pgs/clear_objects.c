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

// Clear objects.
int
clear_objects (OBJECT *object) {

  size_t i;

  for (i = 0; i < (size_t) MAX_OBJECTS; i++) {

    if (object[i].buffer != NULL) {
      free(object[i].buffer);
      object[i].buffer = NULL;
    }

    object[i].length = 0;
  }

  return (EXIT_SUCCESS);
}
