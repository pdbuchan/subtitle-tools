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
  int len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (const char *, const BOM *);
char *allocate_strmem (int);
char **allocate_strmemp (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int type, status, alllines, nlines, line, nsubs, sub, nospace;
  const char *filename;
  char temp[MAXLEN], **input;
  FILE *fi, *fo;

  // Byte Order Mark (BOM) names and sequences.
  static const char *const name[MAXBOM] = {"UTF-8", "UTF-16 (BE)", "UTF-16 (LE)", "UTF-32 (BE)", "UTF-32 (LE)", "UTF-7", "UTF-1", "UTF-EBCDIC", "SCSU", "BOCU-1", "GB18030"};
  static const uint8_t utf8[3]       = {0xef, 0xbb, 0xbf};
  static const uint8_t utf16be[2]    = {0xfe, 0xff};
  static const uint8_t utf16le[2]    = {0xff, 0xfe};
  static const uint8_t utf32be[4]    = {0x00, 0x00, 0xfe, 0xff};
  static const uint8_t utf32le[4]    = {0xff, 0xfe, 0x00, 0x00};
  static const uint8_t utf7[3]       = {0x2b, 0x2f, 0x76};
  static const uint8_t utf1[3]       = {0xf7, 0x64, 0x4c};
  static const uint8_t utfebcdic[4]  = {0xdd, 0x73, 0x66, 0x73};
  static const uint8_t scsu[3]       = {0x0e, 0xfe, 0xff};
  static const uint8_t bocu1[3]      = {0xfb, 0xee, 0x28};
  static const uint8_t gb18030[4]    = {0x84, 0x31, 0x95, 0x33};
  BOM bom[MAXBOM];

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

  // Populate array with Byte Order Mark data.
  bom[0].len = 3;    bom[0].name = name[0];    bom[0].sequence = utf8;
  bom[1].len = 2;    bom[1].name = name[1];    bom[1].sequence = utf16be;
  bom[2].len = 2;    bom[2].name = name[2];    bom[2].sequence = utf16le;
  bom[3].len = 4;    bom[3].name = name[3];    bom[3].sequence = utf32be;
  bom[4].len = 4;    bom[4].name = name[4];    bom[4].sequence = utf32le;
  bom[5].len = 3;    bom[5].name = name[5];    bom[5].sequence = utf7;
  bom[6].len = 3;    bom[6].name = name[6];    bom[6].sequence = utf1;
  bom[7].len = 4;    bom[7].name = name[7];    bom[7].sequence = utfebcdic;
  bom[8].len = 3;    bom[8].name = name[8];    bom[8].sequence = scsu;
  bom[9].len = 3;    bom[9].name = name[9];    bom[9].sequence = bocu1;
  bom[10].len = 4;   bom[10].name = name[10];  bom[10].sequence = gb18030;

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file.
  fi = fopen (filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
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

  if (fseek (fi, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind input SubRip file %s.\n", filename);
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

  // Detect any Byte Order Mark (BOM) at beginning of first line.
  type = byteordermark (input[0], bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    // This program parses SubRip syntax as single-byte/UTF-8 text. Other
    // BOM-marked encodings must be converted before they can be processed.
    if (type != 0) {
      fprintf (stderr, "ERROR: Character encoding %s is not supported by this byte-oriented SubRip parser.\n", bom[type].name);
      return (EXIT_FAILURE);
    }
  }

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
    if (fwrite (bom[type].sequence, sizeof (uint8_t), (size_t) bom[type].len, fo) != (size_t) bom[type].len) {
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

// Detect Byte Order Mark (BOM), if it exists, at beginning of line.
// Return index of bom array corresponding to type of longest matching BOM,
// or return -1 if none (or an unlisted type) is detected.
int
byteordermark (const char *text, const BOM *bom) {

  int type, i, found, best, bestlen;

  if ((text == NULL) || (bom == NULL)) return (-1);

  best = -1;
  bestlen = -1;

  // Test every BOM so a shorter prefix (for example UTF-16 LE) cannot hide a
  // longer BOM (for example UTF-32 LE).
  for (type=0; type<MAXBOM; type++) {

    found = 1;
    for (i=0; i<bom[type].len; i++) {
      if ((uint8_t) text[i] != bom[type].sequence[i]) {
        found = 0;
        break;
      }
    }

    if (found && (bom[type].len > bestlen)) {
      best = type;
      bestlen = bom[type].len;
    }
  }

  return (best);
}

// Allocate memory for an array of chars.
char *
allocate_strmem (int len) {

  char *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate memory for an array of pointers to arrays of chars.
char **
allocate_strmemp (int len) {

  char **tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmemp().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmemp().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}
