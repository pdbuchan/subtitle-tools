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

// State retained while all text lines belonging to one cue are converted.
// Keeping the state for the whole cue permits markup to span line boundaries.
typedef struct {
  char *output;
  size_t output_length;
  size_t output_capacity;
  unsigned char *color_stack;
  size_t color_depth;
  size_t color_capacity;
  size_t rt_depth;
} CUE_TEXT_STATE;

// Append bytes to the dynamically growing converted cue-text buffer.
//
// The allocation is enlarged geometrically so repeated small additions do not
// require a realloc() for every character or replacement tag. The buffer is
// always kept NUL-terminated for convenient use as a C string.
static int
append_output (CUE_TEXT_STATE *state, const char *text, size_t text_length) {

  size_t needed, new_capacity;
  char *tmp;

  if ((state == NULL) || ((text == NULL) && (text_length != 0u))) {
    errno = EINVAL;
    return (-1);
  }

  // Account for both the bytes being appended and the terminating NUL.
  if (text_length > (SIZE_MAX - state->output_length - 1u)) {
    errno = ENOMEM;
    return (-1);
  }

  needed = state->output_length + text_length + 1u;

  if (needed > state->output_capacity) {
    new_capacity = (state->output_capacity == 0u) ? INITIAL_LINE_SIZE : state->output_capacity;

    // Double the allocation until the requested text fits. If doubling would
    // overflow size_t, use the exact required size instead.
    while (new_capacity < needed) {
      if (new_capacity > (SIZE_MAX / 2u)) {
        new_capacity = needed;
        break;
      }

      new_capacity *= 2u;
    }

    tmp = realloc (state->output, new_capacity);
    if (tmp == NULL) {
      return (-1);
    }

    state->output = tmp;
    state->output_capacity = new_capacity;
  }

  if (text_length != 0u) {
    memcpy (state->output + state->output_length, text, text_length);
    state->output_length += text_length;
  }

  state->output[state->output_length] = '\0';

  return (0);
}

// Compare a non-NUL-terminated character span with a NUL-terminated string.
static int
span_equal (const char *text, size_t length, const char *word) {

  size_t word_length;

  if ((text == NULL) || (word == NULL)) {
    return (0);
  }

  word_length = strlen (word);

  return ((length == word_length) && (memcmp (text, word, length) == 0));
}

// Return the SRT color corresponding to one WebVTT foreground class.
//
// WebVTT defines black, red, lime, yellow, blue, magenta, cyan, and white as
// default cue-text classes. Some real-world subtitle streams use "green" in
// place of the WebVTT name "lime", so green is accepted as a compatibility
// alias for the same #00FF00 value.
static const char *
class_color (const char *text, size_t length) {

  if (span_equal (text, length, "black")) {
    return ("#000000");
  }

  if (span_equal (text, length, "red")) {
    return ("#FF0000");
  }

  if (span_equal (text, length, "lime") || span_equal (text, length, "green")) {
    return ("#00FF00");
  }

  if (span_equal (text, length, "yellow")) {
    return ("#FFFF00");
  }

  if (span_equal (text, length, "blue")) {
    return ("#0000FF");
  }

  if (span_equal (text, length, "magenta")) {
    return ("#FF00FF");
  }

  if (span_equal (text, length, "cyan")) {
    return ("#00FFFF");
  }

  if (span_equal (text, length, "white")) {
    return ("#FFFFFF");
  }

  return (NULL);
}

// Find the last recognized foreground color in a WebVTT <c.class...> tag.
//
// A class list may contain several dot-separated names. WebVTT background
// classes begin with "bg_" and cannot be represented portably in SubRip, so
// they are ignored. Unknown classes are likewise ignored. If more than one
// recognized foreground color is present, the last one is used.
static const char *
cue_color (const char *tag, size_t length) {

  size_t start, end;
  const char *color, *candidate;

  color = NULL;

  // A color tag begins with c; anything after it must be a dot-separated
  // class list. A bare <c> therefore has no representable color.
  if ((length < 3u) || (tag[0] != 'c') || (tag[1] != '.')) {
    return (NULL);
  }

  start = 2u;

  while (start < length) {
    end = start;

    while ((end < length) && (tag[end] != '.')) {
      end++;
    }

    // Background colors and arbitrary CSS classes are deliberately ignored.
    // class_color() returns NULL for anything other than a supported
    // foreground color name.
    candidate = NULL;
    if ((end - start < 3u) || (memcmp (tag + start, "bg_", 3u) != 0)) {
      candidate = class_color (tag + start, end - start);
    }

    if (candidate != NULL) {
      color = candidate;
    }

    start = end + 1u;
  }

  return (color);
}

