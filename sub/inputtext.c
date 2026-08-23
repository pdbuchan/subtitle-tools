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

// Obtain a text string from standard input. It can include spaces.
int
inputtext (char *text) {

  size_t len;

  if (fgets (text, MAX_STRINGLEN, stdin) == NULL) {
    fprintf (stderr, "Unable to read text from standard input.\n");
    exit (EXIT_FAILURE);
  }

  len = strlen (text);
  if (len > 0 && text[len - 1] == '\n') {
    text[len - 1] = '\0';
  } else if (len == MAX_STRINGLEN - 1) {
    int ch;
    while ((ch = getchar ()) != '\n' && ch != EOF) {
      // Discard the remainder of an overlong input line.
    }
    fprintf (stderr, "Input text is too long; maximum is %d characters.\n", MAX_STRINGLEN - 2);
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

