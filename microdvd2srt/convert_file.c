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

// Convert an open MicroDVD stream to an open SubRip stream.
//
// A preliminary pass gathers {DEFAULT} formatting because MicroDVD permits
// that metadata anywhere in the file. The second pass converts each ordinary
// {start}{end} subtitle record using the frame rate supplied by the user.
int
convert_file (FILE *fi, FILE *fo, const char *input_name, long double frame_rate) {

  int status, is_default, result, first_frame_record;
  char start_time[64], end_time[64];
  char *line, *converted;
  const char *text, *payload;
  unsigned long line_number, cue_number;
  unsigned long long start_frame, end_frame;
  long double declared_rate;
  FORMAT_STATE defaults, ignored_defaults;

  if ((fi == NULL) || (fo == NULL) || (input_name == NULL) || (frame_rate <= 0.0l) || !isfinite (frame_rate)) {
    errno = EINVAL;
    return (-1);
  }

  // Resolve file-wide formatting before processing cues so a {DEFAULT} line
  // appearing near the end of the source can still affect earlier subtitles.
  if (scan_defaults (fi, input_name, &defaults) != 0) {
    return (-1);
  }

  line_number = 0ul;
  cue_number = 0ul;
  memset (&ignored_defaults, 0, sizeof (ignored_defaults));
  first_frame_record = 1;
  result = 0;

  while ((status = readline (fi, &line)) == 1) {

    line_number++;
    text = line;

    // Empty physical lines and the optional MicroDVD Player boundary markers
    // carry no subtitle content.
    if ((text[0] == '\0') || (strcmp (text, "[BEGIN]") == 0) || (strcmp (text, "[END]") == 0)) {
      free (line);
      continue;
    }

    // {DEFAULT} was already applied during the first pass and is never emitted
    // as a visible SubRip cue.
    if (parse_default_line (text, &ignored_defaults, &is_default) != 0) {
      fprintf (stderr, "Invalid MicroDVD DEFAULT line at line %lu: %s\n", line_number, text);
      free (line);
      result = -1;
      break;
    }

    if (is_default) {
      free (line);
      continue;
    }

    if (parse_subtitle_line (text, &start_frame, &end_frame, &payload) != 0) {
      fprintf (stderr, "Invalid MicroDVD subtitle line at line %lu: %s\n", line_number, text);
      free (line);
      result = -1;
      break;
    }

    // Many MicroDVD files begin with {1}{1}FPS. The user-supplied value remains
    // authoritative as requested, but this conventional information record is
    // recognized and omitted from the subtitle output.
    if (first_frame_record && (start_frame == 1ull) && (end_frame == 1ull) &&
        (parse_frame_rate (payload, &declared_rate) == 0) &&
        (declared_rate >= 3.0L) && (declared_rate <= 100.0L)) {

      if (fabsl (declared_rate - frame_rate) > 0.000001L) {
        fprintf (stderr, "Note: input declares %.9Lg fps; using entered rate %.9Lg fps.\n", declared_rate, frame_rate);
      }

      first_frame_record = 0;
      free (line);
      continue;
    }

    first_frame_record = 0;

    if (end_frame < start_frame) {
      fprintf (stderr, "Ending frame precedes starting frame at line %lu: %s\n", line_number, text);
      free (line);
      result = -1;
      break;
    }

    // Convert both frame positions independently. MicroDVD convention treats
    // frame/fps as the corresponding subtitle time, so no extra frame is added
    // to the ending position.
    if ((frame_to_timestamp (start_frame, frame_rate, start_time, sizeof (start_time)) != 0) ||
        (frame_to_timestamp (end_frame, frame_rate, end_time, sizeof (end_time)) != 0)) {
      fprintf (stderr, "Unable to convert frame numbers at line %lu: %s\n", line_number, text);
      free (line);
      result = -1;
      break;
    }

    converted = NULL;
    if (convert_text (payload, &defaults, &converted) != 0) {
      fprintf (stderr, "Unable to convert subtitle text at line %lu: %s\n", line_number, text);
      free (line);
      result = -1;
      break;
    }

    cue_number++;

    if (fprintf (fo, "%lu\n%s --> %s\n%s\n\n", cue_number, start_time, end_time, converted) < 0) {
      fprintf (stderr, "Error writing 'out.srt'.\nError message: %s\n", strerror (errno));
      free (converted);
      free (line);
      result = -1;
      break;
    }

    free (converted);
    free (line);
  }

  if (status < 0) {
    fprintf (stderr, "Error reading '%s'.\nError message: %s\n", input_name, strerror (errno));
    result = -1;
  }

  // Force buffered output to the underlying stream now so write errors can be
  // reported by the converter rather than being deferred until fclose().
  if ((result == 0) && (fflush (fo) == EOF)) {
    fprintf (stderr, "Error writing 'out.srt'.\nError message: %s\n", strerror (errno));
    result = -1;
  }

  return (result);
}
