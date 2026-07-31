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

// Calculate h, m, s, ms from totalms in TIME struct.
int
mstotime (TIME *time) {

  int64_t totalms;

  totalms = time->totalms;

  time->h = (int) (totalms / (1000 * 60 * 60));
  totalms %= (1000 * 60 * 60);
  
  time->m = (int) (totalms / (1000 * 60));
  totalms %= (1000 * 60);
  
  time->s = (int) (totalms / 1000);

  time->ms = (int) (totalms % 1000);

  return (EXIT_SUCCESS);
}
