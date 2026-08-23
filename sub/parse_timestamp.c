/*  Copyright (C) 2025-2026 P. David Buchan (pdbuchan@gmail.com)

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

#include "sub.h"

int
parse_timestamp (const char *timestamp, TIME *time) {

  size_t i;
  int h, m, s, ms;

  if (timestamp == NULL || strlen (timestamp) != 12 ||
      timestamp[2] != ':' || timestamp[5] != ':' || timestamp[8] != ',') {
    fprintf (stderr, "Invalid timestamp format: %s (expected hh:mm:ss,mmm)\n",
             timestamp == NULL ? "(null)" : timestamp);
    return (EXIT_FAILURE);
  }

  for (i = 0; i < 12; i++) {
    if (i == 2 || i == 5 || i == 8) continue;
    if (!isdigit ((unsigned char) timestamp[i])) {
      fprintf (stderr, "Invalid timestamp format: %s (expected hh:mm:ss,mmm)\n", timestamp);
      return (EXIT_FAILURE);
    }
  }

  h = (timestamp[0] - '0') * 10 + (timestamp[1] - '0');
  m = (timestamp[3] - '0') * 10 + (timestamp[4] - '0');
  s = (timestamp[6] - '0') * 10 + (timestamp[7] - '0');
  ms = (timestamp[9] - '0') * 100 + (timestamp[10] - '0') * 10 + (timestamp[11] - '0');

  if (m > 59 || s > 59 || ms > 999) {
    fprintf (stderr, "Timestamp component out of range: %s\n", timestamp);
    return (EXIT_FAILURE);
  }

  time->h = h;
  time->m = m;
  time->s = s;
  time->ms = ms;
  timetoms (time);

  return (EXIT_SUCCESS);
}
