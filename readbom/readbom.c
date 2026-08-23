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

// readbom.c - Check for a Byte Order Mark (BOM) in a text file.

// gcc -Wall readbom.c -o readbom

// Run without command line arguments to see usage notes.
// Output: reports to stdout

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Definition of structs
typedef struct {
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);

// Byte Order Marks are at most four bytes long in the table below.
#define BOM_BUFFER_SIZE 4

int
main (int argc, char **argv) {

  int type;
  size_t nread;
  const char *filename;
  uint8_t input[BOM_BUFFER_SIZE] = {0};
  FILE *fi;

  // Byte Order Mark (BOM) names and sequences.
  static const uint8_t utf8[]       = {0xef, 0xbb, 0xbf};
  static const uint8_t utf16be[]    = {0xfe, 0xff};
  static const uint8_t utf16le[]    = {0xff, 0xfe};
  static const uint8_t utf32be[]    = {0x00, 0x00, 0xfe, 0xff};
  static const uint8_t utf32le[]    = {0xff, 0xfe, 0x00, 0x00};
  static const uint8_t utf7[]       = {0x2b, 0x2f, 0x76};
  static const uint8_t utf1[]       = {0xf7, 0x64, 0x4c};
  static const uint8_t utfebcdic[]  = {0xdd, 0x73, 0x66, 0x73};
  static const uint8_t scsu[]       = {0x0e, 0xfe, 0xff};
  static const uint8_t bocu1[]      = {0xfb, 0xee, 0x28};
  static const uint8_t gb18030[]    = {0x84, 0x31, 0x95, 0x33};

  static const BOM bom[] = {
    {sizeof (utf8),      "UTF-8",        utf8},
    {sizeof (utf16be),   "UTF-16 (BE)",  utf16be},
    {sizeof (utf16le),   "UTF-16 (LE)",  utf16le},
    {sizeof (utf32be),   "UTF-32 (BE)",  utf32be},
    {sizeof (utf32le),   "UTF-32 (LE)",  utf32le},
    {sizeof (utf7),      "UTF-7",        utf7},
    {sizeof (utf1),      "UTF-1",        utf1},
    {sizeof (utfebcdic), "UTF-EBCDIC",   utfebcdic},
    {sizeof (scsu),      "SCSU",         scsu},
    {sizeof (bocu1),     "BOCU-1",       bocu1},
    {sizeof (gb18030),   "GB18030",      gb18030}
  };
  const size_t nbom = sizeof (bom) / sizeof (bom[0]);

  // Process the command line arguments, if any.
  if (argc == 2) {
    filename = argv[1];
  } else {
    fprintf (stdout, "\nUsage: ./readbom inputfilename\n");
    fprintf (stdout, "       Output will be to stdout.\n\n");
    return (EXIT_SUCCESS);
  }

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open input file in binary mode so bytes are examined exactly as stored.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Read up to the maximum BOM length. A short file is valid input; it may
  // still contain a two- or three-byte BOM.
  nread = fread (input, sizeof (input[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to read input file %s.\n", filename);
    if (fclose (fi) != 0) {
      fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
    }
    return (EXIT_FAILURE);
  }

  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Detect a BOM at the beginning of the file.
  type = byteordermark (input, nread, bom, nbom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n\n", bom[type].name);
  }

  return (EXIT_SUCCESS);
}

// Detect a Byte Order Mark (BOM), if one exists at the beginning of the file.
// If more than one signature is a prefix of the input, return the longest
// matching signature. This prevents UTF-32 LE (ff fe 00 00), for example,
// from being mistaken for UTF-16 LE (ff fe).
// Return the index of the matching bom array entry, or -1 if none matches.
int
byteordermark (const uint8_t *text, size_t nbytes, const BOM *bom, size_t nbom) {

  size_t type, best_len;
  int best;

  if ((text == NULL) || (bom == NULL)) {
    return (-1);
  }

  best = -1;
  best_len = 0u;

  for (type=0u; type<nbom; type++) {

    // The file must contain the complete signature.
    if (bom[type].len > nbytes) {
      continue;
    }

    if ((bom[type].len > best_len) &&
        (memcmp (text, bom[type].sequence, bom[type].len) == 0)) {
      best = (int) type;
      best_len = bom[type].len;
    }
  }

  return (best);
}
