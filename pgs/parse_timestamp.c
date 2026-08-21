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
#include <ctype.h>

// Parse a fixed-format hh:mm:ss,mmm timestamp into TIME.
int
parse_timestamp (char *timestamp, TIME *time) {

  size_t i;

  if (strlen (timestamp) != 12 || timestamp[2] != ':' || timestamp[5] != ':' || timestamp[8] != ',') {
    fprintf (stderr, "Invalid timestamp '%s'. Expected hh:mm:ss,mmm.\n", timestamp);
    exit (EXIT_FAILURE);
  }

  for (i = 0; i < 12; i++) {
    if (i == 2 || i == 5 || i == 8) continue;
    if (!isdigit ((unsigned char) timestamp[i])) {
      fprintf (stderr, "Invalid timestamp '%s'. Expected decimal digits in hh:mm:ss,mmm.\n", timestamp);
      exit (EXIT_FAILURE);
    }
  }

  time->h = (timestamp[0] - '0') * 10 + (timestamp[1] - '0');
  time->m = (timestamp[3] - '0') * 10 + (timestamp[4] - '0');
  time->s = (timestamp[6] - '0') * 10 + (timestamp[7] - '0');
  time->ms = (timestamp[9] - '0') * 100 + (timestamp[10] - '0') * 10 + (timestamp[11] - '0');

  if (time->m > 59 || time->s > 59) {
    fprintf (stderr, "Invalid timestamp '%s': minutes and seconds must be between 00 and 59.\n", timestamp);
    exit (EXIT_FAILURE);
  }

  timetoms (time);
  return (EXIT_SUCCESS);
}
