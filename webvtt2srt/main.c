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

// webvtt2srt - Convert a WebVTT file to SubRip (.srt) format.
//              Retain those markup features supported by SubRip format.

// Build: make

// Run without command line arguments to see usage notes.
// Output: out.srt

#include "webvtt2srt.h"

int
main (int argc, char *argv[]) {

  int result;
  FILE *fi, *fo;
  const char *output_name;

  output_name = "out.srt";

  // Exactly one WebVTT input filename is required on the command line.
  if (argc != 2) {
    fprintf (stdout, "\nUsage: ./webvtt2srt input.webvtt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  // Refuse the obvious case where opening out.srt would truncate the input.
  if (strcmp (argv[1], output_name) == 0) {
    fprintf (stderr, "Input and output files must be different.\n");
    return (EXIT_FAILURE);
  }

  // Open the WebVTT source in binary mode so readline() can handle line
  // endings explicitly and consistently on all platforms.
  fi = fopen (argv[1], "rb");
  if (fi == NULL) {
    fprintf (stderr, "Unable to open input file '%s': %s\n", argv[1], strerror (errno));
    return (EXIT_FAILURE);
  }

  // Create the SubRip output file in binary mode. The converter itself writes
  // the desired newline characters rather than relying on text translation.
  fo = fopen (output_name, "wb");
  if (fo == NULL) {
    fprintf (stderr, "Unable to open output file '%s': %s\n", output_name, strerror (errno));
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Perform the actual stream conversion before either file is closed.
  result = convert_file (fi, fo, argv[1]);

  // A close operation can itself report delayed I/O errors, so treat failure
  // to close either stream as a failed conversion.
  if (fclose (fi) == EOF) {
    fprintf (stderr, "Error closing input file '%s': %s\n", argv[1], strerror (errno));
    result = -1;
  }

  if (fclose (fo) == EOF) {
    fprintf (stderr, "Error closing output file '%s': %s\n", output_name, strerror (errno));
    result = -1;
  }

  // Do not leave a partial or otherwise unreliable SRT file after an error.
  if (result != 0) {
    remove (output_name);
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
