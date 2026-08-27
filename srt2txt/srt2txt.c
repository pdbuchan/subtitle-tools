/*  Copyright (C) 2024-2026 P. David Buchan (pdbuchan@gmail.com)

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

// srt2txt.c - Read an existing SubRip (.srt) file and save only the text lines to an output file.

// gcc -Wall srt2txt.c -o srt2txt

// Usage: ./srt2txt inputfilename.srt [nospace]
// Output: out.txt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

// Definition of structs
typedef struct {
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);
static void *allocate_mem (size_t, size_t, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line

// Byte Order Marks are at most four bytes long in the table below.
#define BOM_BUFFER_SIZE 4

// Byte Order Mark (BOM) names and sequences.
static const uint8_t utf8[]       = {0xef, 0xbb, 0xbf};
static const uint8_t utf16be[]    = {0xfe, 0xff};
static const uint8_t utf16le[]    = {0xff, 0xfe};
static const uint8_t utf32be[]    = {0x00, 0x00, 0xfe, 0xff};
static const uint8_t utf32le[]    = {0xff, 0xfe, 0x00, 0x00};
static const uint8_t utf7_1[]     = {0x2b, 0x2f, 0x76, 0x38};
static const uint8_t utf7_2[]     = {0x2b, 0x2f, 0x76, 0x39};
static const uint8_t utf7_3[]     = {0x2b, 0x2f, 0x76, 0x2b};
static const uint8_t utf7_4[]     = {0x2b, 0x2f, 0x76, 0x2f};
static const uint8_t utf1[]       = {0xf7, 0x64, 0x4c};
static const uint8_t utfebcdic[]  = {0xdd, 0x73, 0x66, 0x73};
static const uint8_t scsu[]       = {0x0e, 0xfe, 0xff};
static const uint8_t bocu1[]      = {0xfb, 0xee, 0x28};
static const uint8_t gb18030[]    = {0x84, 0x31, 0x95, 0x33};

static const BOM boms[] = {
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

static const size_t nbom = sizeof (boms) / sizeof (boms[0]);

int
main (int argc, char **argv) {

  int type, status, alllines, nlines, line, nsubs, sub, nospace;
  size_t nread;
  const char *filename;
  char temp[MAXLEN], **input;
  uint8_t bom_buffer[BOM_BUFFER_SIZE] = {0};
  FILE *fi, *fo;

  // Process the command line arguments, if any.
  nospace = 0;  // Default to leaving a blank line between text of each subtitle.
  if (argc == 2) {
    filename = argv[1];

  } else if ((argc == 3) && (strcmp (argv[2], "nospace") == 0)) {
    filename = argv[1];
    nospace = 1;

  } else {
    fprintf (stdout, "\nUsage: ./srt2txt inputfilename.srt [nospace]\n");
    fprintf (stdout, "       Output filename will be out.txt.\n\n");
    return (EXIT_SUCCESS);
  }

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file in binary mode so BOM bytes are examined exactly.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Examine the beginning of the file for a BOM before parsing any SubRip
  // lines. A short file is valid input and may still contain a complete BOM.
  nread = fread (bom_buffer, sizeof (bom_buffer[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to examine input SubRip file %s for a Byte Order Mark.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Detect a BOM at the beginning of the file.
  type = byteordermark (bom_buffer, nread, boms, nbom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", boms[type].name);

    // This program parses SubRip syntax byte-by-byte. UTF-8 is compatible with
    // that processing; the other BOM-marked encodings require decoding first.
    if (type != 0) {
      fprintf (stderr, "ERROR: Character encoding %s is not supported by this byte-oriented SubRip parser.\n", boms[type].name);
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  // Position the stream at the first subtitle byte. Skip an accepted UTF-8 BOM;
  // otherwise return to the beginning of the file.
  if (fseek (fi, (type == 0) ? (long) boms[type].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to position input SubRip file %s after BOM detection.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllines = 0;
  for (;;) {
    status = readline (fi, temp, MAXLEN);
    if (status == 0) {
      alllines++;
    } else if (status == -1) {
      break;
    } else if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in input SubRip file %s does not fit in the %d-byte input buffer.\n", alllines + 1, filename, MAXLEN);
      fclose (fi);
      return (EXIT_FAILURE);
    } else {
      fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  if (alllines == 0) {
    fprintf (stderr, "ERROR: Input SubRip file %s is empty.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "\n%i lines found including any excess trailing line-feeds.\n", alllines);

  // Return to the first subtitle byte for the second input pass.
  if (fseek (fi, (type == 0) ? (long) boms[type].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to reposition input SubRip file %s.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Allocate memory for array to hold input file.
  input = allocate_strmemp (alllines);
  for (line=0; line<alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0; line<alllines; line++) {
    status = readline (fi, input[line], MAXLEN);
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in input SubRip file %s does not fit in the %d-byte input buffer.\n", line + 1, filename, MAXLEN);
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read line %i from input SubRip file %s.\n", line + 1, filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (status == -1) {
      fprintf (stderr, "ERROR: Unexpected end of input SubRip file %s while reading line %i.\n", filename, line + 1);
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  // Close input file.
  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Remove excess line-feeds at end of array input, retaining one closing blank line.
  nlines = alllines;
  for (line=alllines; line>1; line--) {
    if ((input[line - 1][0] == '\n') && (input[line - 2][0] == '\n')) {
      nlines--;
    } else {
      break;
    }
  }
  fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);

  // Validate the basic SubRip block structure and count subtitles.
  nsubs = 0;
  line = 0;
  while (line < nlines) {

    if (input[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at input line %i.\n", line + 1);
      return (EXIT_FAILURE);
    }

    // Each subtitle must contain a subtitle-number line and timestamp line.
    if ((line + 1) >= nlines) {
      fprintf (stderr, "ERROR: Subtitle beginning at input line %i has no timestamp line.\n", line + 1);
      return (EXIT_FAILURE);
    }

    line += 2;

    // Advance through subtitle text to the closing blank line.
    while ((line < nlines) && (input[line][0] != '\n')) {
      line++;
    }

    if (line >= nlines) {
      fprintf (stderr, "ERROR: Last subtitle is not closed by a blank line.\n");
      return (EXIT_FAILURE);
    }

    nsubs++;
    line++;  // Move past the closing blank line.
  }

  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Create output file without overwriting an existing file.
  errno = 0;
  fo = fopen ("out.txt", "wx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.txt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.txt.\n");
    }
    return (EXIT_FAILURE);
  }

  // Write UTF-8 Byte Order Mark (BOM) to output file if detected in input file.
  if (type == 0) {
    if (fwrite (boms[type].sequence, sizeof (uint8_t), boms[type].len, fo) != boms[type].len) {
      fprintf (stderr, "ERROR: Unable to write Byte Order Mark to output file out.txt.\n");
      fclose (fo);
      return (EXIT_FAILURE);
    }
  }

  // Write only the text lines of each subtitle.
  line = 0;
  for (sub=0; sub<nsubs; sub++) {

    line += 2;  // Skip subtitle number and timestamp.

    while (input[line][0] != '\n') {
      if (fputs (input[line], fo) == EOF) {
        fprintf (stderr, "ERROR: Unable to write subtitle text to output file out.txt.\n");
        fclose (fo);
        return (EXIT_FAILURE);
      }
      line++;
    }

    // Unless requested otherwise, separate subtitles by one blank line.
    if (!nospace) {
      if (fputc ('\n', fo) == EOF) {
        fprintf (stderr, "ERROR: Unable to write to output file out.txt.\n");
        fclose (fo);
        return (EXIT_FAILURE);
      }
    }

    line++;  // Move past the closing blank line.
  }

  // Close output file and detect any buffered write failure.
  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.txt.\n");
    return (EXIT_FAILURE);
  }

  // Free allocated memory.
  for (line=0; line<alllines; line++) {
    free (input[line]);
  }
  free (input);

  return (EXIT_SUCCESS);
}

// Read a single line of text from a subtitle/text file.
// The terminating line-feed is retained when one is present in the input.
// Carriage returns are discarded so LF and CRLF input are handled identically.
//
// Returns:
//   0  - line successfully read
//  -1  - EOF encountered before any characters were read
//  -2  - line is too long for the supplied buffer
//  -3  - invalid arguments or input error
int
readline (FILE *fi, char *line, int limit) {

  int ch, i;

  if ((fi == NULL) || (line == NULL) || (limit < 2)) {
    return (-3);
  }

  i = 0;
  for (;;) {

    ch = fgetc (fi);

    // End of file reached.
    if (ch == EOF) {

      // File stream error encountered.
      if (ferror (fi)) {
        line[0] = '\0';
        return (-3);
      }

      // No characters were read for this line.
      if (i == 0) {
        line[0] = '\0';
        return (-1);
      }

      // Accept a final line that does not end with a line-feed.
      line[i] = '\0';
      return (0);
    }

    // Ignore carriage returns so CRLF input is treated as LF input.
    if (ch == '\r') {
      continue;
    }

    // Found a line-feed. Retain it because the subtitle tools use a line
    // containing only '\n' to identify the blank line between subtitles.
    if (ch == '\n') {

      // Line too long for supplied buffer.
      if (i >= (limit - 1)) {
        line[limit - 1] = '\0';
        return (-2);
      }

      line[i++] = '\n';
      line[i] = '\0';
      return (0);
    }

    // Reserve one byte for the terminating null character. If the line is too
    // long, discard the rest of the physical line so the next call starts at
    // the beginning of the following line.
    if (i >= (limit - 1)) {
      line[limit - 1] = '\0';
      while ((ch = fgetc (fi)) != '\n' && ch != EOF) {
      }
      if ((ch == EOF) && ferror (fi)) {
        return (-3);
      }
      return (-2);
    }

    line[i++] = (char) ch;
  }
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

  for (type = 0u; type < nbom; type++) {

    // The file must contain the complete signature.
    if (bom[type].len > nbytes) {
      continue;
    }

    if ((bom[type].len > best_len) && (memcmp (text, bom[type].sequence, bom[type].len) == 0)) {
      best = (int) type;
      best_len = bom[type].len;
    }
  }

  return (best);
}

static void *
allocate_mem (size_t len, size_t item_size, const char *name) {

  void *tmp;

  if (len == 0 || item_size == 0 || len > (SIZE_MAX / item_size)) {
    fprintf (stderr, "Cannot allocate memory for %s: invalid size in allocate_mem().\n", name);
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, item_size);
  if (tmp == NULL) {
    fprintf (stderr, "Cannot allocate memory for %s in allocate_mem().\n", name);
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate memory for an array of chars (i.e., a character string).
char *
allocate_strmem (size_t len) {
  return (allocate_mem (len, sizeof (char), "array of chars"));
}

// Allocate memory for an array of pointers to arrays of chars.
char **
allocate_strmemp (size_t len) {
  return (allocate_mem (len, sizeof (char *), "array of pointers to arrays of chars"));
}
