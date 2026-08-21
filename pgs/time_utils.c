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

// Calculate totalms from h, m, s, ms in TIME struct.
int
timetoms (TIME *time) {

  time->totalms = (int64_t) time->h * 60 * 60 * 1000;
  time->totalms += (int64_t) time->m * 60 * 1000;
  time->totalms += (int64_t) time->s * 1000;
  time->totalms += (int64_t) time->ms;

  return (EXIT_SUCCESS);
}

// Calculate h, m, s, ms from a non-negative totalms value.
int
mstotime (TIME *time) {

  int64_t totalms = time->totalms;

  if (totalms < 0) totalms = 0;

  time->h = (int) (totalms / (INT64_C (1000) * 60 * 60));
  totalms %= (INT64_C (1000) * 60 * 60);
  time->m = (int) (totalms / (INT64_C (1000) * 60));
  totalms %= (INT64_C (1000) * 60);
  time->s = (int) (totalms / 1000);
  time->ms = (int) (totalms % 1000);

  return (EXIT_SUCCESS);
}
