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

// Test whether text begins with a complete WebVTT keyword.
//
// A match is accepted only when the keyword is followed by end-of-string or
// whitespace, preventing a prefix such as NOTEBOOK from matching NOTE.
int
starts_keyword (const char *text, const char *keyword) {

  size_t length;
  int result;

  // Callers normally pass valid strings, but reject NULL defensively.
  if ((text == NULL) || (keyword == NULL)) {
    return (0);
  }

  length = strlen (keyword);

  // The characters at the beginning of text must exactly match keyword.
  if (strncmp (text, keyword, length) != 0) {
    return (0);
  }

  // Require a keyword boundary. Without this test, NOTEBOOK would be
  // incorrectly classified as a NOTE block.
  result = (text[length] == '\0') || isspace ((unsigned char) text[length]);

  return (result);
}
