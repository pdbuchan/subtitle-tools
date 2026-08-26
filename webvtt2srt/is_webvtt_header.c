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

// Determine whether a line is a valid WEBVTT file header.
//
// BOM handling is deliberately performed on raw input bytes before readline()
// is called. WebVTT permits optional header text after the WEBVTT signature when
// it is separated from the signature by a space or tab.
int
is_webvtt_header (const char *text) {

  size_t length;

  if (text == NULL) {
    return (0);
  }

  length = strlen (text);

  // The required signature consists of the six literal characters WEBVTT.
  if ((length < 6u) || (strncmp (text, "WEBVTT", 6u) != 0)) {
    return (0);
  }

  // If header text follows the signature, the WebVTT syntax requires it to
  // be separated from WEBVTT by a space or tab.
  return ((length == 6u) || (text[6] == ' ') || (text[6] == '\t'));
}
