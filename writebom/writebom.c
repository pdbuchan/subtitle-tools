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

// writebom.c - Prepend a Byte Order Mark (BOM) to a text file.

// gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2 writebom.c -o writebom

// Run without command line arguments to see usage notes.
// Output: out.txt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

// Set some symbolic constants.
#define MAX_STRINGLEN 256  // Maximum number of characters read from standard input
#define BOM_BUFFER_SIZE 4  // Maximum number of bytes in a recognized BOM
#define COPY_BUFSIZE 8192  // Buffer size used when copying the input file

// Definition of structs.
typedef struct {
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes.
int inputtext (char *);
int parse_choice (const char *, int *, size_t);
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);
int copy_stream (FILE *, FILE *);

// Byte Order Mark (BOM) sequences.
static const uint8_t utf8[]       = {0xef, 0xbb, 0xbf};
static const uint8_t utf16be[]    = {0xfe, 0xff};
static const uint8_t utf16le[]    = {0xff, 0xfe};
static const uint8_t utf32be[]    = {0x00, 0x00, 0xfe, 0xff};
static const uint8_t utf32le[]    = {0xff, 0xfe, 0x00, 0x00};
static const uint8_t utf7_1[]     = {0x2b, 0x2f, 0x76, 0x38};  // +/v8
static const uint8_t utf7_2[]     = {0x2b, 0x2f, 0x76, 0x39};  // +/v9
static const uint8_t utf7_3[]     = {0x2b, 0x2f, 0x76, 0x2b};  // +/v+
static const uint8_t utf7_4[]     = {0x2b, 0x2f, 0x76, 0x2f};  // +/v/
static const uint8_t utf1[]       = {0xf7, 0x64, 0x4c};
static const uint8_t utfebcdic[]  = {0xdd, 0x73, 0x66, 0x73};
static const uint8_t scsu[]       = {0x0e, 0xfe, 0xff};
static const uint8_t bocu1[]      = {0xfb, 0xee, 0x28};
static const uint8_t gb18030[]    = {0x84, 0x31, 0x95, 0x33};

// Table of recognized Byte Order Marks.
static const BOM bom[] = {
  {sizeof (utf8),      "UTF-8",        utf8},
  {sizeof (utf16be),   "UTF-16 (BE)",  utf16be},
  {sizeof (utf16le),   "UTF-16 (LE)",  utf16le},
  {sizeof (utf32be),   "UTF-32 (BE)",  utf32be},
  {sizeof (utf32le),   "UTF-32 (LE)",  utf32le},
  {sizeof (utf7_1),    "UTF-7",        utf7_1},
  {sizeof (utf7_2),    "UTF-7",        utf7_2},
  {sizeof (utf7_3),    "UTF-7",        utf7_3},
  {sizeof (utf7_4),    "UTF-7",        utf7_4},
  {sizeof (utf1),      "UTF-1",        utf1},
  {sizeof (utfebcdic), "UTF-EBCDIC",   utfebcdic},
  {sizeof (scsu),      "SCSU",         scsu},
  {sizeof (bocu1),     "BOCU-1",       bocu1},
  {sizeof (gb18030),   "GB18030",      gb18030}
};

static const size_t nbom = sizeof (bom) / sizeof (bom[0]);

