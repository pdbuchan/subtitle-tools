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

#include <iostream>
#include <string> 
#include <string.h>
#include "compact_enc_det.h"

// Function prototypes
char *allocate_strmem (int);
int *enc_name (int);

// Symbolic constants
#define TEXT_STRING_LEN 256  // Maximum number of characters in a line of text.

int
main (int argc, char **argv) {

  char *filein;
  char* text;
  bool is_reliable;
  int nbytes, n, i, bytes_consumed;
  FILE *fi;

  filein = allocate_strmem (TEXT_STRING_LEN);

  // Process the command line arguments, if any.
  if (argc == 2) {
    strncpy (filein, argv[1], TEXT_STRING_LEN);
  } else {
    fprintf (stdout, "\nUsage: ./ced inputfilename\n\n");
    exit (EXIT_SUCCESS);
  }

  // Open input file if it exists.
  fi = fopen (filein, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Input file %s does not exist.\n", filein);
    exit (EXIT_FAILURE);
  }

  // Count number of bytes in file.
  nbytes = 0;
  while ((n = fgetc (fi)) != EOF) {
    nbytes++;
  }
  rewind (fi);

  // Allocate memory for the file's contents.
  text = allocate_strmem (nbytes);

  // Read file contents.
  for (i=0; i<nbytes; i++) {
    text[i] = fgetc (fi);
  }

  // Close input file.
  fclose (fi);

  // Detemine the most probable character-encoding for the file.
  Encoding encoding = CompactEncDet::DetectEncoding(
    text, strlen(text),
    nullptr, nullptr, nullptr,
    UNKNOWN_ENCODING,
    UNKNOWN_LANGUAGE,
    CompactEncDet::QUERY_CORPUS,
    true,
    &bytes_consumed,
    &is_reliable);

  enc_name (encoding);
//  std::cout << encoding << "\n";

  printf ("bytes consumed: %i\n", bytes_consumed);
  printf ("Is reliable?: %s\n", is_reliable ? "true" : "false");

  // Free allocated memory.
  free (filein);
  free (text);

  return (EXIT_SUCCESS);
}

// Allocate memory for an array of chars.
char *
allocate_strmem (int len) {

  char *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmem().\n", len);
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
