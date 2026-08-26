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

// State for the dynamically growing converted cue-text buffer.
typedef struct {
  char *output;
  size_t length;
  size_t capacity;
} OUTPUT_STATE;

// Append bytes to the dynamically growing output string.
static int
append_output (OUTPUT_STATE *state, const char *text, size_t length) {

  size_t needed, new_capacity;
  char *tmp;

  if ((state == NULL) || ((text == NULL) && (length != 0u))) {
    errno = EINVAL;
    return (-1);
  }

  if (length > (SIZE_MAX - state->length - 1u)) {
    errno = ENOMEM;
    return (-1);
  }

  // Determine needed memory allocation.
  needed = state->length + length + 1u;

  // Increase memory allocation to meet current needs.
  if (needed > state->capacity) {
    new_capacity = (state->capacity == 0u) ? INITIAL_LINE_SIZE : state->capacity;

    while (new_capacity < needed) {
      if (new_capacity > (SIZE_MAX / 2u)) {
        new_capacity = needed;
        break;
      }

      new_capacity *= 2u;
    }

    // Re-allocate memory to new capacity.
    tmp = realloc (state->output, new_capacity);
    if (tmp == NULL) {
      return (-1);
    }

    state->output = tmp;
    state->capacity = new_capacity;
  }

  if (length != 0u) {
    memcpy (state->output + state->length, text, length);
    state->length += length;
  }

  state->output[state->length] = '\0';

  return (0);
}

// Append one ordinary NUL-terminated string.
static int
append_string (OUTPUT_STATE *state, const char *text) {

  return (append_output (state, text, strlen (text)));
}

// Append a font name while escaping characters that would otherwise terminate
// or corrupt the generated SRT <font> attribute.
static int
append_font_name (OUTPUT_STATE *state, const char *font) {

  const char *replacement;
  size_t i;

  for (i = 0u; font[i] != '\0'; i++) {
    replacement = NULL;

    switch (font[i]) {

      case '&':
        replacement = "&amp;";
        break;

      case '"':
        replacement = "&quot;";
        break;

      case '<':
        replacement = "&lt;";
        break;

      case '>':
        replacement = "&gt;";
        break;
    }  // End switch

    if (replacement != NULL) {
      if (append_string (state, replacement) != 0) {
        return (-1);
      }
    } else if (append_output (state, font + i, 1u) != 0) {
      return (-1);
    }
  }

  return (0);
}

// Merge file defaults, cue-persistent controls, and one line's local controls.
static void
merge_format (const FORMAT_STATE *defaults, const FORMAT_STATE *persistent, const FORMAT_STATE *local, FORMAT_STATE *effective) {

  *effective = *defaults;

  // Style controls are additive across the three scopes because MicroDVD has
  // no corresponding "turn default style off" control.
  effective->styles = defaults->styles | persistent->styles | local->styles;

  if (persistent->has_color) {
    effective->color = persistent->color;
    effective->has_color = 1;
  }

  if (local->has_color) {
    effective->color = local->color;
    effective->has_color = 1;
  }

  if (persistent->has_size) {
    effective->size = persistent->size;
    effective->has_size = 1;
  }

  if (local->has_size) {
    effective->size = local->size;
    effective->has_size = 1;
  }

  if (persistent->has_font) {
    memcpy (effective->font, persistent->font, sizeof (effective->font));
    effective->has_font = 1;
  }

  if (local->has_font) {
    memcpy (effective->font, local->font, sizeof (effective->font));
    effective->has_font = 1;
  }
}

// Append opening SRT markup for one complete logical subtitle line.
static int
open_format (OUTPUT_STATE *state, const FORMAT_STATE *format) {

  char number[64];
  int written;

  // Font face, size, and color can share one <font> element. These attributes
  // are widely understood extensions to SRT, though exact player support for
  // face and size is not universal.
  if (format->has_font || format->has_size || format->has_color) {
    if (append_string (state, "<font") != 0) {
      return (-1);
    }

    if (format->has_font) {
      if ((append_string (state, " face=\"") != 0) ||
          (append_font_name (state, format->font) != 0) ||
          (append_string (state, "\"") != 0)) {
        return (-1);
      }
    }

    if (format->has_size) {
      written = snprintf (number, sizeof (number), " size=\"%u\"", format->size);
      if ((written < 0) || ((size_t) written >= sizeof (number)) ||
          (append_output (state, number, (size_t) written) != 0)) {
        return (-1);
      }
    }

    if (format->has_color) {
      written = snprintf (number, sizeof (number), " color=\"#%06X\"", format->color);
      if ((written < 0) || ((size_t) written >= sizeof (number)) ||
          (append_output (state, number, (size_t) written) != 0)) {
        return (-1);
      }
    }

    if (append_string (state, ">") != 0) {
      return (-1);
    }
  }

  if ((format->styles & STYLE_BOLD) && (append_string (state, "<b>") != 0)) {
    return (-1);
  }

  if ((format->styles & STYLE_ITALIC) && (append_string (state, "<i>") != 0)) {
    return (-1);
  }

  if ((format->styles & STYLE_UNDERLINE) && (append_string (state, "<u>") != 0)) {
    return (-1);
  }

  // STYLE_STRIKE is intentionally not emitted. Strike-through is not part of
  // the small, portable set of SRT formatting tags supported by most players.

  return (0);
}