int
main (int argc, char **argv) {

  int type, choice, status;
  char temp[MAX_STRINGLEN];
  uint8_t input[BOM_BUFFER_SIZE];
  size_t i, nread;
  const char *filename;
  FILE *fi, *fo;

  // Process the command line arguments, if any.
  if (argc == 2) {
    filename = argv[1];
  } else {
    fprintf (stdout, "\nUsage: ./writebom inputfilename\n");
    fprintf (stdout, "       Output will be out.txt\n\n");
    return (EXIT_SUCCESS);
  }

  // Ask which BOM should be prepended if the input does not already have one.
  fprintf (stdout, "\nChoose Byte Order Mark (BOM) to apply to file %s:\n\n", filename);
  for (i=0u; i<nbom; i++) {
    fprintf (stdout, "%zu = %s", i + 1u, bom[i].name);

    // UTF-7 has four distinct BOM signatures. Show the actual signature so the
    // user can select the intended form rather than seeing four identical rows.
    if ((i >= 5u) && (i <= 8u)) {
      fprintf (stdout, " (%c%c%c%c)", bom[i].sequence[0], bom[i].sequence[1],
               bom[i].sequence[2], bom[i].sequence[3]);
    }

    fputc ('\n', stdout);
  }

  fprintf (stdout, "\nChoice? ");
  memset (temp, 0, sizeof (temp));
  inputtext (temp);
  if (parse_choice (temp, &choice, nbom) != EXIT_SUCCESS) {
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open the input file in binary mode so its bytes are copied unchanged.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Read only enough bytes to identify any supported BOM. Short and empty
  // input files are valid; fread() simply returns the number of bytes present.
  nread = fread (input, sizeof (uint8_t), sizeof (input), fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to read input file %s.\n", filename);
    if (fclose (fi) != 0) {
      fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
    }
    return (EXIT_FAILURE);
  }

  // Detect an existing BOM. The detector chooses the longest complete match,
  // which prevents UTF-32LE from being mistaken for its UTF-16LE prefix.
  type = byteordermark (input, nread, bom, nbom);
  if (type >= 0) {
    fprintf (stdout, "\nExisting Byte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);
    fprintf (stdout, "No action taken.\n\n");

    if (fclose (fi) != 0) {
      fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
      return (EXIT_FAILURE);
    }
    return (EXIT_SUCCESS);
  }

  fprintf (stdout, "\nNo known existing Byte Order Mark (BOM) found in %s.\n\n", filename);

  // Create the output file exclusively so an existing out.txt is never
  // overwritten between a separate existence check and the create operation.
  errno = 0;
  fo = fopen ("out.txt", "wbx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.txt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.txt.\n");
    }

    if (fclose (fi) != 0) {
      fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
    }
    return (EXIT_FAILURE);
  }

  status = EXIT_SUCCESS;

  // Write the selected BOM.
  if (fwrite (bom[choice - 1].sequence, sizeof (uint8_t), bom[choice - 1].len, fo) != bom[choice - 1].len) {
    fprintf (stderr, "ERROR: Unable to write Byte Order Mark to out.txt.\n");
    status = EXIT_FAILURE;
  }

  // The first bytes of the input file have already been read for BOM
  // detection, so write those bytes before copying the remainder of the file.
  if ((status == EXIT_SUCCESS) && (nread > 0u)) {
    if (fwrite (input, sizeof (uint8_t), nread, fo) != nread) {
      fprintf (stderr, "ERROR: Unable to write input data to out.txt.\n");
      status = EXIT_FAILURE;
    }
  }

  // Copy the rest of the input file in blocks.
  if ((status == EXIT_SUCCESS) && (copy_stream (fi, fo) != EXIT_SUCCESS)) {
    status = EXIT_FAILURE;
  }

  // Close both streams and regard a close failure as an I/O failure.
  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
    status = EXIT_FAILURE;
  }

  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.txt.\n");
    status = EXIT_FAILURE;
  }

  // Do not leave a partial output file after an I/O failure.
  if (status != EXIT_SUCCESS) {
    if (remove ("out.txt") != 0) {
      fprintf (stderr, "WARNING: Unable to remove incomplete output file out.txt.\n");
    }
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Obtain a text string from standard input. It can include spaces.
int
inputtext (char *text) {

  int ch;
  size_t len;

  if (text == NULL) {
    fprintf (stderr, "ERROR: Invalid text buffer supplied to inputtext().\n");
    exit (EXIT_FAILURE);
  }

  if (fgets (text, MAX_STRINGLEN, stdin) == NULL) {
    fprintf (stderr, "ERROR: Unable to read text from standard input.\n");
    exit (EXIT_FAILURE);
  }

  len = strlen (text);

  // Remove trailing newline, and a preceding carriage return if present.
  if ((len > 0u) && (text[len - 1u] == '\n')) {
    text[--len] = '\0';
    if ((len > 0u) && (text[len - 1u] == '\r')) {
      text[--len] = '\0';
    }
    return (EXIT_SUCCESS);
  }

  // If the buffer is full, determine whether the input was exactly
  // MAX_STRINGLEN - 1 characters or was genuinely too long.
  if (len == (MAX_STRINGLEN - 1u)) {

    ch = getchar ();

    // Exactly MAX_STRINGLEN - 1 characters followed by newline or EOF.
    if ((ch == '\n') || (ch == EOF)) {
      return (EXIT_SUCCESS);
    }

    // Handle CRLF after an exactly full input line.
    if (ch == '\r') {
      ch = getchar ();
      if ((ch == '\n') || (ch == EOF)) {
        return (EXIT_SUCCESS);
      }
    }

    // Discard the remainder of an overlong input line.
    while ((ch != '\n') && (ch != EOF)) {
      ch = getchar ();
    }

    fprintf (stderr, "ERROR: Input text is too long; maximum is %d characters.\n", MAX_STRINGLEN - 1);
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Parse a menu choice. Leading and trailing white space is allowed, but other
// trailing characters are rejected.
int
parse_choice (const char *text, int *choice, size_t nchoices) {

  char *endptr;
  long value;

  if ((text == NULL) || (choice == NULL) || (nchoices == 0u)) {
    fprintf (stderr, "ERROR: Invalid argument supplied to parse_choice().\n");
    return (EXIT_FAILURE);
  }

  errno = 0;
  value = strtol (text, &endptr, 10);
  if ((errno == ERANGE) || (endptr == text)) {
    fprintf (stderr, "ERROR: Cannot make integer of: %s\n", text);
    return (EXIT_FAILURE);
  }

  while ((*endptr != '\0') && isspace ((unsigned char) *endptr)) {
    endptr++;
  }

  if (*endptr != '\0') {
    fprintf (stderr, "ERROR: Invalid choice: %s\n", text);
    return (EXIT_FAILURE);
  }

  if ((value < 1L) || ((size_t) value > nchoices)) {
    fprintf (stderr, "ERROR: Choice must be between 1 and %zu.\n", nchoices);
    return (EXIT_FAILURE);
  }

  *choice = (int) value;
  return (EXIT_SUCCESS);
}

// Detect a Byte Order Mark (BOM), if one exists at the beginning of the file.
// If more than one signature is a prefix of the input, return the longest
// matching signature. This prevents UTF-32 LE (ff fe 00 00), for example,
// from being mistaken for UTF-16 LE (ff fe).
// Return the index of the matching bom array entry, or -1 if none matches.
int
byteordermark (const uint8_t *text, size_t nbytes, const BOM *table, size_t ntypes) {

  size_t type, best_len;
  int best;

  if ((text == NULL) || (table == NULL)) {
    return (-1);
  }

  best = -1;
  best_len = 0u;

  for (type=0u; type<ntypes; type++) {

    // The file must contain the complete signature.
    if (table[type].len > nbytes) {
      continue;
    }

    if ((table[type].len > best_len) &&
        (memcmp (text, table[type].sequence, table[type].len) == 0)) {
      best = (int) type;
      best_len = table[type].len;
    }
  }

  return (best);
}

// Copy all remaining bytes from the input stream to the output stream.
int
copy_stream (FILE *fi, FILE *fo) {

  uint8_t buffer[COPY_BUFSIZE];
  size_t nread;

  if ((fi == NULL) || (fo == NULL)) {
    fprintf (stderr, "ERROR: Invalid file stream supplied to copy_stream().\n");
    return (EXIT_FAILURE);
  }

  while ((nread = fread (buffer, sizeof (uint8_t), sizeof (buffer), fi)) > 0u) {
    if (fwrite (buffer, sizeof (uint8_t), nread, fo) != nread) {
      fprintf (stderr, "ERROR: Unable to write input data to out.txt.\n");
      return (EXIT_FAILURE);
    }
  }

  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to read input file while copying.\n");
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
