/*  Copyright (C) 2025 P. David Buchan (pdbuchan@gmail.com)

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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compact_enc_det.h"

// Function prototypes
char *allocate_strmem (size_t);
int enc_name (Encoding);

int
main (int argc, char **argv) {

  char *text;
  bool is_reliable;
  int bytes_consumed, nbytes;
  long file_size;
  size_t nread;
  FILE *fi;

  // Process the command line arguments, if any.
  if (argc != 2) {
    fprintf (stdout, "\nUsage: ./ced inputfilename\n\n");
    return (EXIT_SUCCESS);
  }

  // Open input file if it exists.
  fi = fopen (argv[1], "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input file %s.\n", argv[1]);
    return (EXIT_FAILURE);
  }

  // Determine the size of the input file. DetectEncoding() takes the input
  // length as an int, so reject files that cannot be represented safely.
  if (fseek (fi, 0L, SEEK_END) != 0) {
    fprintf (stderr, "ERROR: Unable to seek to the end of input file %s.\n", argv[1]);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  file_size = ftell (fi);
  if (file_size < 0) {
    fprintf (stderr, "ERROR: Unable to determine the size of input file %s.\n", argv[1]);
    fclose (fi);
    return (EXIT_FAILURE);
  }
  if (file_size > INT_MAX) {
    fprintf (stderr, "ERROR: Input file %s is too large for CED.\n", argv[1]);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  nbytes = (int) file_size;
  rewind (fi);

  // Allocate one extra byte and leave it zero-filled. CED itself does not
  // require a terminating NUL because the explicit byte count is passed to
  // DetectEncoding(), but the extra byte makes the buffer safe to inspect as
  // a C string while debugging.
  text = allocate_strmem ((size_t) nbytes + 1);

  // Read the complete file, including any embedded NUL bytes.
  nread = fread (text, 1, (size_t) nbytes, fi);
  if (nread != (size_t) nbytes) {
    fprintf (stderr, "ERROR: Unable to read complete input file %s.\n", argv[1]);
    fclose (fi);
    free (text);
    return (EXIT_FAILURE);
  }

  // Close input file.
  fclose (fi);

  // Determine the most probable character encoding for the file. Pass the
  // actual byte count rather than strlen(), since CED is designed to examine
  // arbitrary raw bytes and an input file may contain embedded NUL bytes.
  Encoding encoding = CompactEncDet::DetectEncoding(
    text, nbytes,
    nullptr, nullptr, nullptr,
    UNKNOWN_ENCODING,
    UNKNOWN_LANGUAGE,
    CompactEncDet::QUERY_CORPUS,
    true,
    &bytes_consumed,
    &is_reliable);

  if (enc_name (encoding) != EXIT_SUCCESS) {
    free (text);
    return (EXIT_FAILURE);
  }

  printf ("bytes consumed: %i\n", bytes_consumed);
  printf ("Is reliable?: %s\n", is_reliable ? "true" : "false");

  // Free allocated memory.
  free (text);

  return (EXIT_SUCCESS);
}

// Allocate memory for an array of chars.
char *
allocate_strmem (size_t len) {

  char *tmp;

  if (len == 0) {
    fprintf (stderr, "ERROR: Cannot allocate zero bytes in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }

  tmp = (char *) malloc (len * sizeof (char));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (char));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }
}