// Determine whether a WebVTT cue tag is an internal timestamp.
//
// Internal timestamps use the same MM:SS.mmm or HH:MM:SS.mmm forms as cue
// timing, but occur inside cue text to control progressive/karaoke display.
// SubRip cannot represent this intra-cue timing, so valid timestamp tags are
// recognized here and omitted from the converted text.
static int
is_internal_timestamp (const char *tag, size_t length) {

  size_t i, npart, ndigit;
  unsigned int digit;
  unsigned long long value, part[3];

  if ((tag == NULL) || (length == 0u)) {
    return (0);
  }

  i = 0u;
  npart = 0u;

  // Parse the two or three colon-separated decimal components preceding the
  // milliseconds. The checks intentionally mirror convert_timestamp().
  while (1) {
    if (npart >= 3u) {
      return (0);
    }

    value = 0ull;
    ndigit = 0u;

    while ((i < length) && isdigit ((unsigned char) tag[i])) {
      digit = (unsigned int) (tag[i] - '0');

      if (value > ((ULLONG_MAX - digit) / 10ull)) {
        return (0);
      }

      value = value * 10ull + digit;
      i++;
      ndigit++;
    }

    if (ndigit == 0u) {
      return (0);
    }

    part[npart++] = value;

    if ((i >= length) || (tag[i] != ':')) {
      break;
    }

    i++;
  }

  if (((npart != 2u) && (npart != 3u)) || (i >= length) || (tag[i] != '.')) {
    return (0);
  }

  i++;

  // Exactly three fractional digits are required and must consume the entire
  // tag. Their numeric value is irrelevant because the tag will be removed.
  for (ndigit = 0u; ndigit < 3u; ndigit++) {
    if ((i >= length) || !isdigit ((unsigned char) tag[i])) {
      return (0);
    }

    i++;
  }

  if (i != length) {
    return (0);
  }

  // Minutes and seconds retain their clock-component limits just as they do
  // on an ordinary WebVTT timing line.
  if (npart == 2u) {
    return ((part[0] <= 59ull) && (part[1] <= 59ull));
  }

  return ((part[1] <= 59ull) && (part[2] <= 59ull));
}

// Push one <c> conversion state onto the dynamically growing color stack.
//
// A nonzero entry means the corresponding WebVTT <c> opening tag was
// converted to an SRT <font> tag, so its eventual </c> must become </font>.
// A zero entry means the opening tag was stripped and its closing tag must
// also be stripped. The stack preserves correct behavior for nested spans.
static int
push_color (CUE_TEXT_STATE *state, unsigned char converted) {

  size_t new_capacity;
  unsigned char *tmp;

  if (state == NULL) {
    errno = EINVAL;
    return (-1);
  }

  if (state->color_depth == state->color_capacity) {
    new_capacity = (state->color_capacity == 0u) ? 8u : state->color_capacity * 2u;

    if (new_capacity < state->color_capacity) {
      errno = ENOMEM;
      return (-1);
    }

    tmp = realloc (state->color_stack, new_capacity * sizeof (*state->color_stack));
    if (tmp == NULL) {
      return (-1);
    }

    state->color_stack = tmp;
    state->color_capacity = new_capacity;
  }

  state->color_stack[state->color_depth++] = converted;

  return (0);
}

// Convert one complete WebVTT cue-text tag.
//
// Supported formatting is translated to SRT markup. Unsupported tags return
// success without appending anything, which removes the tag while allowing
// the ordinary visible text between opening and closing tags to pass through.
static int
convert_tag (CUE_TEXT_STATE *state, const char *tag, size_t tag_length) {

  int written;
  char font_tag[32];
  const char *color;

  // While inside ruby <rt> annotation text, suppress all tags and text. Only
  // nested <rt> tags affect the depth needed to locate the matching close.
  if (state->rt_depth != 0u) {
    if (span_equal (tag, tag_length, "rt")) {
      state->rt_depth++;
    } else if (span_equal (tag, tag_length, "/rt")) {
      state->rt_depth--;
    }

    return (0);
  }

  // Progressive/karaoke timestamps cannot be represented within an SRT cue.
  if (is_internal_timestamp (tag, tag_length)) {
    return (0);
  }

  // Preserve bold, italic, and underline. Any WebVTT CSS class suffix is
  // deliberately discarded because SRT has no corresponding class concept.
  if ((tag_length >= 1u) && ((tag[0] == 'b') || (tag[0] == 'i') || (tag[0] == 'u')) && ((tag_length == 1u) || (tag[1] == '.'))) {
    if ((append_output (state, "<", 1u) != 0) ||
        (append_output (state, tag, 1u) != 0) ||
        (append_output (state, ">", 1u) != 0)) {
      return (-1);
    }

    return (0);
  }

  if ((tag_length == 2u) && (tag[0] == '/') && ((tag[1] == 'b') || (tag[1] == 'i') || (tag[1] == 'u'))) {
    if ((append_output (state, "</", 2u) != 0) ||
        (append_output (state, tag + 1u, 1u) != 0) ||
        (append_output (state, ">", 1u) != 0)) {
      return (-1);
    }

    return (0);
  }

  // Convert recognized <c.color> classes to SRT <font color> markup. Push a
  // state entry even when no known color is found so a later </c> can be
  // discarded rather than generating an unmatched </font> tag.
  if ((tag_length >= 1u) && (tag[0] == 'c') && ((tag_length == 1u) || (tag[1] == '.'))) {
    color = cue_color (tag, tag_length);

    if (push_color (state, (unsigned char) (color != NULL)) != 0) {
      return (-1);
    }

    if (color != NULL) {
      written = snprintf (font_tag, sizeof (font_tag), "<font color=\"%s\">", color);
      if ((written < 0) || ((size_t) written >= sizeof (font_tag))) {
        errno = EOVERFLOW;
        return (-1);
      }

      if (append_output (state, font_tag, (size_t) written) != 0) {
        return (-1);
      }
    }

    return (0);
  }

  // Match each </c> with the most recent <c>. Only emit </font> when that
  // opening tag produced a corresponding SRT <font> tag.
  if (span_equal (tag, tag_length, "/c")) {
    if (state->color_depth != 0u) {
      state->color_depth--;

      if ((state->color_stack[state->color_depth] != 0u) && (append_output (state, "</font>", 7u) != 0)) {
        return (-1);
      }
    }

    return (0);
  }

  // Preserve ruby base text by stripping <ruby>, but suppress pronunciation
  // annotation text beginning at <rt> until its matching </rt> is reached.
  if (span_equal (tag, tag_length, "rt")) {
    state->rt_depth = 1u;
    return (0);
  }

  if (span_equal (tag, tag_length, "ruby") ||
      span_equal (tag, tag_length, "/ruby") ||
      span_equal (tag, tag_length, "/rt")) {
    return (0);
  }

  // Voice (<v ...>), language (<lang ...>), and any other WebVTT-only tags
  // arrive here. Removing just the tag preserves their enclosed visible text.
  return (0);
}

