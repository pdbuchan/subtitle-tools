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

// Convert one MicroDVD frame number to an SRT timestamp.
//
// The frame time is frame / frame_rate seconds. Because SRT stores only whole
// milliseconds, the result is rounded to the nearest millisecond rather than
// truncated. Long-double arithmetic reduces accumulated error for fractional
// rates such as 24000/1001.
int
frame_to_timestamp (unsigned long long frame, long double frame_rate, char *dst, size_t dst_size) {

  int written;
  unsigned long long hours, minutes, seconds, milliseconds, total_milliseconds;
  long double time_milliseconds, rounded;

  if ((frame_rate <= 0.0l) || !isfinite (frame_rate) || (dst == NULL) || (dst_size == 0u)) {
    return (-1);
  }

  // Perform the multiplication in floating point so frame * 1000 cannot wrap
  // an unsigned integer before division by the frame rate.
  time_milliseconds = ((long double) frame * 1000.0l) / frame_rate;
  if (!isfinite (time_milliseconds) || (time_milliseconds < 0.0l)) {
    return (-1);
  }

  rounded = floorl (time_milliseconds + 0.5l);
  if (rounded > (long double) ULLONG_MAX) {
    return (-1);
  }

  total_milliseconds = (unsigned long long) rounded;

  // Split the rounded absolute time into SubRip's clock fields.
  hours = total_milliseconds / 3600000ull;
  total_milliseconds %= 3600000ull;
  minutes = total_milliseconds / 60000ull;
  total_milliseconds %= 60000ull;
  seconds = total_milliseconds / 1000ull;
  milliseconds = total_milliseconds % 1000ull;

  written = snprintf (dst, dst_size, "%02llu:%02llu:%02llu,%03llu", hours, minutes, seconds, milliseconds);
  if ((written < 0) || ((size_t) written >= dst_size)) {
    return (-1);
  }

  return (0);
}
