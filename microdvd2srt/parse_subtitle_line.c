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

// Parse one unsigned decimal frame number terminated by a closing brace.
static int
parse_frame (const char **text, unsigned long long *frame) {

  unsigned int digit;
  unsigned long long value;
  const char *p;

  if ((text == NULL) || (*text == NULL) || (frame == NULL)) {
    return (-1);
  }

  p = *text;
  value = 0ull;

  // MicroDVD frame fields must contain at least one decimal digit. Although
  // some readers accept an omitted ending frame, SRT requires an end time, so
  // that extension is deliberately rejected by this converter.
  if (!isdigit ((unsigned char) *p)) {
    return (-1);
  }

  while (isdigit ((unsigned char) *p)) {
    digit = (unsigned int) (*p - '0');

    if (value > ((ULLONG_MAX - digit) / 10ull)) {
      return (-1);
    }

    value = value * 10ull + digit;
    p++;
  }

  if (*p != '}') {
    return (-1);
  }

  *frame = value;
  *text = p + 1;

  return (0);
}

// Parse the {start-frame}{end-frame} prefix of one MicroDVD subtitle line.
//
// On success, text points into the caller's original string immediately after
// the second closing brace. No subtitle-text copy is made here.
int
parse_subtitle_line (const char *line, unsigned long long *start_frame,
                     unsigned long long *end_frame, const char **text) {

  const char *p;

  if ((line == NULL) || (start_frame == NULL) || (end_frame == NULL) || (text == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  p = line;

  if (*p != '{') {
    return (-1);
  }

  p++;
  if (parse_frame (&p, start_frame) != 0) {
    return (-1);
  }

  if (*p != '{') {
    return (-1);
  }

  p++;
  if (parse_frame (&p, end_frame) != 0) {
    return (-1);
  }

  *text = p;

  return (0);
}