// Convert one physical line of WebVTT cue text.
static int
convert_line (CUE_TEXT_STATE *state, const char *text) {

  size_t i, tag_end, tag_start, tag_length;

  if ((state == NULL) || (text == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  i = 0u;

  while (text[i] != '\0') {
    // Ordinary text is copied directly unless it belongs to an <rt>
    // pronunciation annotation currently being suppressed.
    if (text[i] != '<') {
      if ((state->rt_depth == 0u) && (append_output (state, text + i, 1u) != 0)) {
        return (-1);
      }

      i++;
      continue;
    }

    // Find the closing '>' of the candidate tag. An unmatched '<' is retained
    // literally rather than causing the rest of malformed cue text to vanish.
    tag_end = i + 1u;
    while ((text[tag_end] != '\0') && (text[tag_end] != '>')) {
      tag_end++;
    }

    if (text[tag_end] == '\0') {
      if ((state->rt_depth == 0u) && (append_output (state, text + i, 1u) != 0)) {
        return (-1);
      }

      i++;
      continue;
    }

    tag_start = i + 1u;
    tag_length = tag_end - tag_start;

    if (convert_tag (state, text + tag_start, tag_length) != 0) {
      return (-1);
    }

    i = tag_end + 1u;
  }

  return (0);
}

// Convert WebVTT cue text to markup that can be represented usefully in SRT.
//
// The whole cue is processed in one pass, including embedded line breaks, so
// markup that begins on one cue line and ends on another retains its state.
// Bold, italic, and underline tags are preserved while any WebVTT CSS classes
// attached to them are discarded. Recognized <c.color> spans become SRT
// <font color="#RRGGBB"> spans. Unsupported semantic tags are removed while
// preserving their visible text. Ruby <rt> annotation text and internal cue
// timestamps are omitted because SubRip has no equivalent representation.
int
convert_cue_text (const BLOCK *block, size_t first_line, char **dst) {

  size_t i;
  CUE_TEXT_STATE state;

  if ((block == NULL) || (dst == NULL) || (first_line > block->count)) {
    errno = EINVAL;
    return (-1);
  }

  memset (&state, 0, sizeof (state));
  *dst = NULL;

  // Create an initial empty C string. This also gives a cue with no payload a
  // valid allocated result that the caller can print and free normally.
  if (append_output (&state, "", 0u) != 0) {
    free (state.color_stack);
    free (state.output);
    return (-1);
  }

  for (i = first_line; i < block->count; i++) {
    if (convert_line (&state, block->line[i].text) != 0) {
      free (state.color_stack);
      free (state.output);
      return (-1);
    }

    // Reconstruct cue line breaks between source lines. If an <rt> annotation
    // spans the boundary, that newline belongs to the discarded annotation.
    if ((i + 1u < block->count) && (state.rt_depth == 0u) && (append_output (&state, "\n", 1u) != 0)) {
      free (state.color_stack);
      free (state.output);
      return (-1);
    }
  }

  free (state.color_stack);
  *dst = state.output;

  return (0);
}
