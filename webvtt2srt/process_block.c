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

// Process one blank-line-delimited WebVTT block.
//
// NOTE, STYLE, REGION, and repeated WEBVTT header blocks are metadata rather
// than subtitle cues and are ignored. A cue may begin with its timing line or
// with a single cue identifier followed by the timing line. Valid cues are
// renumbered sequentially for SubRip output and their cue-text markup is
// translated to the subset that can be represented usefully in SRT.
int
process_block (FILE *fo, const BLOCK *block, unsigned long *cue_number) {

  size_t timing_index;
  char converted_timing[160];
  char *converted_text;

  // An empty block has no work to perform. NULL arguments are also ignored
  // here because convert_file() owns the higher-level error handling.
  if ((fo == NULL) || (block == NULL) || (cue_number == NULL) || (block->count == 0u)) {
    return (0);
  }

  // These WebVTT block types carry metadata or styling information rather
  // than subtitle text. Repeated WEBVTT blocks occur in concatenated media
  // segments and may also contain X-TIMESTAMP-MAP metadata.
  if (is_webvtt_header (block->line[0].text) ||
      starts_keyword (block->line[0].text, "NOTE") ||
      (strcmp (block->line[0].text, "STYLE") == 0) ||
      (strcmp (block->line[0].text, "REGION") == 0)) {
    return (0);
  }

  // A cue can begin directly with its timing line. If the first line is not a
  // timing line, WebVTT allows one optional cue identifier immediately before
  // it, so check the second line as well.
  if (strstr (block->line[0].text, "-->") != NULL) {
    timing_index = 0u;
  } else if ((block->count >= 2u) && (strstr (block->line[1].text, "-->") != NULL)) {
    timing_index = 1u;
  } else {
    fprintf (stderr, "Invalid WebVTT block beginning at line %lu.\n", block->line[0].number);
    return (-1);
  }

  // Translate the WebVTT timestamp syntax and discard any cue settings that
  // follow the ending timestamp.
  if (convert_timing_line (block->line[timing_index].text, converted_timing,
                           sizeof (converted_timing)) != 0) {
    fprintf (stderr, "Invalid WebVTT timing line at line %lu: %s\n",
             block->line[timing_index].number, block->line[timing_index].text);
    return (-1);
  }

  // Convert all payload lines together rather than independently. This lets
  // formatting state remain correct when a WebVTT tag opens on one line and
  // closes on a later line in the same cue.
  converted_text = NULL;
  if (convert_cue_text (block, timing_index + 1u, &converted_text) != 0) {
    fprintf (stderr, "Unable to convert cue text beginning at line %lu: %s\n",
             block->line[timing_index].number, strerror (errno));
    return (-1);
  }

  // SRT cue numbers are generated sequentially rather than preserving a
  // possibly absent or non-numeric WebVTT cue identifier.
  (*cue_number)++;

  if (fprintf (fo, "%lu\n%s\n%s\n", *cue_number, converted_timing, converted_text) < 0) {
    free (converted_text);
    return (-1);
  }

  free (converted_text);

  // SubRip cues are separated from one another by a blank line. The preceding
  // fprintf() supplied the cue text's terminating newline; this supplies the
  // additional empty separator line required between cues.
  if (fputc ('\n', fo) == EOF) {
    return (-1);
  }

  return (0);
}
