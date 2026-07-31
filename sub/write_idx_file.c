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

// Create a revised .idx file with offset applied or resynchronized timestamps.
// Applies offsets/resyncronization to all timestamps for all languages.
int
write_idx_file (const char*idx_filename, OPTIONS *options) {

  int i;
  char *line, *timestamp;
  double ratio;
  TIME time;
  FILE *fi, *fo;

  // Allocate memory for various arrays.
  line = allocate_strmem (MAX_STRINGLEN);
  timestamp = allocate_strmem (MAX_STRINGLEN);

  // Open existing .idx file.
  fi = fopen (idx_filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "\nUnable to open input .idx file %s.\n", idx_filename);
    exit (EXIT_FAILURE);
  }

  // Open output file for revised .idx file.
  fo = fopen ("out.idx", "r");
  if (fo != NULL) {
    fprintf (stderr, "Output file out.idx already exists.\n");
    exit (EXIT_FAILURE);
  }
  fo = fopen ("out.idx", "w");
  if (fo == NULL) {
    fprintf (stderr, "Can't open output file out.idx.\n");
    exit (EXIT_FAILURE);
  }

  // Loop through all lines of existing .idx file.
  while (readline (fi, line, MAX_STRINGLEN) != -1) {

    // Found a timestamp offset line.
    if (strncmp (line, "timestamp:", 10) == 0) {
      for (i = 11; i < 23; i++) {
        timestamp[i - 11] = line[i];
      }
      parse_timestamp (timestamp, &time);  // Parse timestamp string into TIME struct elements.

      // Apply offset to timestamp, if requested.
      if (options->offset_flag) {
        time.totalms += options->offset.totalms;
        mstotime (&time);  // Update hh, mm, ss, ms based on totalms.
        fprintf (fo, "timestamp: %02d:%02d:%02d,%03d", time.h, time.m, time.s, time.ms);
        fprintf (fo, "%s", line + 23);  // Copy rest of existing line.

      // Synchronize timestamp, if requested.
      } else if (options->sync_flag) {

        ratio = (((double) options->newlastms - (double) options->newfirstms) / ((double) options->oldlastms - (double) options->oldfirstms));

        // Scale timestamp.
        time.totalms = (uint64_t) (((double) options->newfirstms) + ((((double) time.totalms) - ((double) options->oldfirstms)) * ratio));
        mstotime (&time);  // Update hh, mm, ss, ms based on totalms.
        fprintf (fo, "timestamp: %02d:%02d:%02d,%03d", time.h, time.m, time.s, time.ms);
        fprintf (fo, "%s", line + 23);  // Copy rest of existing line.
      }

    // Copy entire existing line.
    } else {
      fprintf (fo, "%s", line);
    }  // End if timestamp keyword found

    memset (line, 0, MAX_STRINGLEN * sizeof (char));

  }  // Next line of existing .idx file.

  // Close input and output files.
  fclose (fi);
  fclose (fo);

  // Free allocated memory.
  free (line);
  free (timestamp);

  return (EXIT_SUCCESS);
}
