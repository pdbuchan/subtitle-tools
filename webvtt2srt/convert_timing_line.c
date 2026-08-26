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

// Convert a WebVTT timing line to a SubRip timing line.
//
// Everything following the WebVTT ending timestamp is a cue setting (for
// example, "align:start" or "position:20%") and has no direct SRT timing-line
// equivalent, so those settings are intentionally discarded.
int
convert_timing_line (const char *src, char *dst, size_t dst_size) {

  size_t length;
  int written;
  const char *arrow, *start_begin, *start_end, *end_begin, *end_end;
  char start[64], end[64], srt_start[64], srt_end[64];

  if ((src == NULL) || (dst == NULL) || (dst_size == 0u)) {
    return (-1);
  }

  // Locate the mandatory separator between the beginning and ending
  // timestamps. The surrounding whitespace is handled independently below.
  arrow = strstr (src, "-->");
  if (arrow == NULL) {
    return (-1);
  }

  // Isolate the beginning timestamp, permitting surrounding whitespace but
  // not copying it into the timestamp parser.
  start_begin = src;
  while (isspace ((unsigned char) *start_begin)) {
    start_begin++;
  }

  start_end = arrow;
  while ((start_end > start_begin) && isspace ((unsigned char) start_end[-1])) {
    start_end--;
  }

  length = (size_t) (start_end - start_begin);
  if ((length == 0u) || (length >= sizeof (start))) {
    return (-1);
  }

  memcpy (start, start_begin, length);
  start[length] = '\0';

  // Skip whitespace following the arrow, then copy only the ending timestamp
  // token. Any later whitespace introduces WebVTT cue settings that SRT does
  // not represent and that are therefore discarded.
  end_begin = arrow + 3;
  while (isspace ((unsigned char) *end_begin)) {
    end_begin++;
  }

  end_end = end_begin;
  while ((*end_end != '\0') && !isspace ((unsigned char) *end_end)) {
    end_end++;
  }

  length = (size_t) (end_end - end_begin);
  if ((length == 0u) || (length >= sizeof (end))) {
    return (-1);
  }

  memcpy (end, end_begin, length);
  end[length] = '\0';

  // Convert both timestamp tokens independently so malformed input on either
  // side of the arrow causes the complete timing line to be rejected.
  if ((convert_timestamp (start, srt_start, sizeof (srt_start)) != 0) ||
      (convert_timestamp (end, srt_end, sizeof (srt_end)) != 0)) {
    return (-1);
  }

  // Assemble the conventional SubRip timing line.
  written = snprintf (dst, dst_size, "%s --> %s", srt_start, srt_end);
  if ((written < 0) || ((size_t) written >= dst_size)) {
    return (-1);
  }

  return (0);
}
