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
timetoms (TIME *time) {

  int64_t total;

  total = (int64_t) time->h * 60 * 60 * 1000;
  total += (int64_t) time->m * 60 * 1000;
  total += (int64_t) time->s * 1000;
  total += (int64_t) time->ms;
  time->totalms = total;

  return (EXIT_SUCCESS);
}

int
mstotime (TIME *time) {

  int64_t totalms;

  if (time->totalms < 0) {
    fprintf (stderr, "Cannot convert a negative absolute timestamp in mstotime().\n");
    return (EXIT_FAILURE);
  }

  totalms = time->totalms;
  time->h = (int) (totalms / INT64_C(3600000));
  totalms %= INT64_C(3600000);
  time->m = (int) (totalms / INT64_C(60000));
  totalms %= INT64_C(60000);
  time->s = (int) (totalms / INT64_C(1000));
  time->ms = (int) (totalms % INT64_C(1000));

  return (EXIT_SUCCESS);
}

int
transform_timestamp90 (const OPTIONS *options, uint64_t original90, uint64_t *new90) {

  long double transformed;
  long double ratio;

  if (original90 > PES_TS_MAX) {
    fprintf (stderr, "PES timestamp exceeds the 33-bit timestamp range.\n");
    return (EXIT_FAILURE);
  }

  transformed = (long double) original90;

  if (options->offset_flag) {
    transformed += (long double) options->offset.totalms * 90.0L;

  } else if (options->sync_flag) {
    if (options->oldlastms <= options->oldfirstms) {
      fprintf (stderr, "Invalid synchronization anchor interval.\n");
      return (EXIT_FAILURE);
    }

    ratio = ((long double) options->newlastms - (long double) options->newfirstms) /
            ((long double) options->oldlastms - (long double) options->oldfirstms);
    transformed = (long double) options->newfirstms * 90.0L +
                  ((long double) original90 - (long double) options->oldfirstms * 90.0L) * ratio;
  }

  if (transformed < 0.0L) {
    transformed = 0.0L;
  }

  if (transformed > (long double) PES_TS_MAX) {
    fprintf (stderr, "Adjusted timestamp exceeds the 33-bit PES timestamp range.\n");
    return (EXIT_FAILURE);
  }

  *new90 = (uint64_t) llroundl (transformed);
  return (EXIT_SUCCESS);
}

int
transform_timestamp_ms (const OPTIONS *options, int64_t originalms, int64_t *newms) {

  long double transformed;
  long double ratio;
  long double maxms;

  transformed = (long double) originalms;

  if (options->offset_flag) {
    transformed += (long double) options->offset.totalms;

  } else if (options->sync_flag) {
    if (options->oldlastms <= options->oldfirstms) {
      fprintf (stderr, "Invalid synchronization anchor interval.\n");
      return (EXIT_FAILURE);
    }

    ratio = ((long double) options->newlastms - (long double) options->newfirstms) /
            ((long double) options->oldlastms - (long double) options->oldfirstms);
    transformed = (long double) options->newfirstms +
                  ((long double) originalms - (long double) options->oldfirstms) * ratio;
  }

  if (transformed < 0.0L) {
    transformed = 0.0L;
  }

  // Keep IDX timestamps representable by the corresponding 33-bit PES clock.
  maxms = (long double) PES_TS_MAX / 90.0L;
  if (transformed > maxms) {
    fprintf (stderr, "Adjusted IDX timestamp exceeds the 33-bit PES timestamp range.\n");
    return (EXIT_FAILURE);
  }

  *newms = (int64_t) llroundl (transformed);
  return (EXIT_SUCCESS);
}
