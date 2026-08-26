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

// Parse a WebVTT timestamp and write its SubRip representation.
//
// WebVTT permits either HH:MM:SS.mmm or MM:SS.mmm. SubRip conventionally
// uses HH:MM:SS,mmm, so the shorter WebVTT form receives an hour value of 00.
int
convert_timestamp (const char *src, char *dst, size_t dst_size) {

  size_t npart, ndigit;
  int written;
  unsigned int digit;
  unsigned long long value;
  unsigned long long part[3], hours, minutes, seconds, milliseconds;
  const char *p;

  if ((src == NULL) || (dst == NULL) || (dst_size == 0u)) {
    return (-1);
  }

  p = src;
  npart = 0u;

  // Read the colon-separated decimal fields preceding the fractional part.
  // There must be either two fields (MM:SS) or three (HH:MM:SS).
  while (1) {

    if (npart >= 3u) {
      return (-1);
    }

    value = 0ull;
    ndigit = 0u;

    // Accumulate one decimal field while checking for integer overflow.
    while (isdigit ((unsigned char) *p)) {

      digit = (unsigned int) (*p - '0');

      if (value > ((ULLONG_MAX - digit) / 10ull)) {
        return (-1);
      }

      value = value * 10ull + digit;
      p++;
      ndigit++;
    }

    // Empty timestamp fields such as 00::01.000 are invalid.
    if (ndigit == 0u) {
      return (-1);
    }

    part[npart++] = value;

    if (*p != ':') {
      break;
    }

    p++;
  }

  // After two or three fields, WebVTT requires a dot before milliseconds.
  if (((npart != 2u) && (npart != 3u)) || (*p != '.')) {
    return (-1);
  }

  p++;
  milliseconds = 0ull;

  // WebVTT timestamps contain exactly three fractional digits.
  for (ndigit = 0u; ndigit < 3u; ndigit++) {
    if (!isdigit ((unsigned char) *p)) {
      return (-1);
    }

    milliseconds = milliseconds * 10ull + (unsigned long long) (*p - '0');
    p++;
  }

  // No additional characters are permitted inside the timestamp token.
  if (*p != '\0') {
    return (-1);
  }

  // Map the parsed fields to hours, minutes, and seconds. The two-field form
  // has no explicit hour component, so SubRip receives 00 hours.
  if (npart == 2u) {
    hours = 0ull;
    minutes = part[0];
    seconds = part[1];
  } else {
    hours = part[0];
    minutes = part[1];
    seconds = part[2];
  }

  // Minutes and seconds are clock components and therefore must remain below
  // 60. Hours may exceed 23 because subtitle timelines can be arbitrarily
  // long.
  if ((minutes > 59ull) || (seconds > 59ull)) {
    return (-1);
  }

  // SubRip uses a comma, rather than WebVTT's dot, before milliseconds.
  written = snprintf (dst, dst_size, "%02llu:%02llu:%02llu,%03llu", hours, minutes, seconds, milliseconds);

  if ((written < 0) || ((size_t) written >= dst_size)) {
    return (-1);
  }

  return (0);
}
