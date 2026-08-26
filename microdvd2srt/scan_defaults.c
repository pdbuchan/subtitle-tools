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

// Scan the complete MicroDVD file for {DEFAULT} formatting metadata.
//
// A separate first pass is used because MicroDVD permits {DEFAULT} to appear
// anywhere in the file, while its settings apply to the complete subtitle
// file. The input stream is rewound before returning successfully.
int
scan_defaults (FILE *fi, const char *input_name, FORMAT_STATE *defaults) {

  int status, is_default;
  char *line;
  const char *text;
  unsigned long line_number;
  long input_start;

  if ((fi == NULL) || (input_name == NULL) || (defaults == NULL)) {
    errno = EINVAL;
    return (-1);
  }

  // The caller may have positioned the stream after a UTF-8 BOM. Remember that
  // logical beginning so the second conversion pass returns there rather than
  // to physical byte zero.
  input_start = ftell (fi);
  if (input_start < 0L) {
    fprintf (stderr, "Unable to determine input position for '%s': %s\n", input_name, strerror (errno));
    return (-1);
  }

  memset (defaults, 0, sizeof (*defaults));
  line_number = 0ul;

  while ((status = readline (fi, &line)) == 1) {
    line_number++;
    text = line;

    if (parse_default_line (text, defaults, &is_default) != 0) {
      fprintf (stderr, "Invalid MicroDVD DEFAULT line at line %lu: %s\n", line_number, text);
      free (line);
      return (-1);
    }

    free (line);
  }

  if (status < 0) {
    fprintf (stderr, "Error reading '%s'.\nError message: %s\n", input_name, strerror (errno));
    return (-1);
  }

  // Return to the logical beginning for the actual conversion pass. This may
  // be byte zero or the first byte following an accepted UTF-8 BOM. fseek()
  // also clears the stream's end-of-file indicator when successful.
  if (fseek (fi, input_start, SEEK_SET) != 0) {
    fprintf (stderr, "Unable to rewind input file '%s': %s\n", input_name, strerror (errno));
    return (-1);
  }

  return (0);
}
