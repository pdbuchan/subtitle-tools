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

// Parse a MicroDVD {DEFAULT} formatting line.
//
// Both upper- and lower-case formatting keys are accepted after {DEFAULT};
// files in the wild use both forms. Unknown well-formed control tags are
// ignored because the entire {DEFAULT} record is metadata rather than visible
// subtitle text.
int
parse_default_line (const char *line, FORMAT_STATE *defaults, int *is_default) {

  size_t length;
  int recognized;
  char key;
  const char *p, *end;

  if ((line == NULL) || (defaults == NULL) || (is_default == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  *is_default = 0;

  if (strncmp (line, "{DEFAULT}", 9u) != 0) {
    return (0);
  }

  *is_default = 1;
  p = line + 9;

  // The remainder consists solely of zero or more formatting controls. Permit
  // whitespace between controls, but reject arbitrary visible text.
  while (*p != '\0') {
    while (isspace ((unsigned char) *p)) {
      p++;
    }

    if (*p == '\0') {
      break;
    }

    if ((p[0] != '{') || (p[1] == '\0') || (p[2] != ':')) {
      return (-1);
    }

    key = p[1];
    end = strchr (p + 3, '}');
    if (end == NULL) {
      return (-1);
    }

    length = (size_t) (end - (p + 3));
    if (apply_format_tag (defaults, key, p + 3, length, &recognized) != 0) {
      return (-1);
    }

    // Unknown tags are metadata that this converter does not interpret. They
    // are skipped here rather than making an otherwise usable file fail.
    p = end + 1;
  }

  return (0);
}
