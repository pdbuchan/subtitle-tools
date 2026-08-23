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

// Read one text line. Carriage returns are discarded, newline is retained if
// present, and the returned string is always NUL terminated. Overlong input
// lines are consumed so the next call begins at the next real line.
int
readline (FILE *fi, char *line, int limit) {

  int c;
  size_t pos, capacity;

  if (fi == NULL || line == NULL || limit <= 1) return -1;
  capacity = (size_t) limit;
  pos = 0;

  for (;;) {
    c = fgetc (fi);
    if (c == EOF) {
      line[pos] = '\0';
      return pos == 0 ? -1 : 0;
    }

    if (c == '\r') continue;

    if (pos + 1 < capacity) {
      line[pos++] = (char) (unsigned char) c;
      line[pos] = '\0';
    }

    if (c == '\n') return 0;

    if (pos + 1 == capacity) {
      // The buffer is full. Consume the remainder of this physical line.
      do {
        c = fgetc (fi);
      } while (c != '\n' && c != EOF);
      line[pos] = '\0';
      return 0;
    }
  }
}
