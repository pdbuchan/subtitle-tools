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

// Parse timestamp into TIME struct, and also return total time in milliseconds.
int
parse_timestamp (char *timestamp, TIME *time) {

  char xx[3], xxx[4], *endptr;

  // Hours
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, timestamp, 2);
  errno = 0;
  time->h = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of hours: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Minutes
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, &timestamp[3], 2);
  errno = 0;
  time->m = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of minutes: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Seconds
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, &timestamp[6], 2);
  errno = 0;
  time->s = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of seconds: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Milliseconds
  memset (xxx, 0, 4 * sizeof (char));
  strncpy (xxx, &timestamp[9], 3);
  errno = 0;
  time->ms = (int) strtol (xxx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xxx)) {
    fprintf (stderr, "ERROR: Cannot make integer of milliseconds: %s\n", xxx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Total milliseconds.
  timetoms (time);

  return (EXIT_SUCCESS);
}