// Append closing SRT markup in the reverse order from open_format().
static int
close_format (OUTPUT_STATE *state, const FORMAT_STATE *format) {

  if ((format->styles & STYLE_UNDERLINE) && (append_string (state, "</u>") != 0)) {
    return (-1);
  }

  if ((format->styles & STYLE_ITALIC) && (append_string (state, "</i>") != 0)) {
    return (-1);
  }

  if ((format->styles & STYLE_BOLD) && (append_string (state, "</b>") != 0)) {
    return (-1);
  }

  if ((format->has_font || format->has_size || format->has_color) && (append_string (state, "</font>") != 0)) {
    return (-1);
  }

  return (0);
}

// Apply format tags from the beginning of one logical MicroDVD text line.
//
// When persistent_only is nonzero, only upper-case controls are applied; all
// recognized controls are still consumed so later upper-case tags can be
// found. Otherwise only lower-case controls are applied to the local state.
static int
read_line_tags (const char **text, const char *line_end, FORMAT_STATE *format, int persistent_only, int *slash_italic) {

  size_t length;
  int recognized, apply_tag;
  char key;
  const char *p, *end;

  p = *text;

  if ((line_end == NULL) || (p > line_end)) {
    errno = EINVAL;
    return (-1);
  }

  // Some older MicroDVD samples use a leading slash as a non-persistent
  // shorthand for italic text.
  if ((p < line_end) && (*p == '/')) {
    if (!persistent_only && (slash_italic != NULL)) {
      *slash_italic = 1;
    }
    p++;
  }

  while (((size_t) (line_end - p) >= 3u) && (p[0] == '{') && (p[1] != '\0') && (p[2] == ':')) {
    key = p[1];
    end = memchr (p + 3, '}', (size_t) (line_end - (p + 3)));
    if (end == NULL) {
      break;
    }

    length = (size_t) (end - (p + 3));
    apply_tag = persistent_only ? isupper ((unsigned char) key) : islower ((unsigned char) key);

    if (apply_tag) {
      if (apply_format_tag (format, key, p + 3, length, &recognized) != 0) {
        return (-1);
      }
    } else {
      FORMAT_STATE ignored;

      memset (&ignored, 0, sizeof (ignored));
      if (apply_format_tag (&ignored, key, p + 3, length, &recognized) != 0) {
        return (-1);
      }
    }

    // Like FFmpeg's MicroDVD reader, an unknown control-looking sequence is
    // treated as visible text rather than silently discarded.
    if (!recognized) {
      break;
    }

    p = end + 1;
  }

  // FFmpeg also accepts the slash italic shorthand immediately after an
  // initial sequence of formatting controls.
  if ((p < line_end) && (*p == '/')) {
    if (!persistent_only && (slash_italic != NULL)) {
      *slash_italic = 1;
    }
    p++;
  }

  *text = p;

  return (0);
}

// Collect upper-case MicroDVD formatting controls that apply to the whole cue.
static int
find_persistent_format (const char *text, FORMAT_STATE *persistent) {

  const char *p, *line_end;

  memset (persistent, 0, sizeof (*persistent));
  p = text;

  while (1) {
    line_end = strchr (p, '|');
    if (line_end == NULL) {
      line_end = p + strlen (p);
    }

    if (read_line_tags (&p, line_end, persistent, 1, NULL) != 0) {
      return (-1);
    }

    if (*line_end == '\0') {
      break;
    }

    p = line_end + 1;
  }

  return (0);
}

// Convert MicroDVD cue text and formatting to SRT-compatible text.
//
// Upper-case controls are cue-persistent; lower-case controls and the leading
// slash shorthand apply only to the logical line containing them. MicroDVD's
// pipe separator becomes a literal newline in the SRT cue.
int
convert_text (const char *text, const FORMAT_STATE *defaults, char **converted) {

  size_t visible_length;
  int slash_italic;
  const char *p, *line_end, *visible;
  FORMAT_STATE persistent, local, effective;
  OUTPUT_STATE output;

  if ((text == NULL) || (defaults == NULL) || (converted == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  memset (&output, 0, sizeof (output));

  // Discover all cue-persistent upper-case controls before writing any text so
  // a persistent tag remains effective even if it appears after the first
  // logical line of the MicroDVD cue.
  if (find_persistent_format (text, &persistent) != 0) {
    return (-1);
  }

  p = text;

  while (1) {
    line_end = strchr (p, '|');
    if (line_end == NULL) {
      line_end = p + strlen (p);
    }

    memset (&local, 0, sizeof (local));
    slash_italic = 0;
    visible = p;

    if (read_line_tags (&visible, line_end, &local, 0, &slash_italic) != 0) {
      free (output.output);
      return (-1);
    }

    if (slash_italic) {
      local.styles |= STYLE_ITALIC;
    }

    // read_line_tags() must never consume past this logical line's pipe.
    if (visible > line_end) {
      free (output.output);
      errno = EINVAL;
      return (-1);
    }

    visible_length = (size_t) (line_end - visible);
    merge_format (defaults, &persistent, &local, &effective);

    if ((open_format (&output, &effective) != 0) ||
        (append_output (&output, visible, visible_length) != 0) ||
        (close_format (&output, &effective) != 0)) {
      free (output.output);
      return (-1);
    }

    if (*line_end == '\0') {
      break;
    }

    if (append_output (&output, "\n", 1u) != 0) {
      free (output.output);
      return (-1);
    }

    p = line_end + 1;
  }

  // Ensure even a completely empty subtitle returns an allocated empty string.
  if (output.output == NULL) {
    if (append_output (&output, "", 0u) != 0) {
      return (-1);
    }
  }

  *converted = output.output;

  return (0);
}
