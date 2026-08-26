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

// Read one text line of arbitrary length.
//
// The returned string does not contain the terminating LF or CRLF. Return 1
// when a line was read, 0 at end-of-file, and -1 on an input or allocation
// error. On success, the caller owns the returned string and must free it.
int
readline (FILE *fi, char **line) {

  size_t capacity, length;
  int c;
  char *buffer, *tmp;

  if ((fi == NULL) || (line == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  // Start with a modest buffer and grow it only when a long line requires
  // more room. One extra byte is always reserved for the terminating NUL.
  capacity = INITIAL_LINE_SIZE;
  length = 0u;
  buffer = malloc (capacity);
  if (buffer == NULL) {
    return (-1);
  }

  // Read through LF or EOF. CR is retained here temporarily so that CRLF can
  // be distinguished and normalized after the line has been collected.
  while ((c = fgetc (fi)) != EOF) {
    if (c == '\n') {
      break;
    }

    if ((length + 1u) >= capacity) {
      // Doubling is efficient for arbitrary line lengths, but guard against
      // size_t overflow before changing the allocation size.
      if (capacity > (SIZE_MAX / 2u)) {
        free (buffer);
        errno = ENOMEM;
        return (-1);
      }

      capacity *= 2u;
      tmp = realloc (buffer, capacity);
      if (tmp == NULL) {
        free (buffer);
        return (-1);
      }

      buffer = tmp;
    }

    buffer[length++] = (char) c;
  }

  // fgetc() uses EOF both for end-of-file and for an input error. ferror()
  // distinguishes the latter case.
  if (ferror (fi)) {
    free (buffer);
    return (-1);
  }

  // EOF before any bytes were obtained means there is no further line.
  if ((c == EOF) && (length == 0u)) {
    free (buffer);
    return (0);
  }

  // Normalize a CRLF line ending by removing the CR. A bare CR within a line
  // is otherwise preserved as input data.
  if ((length > 0u) && (buffer[length - 1u] == '\r')) {
    length--;
  }

  // Convert the collected bytes into an ordinary C string and return
  // ownership of the allocation to the caller.
  buffer[length] = '\0';
  *line = buffer;

  return (1);
}
