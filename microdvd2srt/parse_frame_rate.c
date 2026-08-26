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

#include "microdvd2srt.h"

// Parse a positive video frame rate.
//
// Ordinary decimal values such as 25 or 23.976 are accepted. A rational form
// such as 24000/1001 is also accepted so rates whose exact value is awkward to
// express as a finite decimal can be supplied without unnecessary rounding.
int
parse_frame_rate (const char *text, long double *frame_rate) {

  long double numerator, denominator, value;
  char *end;
  const char *p;

  if ((text == NULL) || (frame_rate == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  // Safe as long sa text is null-terminated.
  p = text;
  while (isspace ((unsigned char) *p)) {
    p++;
  }

  if (*p == '\0') {
    return (-1);
  }

  errno = 0;
  numerator = strtold (p, &end);
  if ((end == p) || (errno == ERANGE) || !isfinite (numerator) || (numerator <= 0.0L)) {
    return (-1);
  }

  p = end;
  while (isspace ((unsigned char) *p)) {
    p++;
  }

  // A slash introduces an optional positive denominator. Without a slash,
  // the numerator itself is the complete frame-rate value.
  if (*p == '/') {
    p++;

    while (isspace ((unsigned char) *p)) {
      p++;
    }

    errno = 0;
    denominator = strtold (p, &end);
    if ((end == p) || (errno == ERANGE) || !isfinite (denominator) || (denominator <= 0.0L)) {
      return (-1);
    }

    p = end;
    value = numerator / denominator;
  } else {
    value = numerator;
  }

  while (isspace ((unsigned char) *p)) {
    p++;
  }

  // Reject trailing characters, zero, infinity, and values that underflowed
  // to zero during a rational division.
  if ((*p != '\0') || !isfinite (value) || (value <= 0.0L)) {
    return (-1);
  }

  *frame_rate = value;

  return (0);
}
