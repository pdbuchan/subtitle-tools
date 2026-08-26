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

// Return the numeric value of one hexadecimal digit, or -1 if invalid.
static int
hex_value (unsigned char c) {

  if ((c >= '0') && (c <= '9')) {
    return ((int) (c - '0'));
  }

  c = (unsigned char) tolower (c);

  if ((c >= 'a') && (c <= 'f')) {
    return ((int) (c - 'a') + 10);
  }

  return (-1);
}

// Apply one MicroDVD formatting tag to a formatting state.
//
// key is the single tag letter and value is the text between ':' and '}'.
// recognized is set when the key belongs to a known MicroDVD control, even if
// that control has no portable SubRip representation and is therefore ignored.
int
apply_format_tag (FORMAT_STATE *state, char key, const char *value, size_t length, int *recognized) {

  size_t i, start;
  int hi, lo;
  unsigned int styles, bgr, red, green, blue;
  unsigned long long size_value;

  if ((state == NULL) || (value == NULL) || (recognized == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  *recognized = 1;

  switch (key) {

    // y/Y selects a set of text styles. Commas and whitespace are permitted
    // between the style letters because both forms occur in existing files.
    case 'y':
    case 'Y':
      styles = 0u;

      for (i = 0u; i < length; i++) {

        switch (tolower ((unsigned char) value[i])) {

          case 'i':
            styles |= STYLE_ITALIC;
            break;

          case 'b':
            styles |= STYLE_BOLD;
            break;

          case 'u':
            styles |= STYLE_UNDERLINE;
            break;

          case 's':
            styles |= STYLE_STRIKE;
            break;

          case ',':
          case ' ':

          case '\t':
            break;

          default:
            return (-1);
        }  // End switch tolower()
      }

      state->styles = styles;
      break;

    // MicroDVD stores colors in $BBGGRR order. Convert the three source bytes
    // into the ordinary RGB order expected by SRT <font color> markup.
    case 'c':
    case 'C':
      start = 0u;
      if ((start < length) && ((value[start] == '$') || (value[start] == '#'))) {
        start++;
      }

      if ((length - start) != 6u) {
        return (-1);
      }

      bgr = 0u;
      for (i = start; i < length; i++) {
        hi = hex_value ((unsigned char) value[i]);
        if (hi < 0) {
          return (-1);
        }

        bgr = (bgr << 4) | (unsigned int) hi;
      }

      blue = (bgr >> 16) & 0xffu;
      green = (bgr >> 8) & 0xffu;
      red = bgr & 0xffu;
      state->color = (red << 16) | (green << 8) | blue;
      state->has_color = 1;
      break;

    // Font names are copied into a bounded state buffer. The complete input
    // line itself remains arbitrarily long; only the format's font-name field
    // receives this practical limit.
    case 'f':
    case 'F':
      if ((length == 0u) || (length >= sizeof (state->font))) {
        return (-1);
      }

      memcpy (state->font, value, length);
      state->font[length] = '\0';
      state->has_font = 1;
      break;

    // Font size is a positive decimal integer. Preserve the numeric value for
    // SRT players that understand a size attribute on the <font> element.
    case 's':
    case 'S':
      if (length == 0u) {
        return (-1);
      }

      size_value = 0ull;
      for (i = 0u; i < length; i++) {
        if (!isdigit ((unsigned char) value[i])) {
          return (-1);
        }

        lo = value[i] - '0';
        if (size_value > ((ULLONG_MAX - (unsigned int) lo) / 10ull)) {
          return (-1);
        }

        size_value = size_value * 10ull + (unsigned int) lo;
      }

      if ((size_value == 0ull) || (size_value > UINT_MAX)) {
        return (-1);
      }

      state->size = (unsigned int) size_value;
      state->has_size = 1;
      break;

    // Charset and position/coordinate controls have no sufficiently portable
    // SRT equivalent. Recognize them so they can be removed without treating
    // their syntax as visible subtitle text.
    case 'h':
    case 'H':
    case 'p':
    case 'P':
    case 'o':
    case 'O':
      break;

    default:
      *recognized = 0;
      break;
  }  // End switch key

  return (0);
}
