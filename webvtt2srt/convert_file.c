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

// Convert an open WebVTT stream to an open SubRip stream.
//
// The initial WEBVTT header and any following header metadata are consumed
// here. Subsequent blank-line-delimited blocks are collected and passed to
// process_block(), which decides whether each block is a cue or WebVTT
// metadata that should not appear in the SRT output.
int
convert_file (FILE *fi, FILE *fo, const char *input_name) {

  int status, in_header, result;
  char *line;
  unsigned long line_number, cue_number;
  BLOCK block;

  if ((fi == NULL) || (fo == NULL) || (input_name == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  // Begin with an empty block. line_number is retained solely for diagnostics,
  // while cue_number supplies the sequential numbering required by SRT.
  memset (&block, 0, sizeof (block));
  line_number = 0ul;
  cue_number = 0ul;
  in_header = 1;
  result = -1;

  // WebVTT requires the first logical line to contain the WEBVTT signature.
  // Any optional UTF-8 BOM has already been consumed before this function is
  // called, so the header parser sees only WebVTT text.
  status = readline (fi, &line);
  if (status < 0) {
    fprintf (stderr, "Error reading '%s': %s\n", input_name, strerror (errno));
    return (-1);
  }

  if (status == 0) {
    fprintf (stderr, "Input file '%s' is empty.\n", input_name);
    return (-1);
  }

  line_number++;

  if (!is_webvtt_header (line)) {
    fprintf (stderr, "'%s' does not begin with a valid WEBVTT header.\n", input_name);
    free (line);
    return (-1);
  }

  free (line);

  // The initial header extends through the first blank line. Header metadata
  // such as X-TIMESTAMP-MAP is not subtitle content and is discarded.
  while ((status = readline (fi, &line)) == 1) {
    line_number++;

    if (in_header) {
      if (line[0] == '\0') {
        in_header = 0;
      }

      free (line);
      continue;
    }

    // A blank line terminates the current cue or metadata block. Multiple
    // consecutive blank lines simply leave the block empty and are ignored.
    if (line[0] == '\0') {
      free (line);

      if (block.count == 0u) {
        continue;
      }

      if (process_block (fo, &block, &cue_number) != 0) {
        break;
      }

      free_block (&block);
      continue;
    }

    // Nonblank lines belong to the current block. append_line() assumes
    // ownership of line only when it succeeds.
    if (append_line (&block, line, line_number) != 0) {
      fprintf (stderr, "Out of memory while reading '%s'.\n", input_name);
      free (line);
      break;
    }
  }

  // A negative readline() result indicates an input error. If EOF arrives
  // after a nonblank line, process the final block because the WebVTT file
  // need not end with an additional blank separator.
  if (status < 0) {
    fprintf (stderr, "Error reading '%s'.\nError message: %s\n", input_name, strerror (errno));
  } else if ((status == 0) && !in_header && (block.count != 0u)) {
    if (process_block (fo, &block, &cue_number) == 0) {
      result = 0;
    }
  } else if (status == 0) {
    // EOF with no pending block is a normal successful completion.
    result = 0;
  }

  // Force buffered output to the underlying stream now so write errors can be
  // reported by the converter rather than being deferred until fclose().
  if ((result == 0) && (fflush (fo) == EOF)) {
    fprintf (stderr, "Error writing 'out.srt'.\nError message: %s\n", strerror (errno));
    result = -1;
  } else if ((result != 0) && ferror (fo)) {
    fprintf (stderr, "Error writing 'out.srt'.\nError message: %s\n", strerror (errno));
  }

  // Release any pending block whether conversion succeeded or failed.
  free_block (&block);

  return (result);
}
