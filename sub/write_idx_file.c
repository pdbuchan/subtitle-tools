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

// Create a revised .idx file with the same timestamp transformation used for
// the MPEG PES timestamps in out.sub.
int
write_idx_file (const char *idx_filename, OPTIONS *options) {

  char line[MAX_STRINGLEN];
  char timestamp[13];
  TIME time;
  int64_t adjusted_ms;
  FILE *fi, *fo;

  fi = fopen (idx_filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "Unable to open input .idx file %s.\n", idx_filename);
    return EXIT_FAILURE;
  }

  fo = fopen ("out.idx", "rb");
  if (fo != NULL) {
    fclose (fo);
    fclose (fi);
    fprintf (stderr, "Output file out.idx already exists.\n");
    return EXIT_FAILURE;
  }

  fo = fopen ("out.idx", "w");
  if (fo == NULL) {
    fclose (fi);
    fprintf (stderr, "Can't open output file out.idx.\n");
    return EXIT_FAILURE;
  }

  while (readline (fi, line, MAX_STRINGLEN) != -1) {
    if (strncmp (line, "timestamp:", 10) == 0) {
      const char *stamp_start = line + 10;
      size_t prefix_len;

      while (*stamp_start == ' ' || *stamp_start == '\t') stamp_start++;
      if (strlen (stamp_start) < 12) {
        fprintf (stderr, "Malformed timestamp line in IDX file: %s", line);
        fclose (fi);
        fclose (fo);
        return EXIT_FAILURE;
      }

      memcpy (timestamp, stamp_start, 12);
      timestamp[12] = '\0';
      if (parse_timestamp (timestamp, &time) != EXIT_SUCCESS ||
          transform_timestamp_ms (options, time.totalms, &adjusted_ms) != EXIT_SUCCESS) {
        fprintf (stderr, "Unable to transform IDX timestamp: %s", line);
        fclose (fi);
        fclose (fo);
        return EXIT_FAILURE;
      }

      time.totalms = adjusted_ms;
      if (mstotime (&time) != EXIT_SUCCESS) {
        fclose (fi);
        fclose (fo);
        return EXIT_FAILURE;
      }

      // IDX's conventional timestamp field has two decimal hour digits.
      if (time.h > 99) {
        fprintf (stderr, "Adjusted IDX timestamp exceeds 99 hours.\n");
        fclose (fi);
        fclose (fo);
        return EXIT_FAILURE;
      }

      prefix_len = (size_t) (stamp_start - line);
      if (fwrite (line, 1, prefix_len, fo) != prefix_len ||
          fprintf (fo, "%02d:%02d:%02d,%03d%s",
                   time.h, time.m, time.s, time.ms, stamp_start + 12) < 0) {
        fprintf (stderr, "I/O error while writing out.idx.\n");
        fclose (fi);
        fclose (fo);
        return EXIT_FAILURE;
      }
    } else {
      fputs (line, fo);
    }
  }

  if (ferror (fi) || ferror (fo)) {
    fprintf (stderr, "I/O error while creating out.idx.\n");
    fclose (fi);
    fclose (fo);
    return EXIT_FAILURE;
  }

  fclose (fi);
  if (fclose (fo) != 0) {
    fprintf (stderr, "Unable to finalize out.idx.\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
