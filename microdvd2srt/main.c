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

// Open the named input file and run the MicroDVD-to-SRT converter.
//
// The video frame rate is obtained interactively because MicroDVD timestamps
// are frame numbers rather than absolute times. The converted subtitles are
// written to out.srt. A partially written output file is removed if conversion
// or file closing fails.
int
main (int argc, char *argv[]) {

  int result, type;
  size_t nread;
  uint8_t bom_buffer[BOM_BUFFER_SIZE] = {0};
  long double frame_rate;
  FILE *fi, *fo;
  const char *output_name;

  output_name = "out.srt";

  // Exactly one MicroDVD input filename is required on the command line.
  if (argc != 2) {
    fprintf (stderr, "Usage: %s input.sub\n", argv[0]);
    return (EXIT_FAILURE);
  }

  // Refuse the obvious case where opening out.srt would truncate the input.
  if (strcmp (argv[1], output_name) == 0) {
    fprintf (stderr, "Input and output files must be different.\n");
    return (EXIT_FAILURE);
  }

  // Open the MicroDVD source in binary mode so readline() can handle line
  // endings explicitly and consistently on all platforms.
  fi = fopen (argv[1], "rb");
  if (fi == NULL) {
    fprintf (stderr, "Unable to open input file '%s': %s\n", argv[1], strerror (errno));
    return (EXIT_FAILURE);
  }

  // Inspect the raw first bytes before any line-oriented parsing. UTF-8 is
  // byte-compatible with this parser after its BOM is skipped. Other known
  // BOM-marked encodings require transcoding, which this converter does not do.
  nread = fread (bom_buffer, sizeof (bom_buffer[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "Unable to inspect input file '%s' for a BOM: %s\n", argv[1], strerror (errno));
    fclose (fi);
    return (EXIT_FAILURE);
  }

  type = byteordermark (bom_buffer, nread, bom, nbom);
  if (type < 0) {
    fprintf (stdout, "No known Byte Order Mark (BOM) found in %s.\n", argv[1]);
  } else {
    fprintf (stdout, "Byte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    if (type != 0) {
      fprintf (stderr, "Character encoding %s is not supported by this byte-oriented MicroDVD parser.\n", bom[type].name);
      fprintf (stderr, "Convert the input file to UTF-8 before running microdvd2srt.\n");
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  // Begin parsing after an accepted UTF-8 BOM, or at byte zero when no known
  // BOM was present. scan_defaults() preserves this position between passes.
  if (fseek (fi, (type == 0) ? (long) bom[type].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "Unable to position input file '%s': %s\n", argv[1], strerror (errno));
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Ask for the frame rate before creating the output file. An invalid or
  // missing value therefore cannot leave behind an empty out.srt file.
  if (get_frame_rate (&frame_rate) != 0) {
    fclose (fi);
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
  result = convert_file (fi, fo, argv[1], frame_rate);

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
