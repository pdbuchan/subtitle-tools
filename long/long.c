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

// long.c - Read a SubRip (.srt) file and combine any two-line subtitle to a single line in order to have complete
//          sentences on a line. Really only designed for characters encountered in English and French.
//          If a UTF-8 Byte Order Mark (BOM) exists in the input file, it will be included in the output file.

// gcc -Wall long.c -o long

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

// Definition of structs
typedef struct {
  int len;
  char *name;
  uint8_t *sequence;
} BOM;

// Function prototypes
int is_french (const char *);
int readline (FILE *, char *, int);
int byteordermark (char *, BOM *);
static void *allocate_mem (size_t, size_t, const char *);
int *allocate_intmem (size_t);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
BOM *allocate_bommem (size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per physical input line
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, type, alllines, nlines, line, nsubs, sub, *ntext, len, val, status;
  size_t bomread;
  char *temp, *filename, **input;
  BOM *bom;
  FILE *fi, *fo;

  // Byte Order Mark (BOM) names and sequences.
  char name[MAXBOM][30] = {"UTF-8", "UTF-16 (BE)", "UTF-16 (LE)", "UTF-32 (BE)", "UTF-32 (LE)", "UTF-7", "UTF-1", "UTF-EBCDIC", "SCSU", "BOCU-1", "GB18030"};
  uint8_t utf8[3]       = {0xef, 0xbb, 0xbf};
  uint8_t utf16be[2]    = {0xfe, 0xff};
  uint8_t utf16le[2]    = {0xff, 0xfe};
  uint8_t utf32be[4]    = {0x00, 0x00, 0xfe, 0xff};
  uint8_t utf32le[4]    = {0xff, 0xfe, 0x00, 0x00};
  uint8_t utf7[3]       = {0x2b, 0x2f, 0x76};
  uint8_t utf1[3]       = {0xf7, 0x64, 0x4c};
  uint8_t utfebcdic[4]  = {0xdd, 0x73, 0x66, 0x73};
  uint8_t scsu[3]       = {0x0e, 0xfe, 0xff};
  uint8_t bocu1[3]      = {0xfb, 0xee, 0x28};
  uint8_t gb18030[4]    = {0x84, 0x31, 0x95, 0x33};

  // Allocate memory for various arrays.
  filename = allocate_strmem (MAXLEN);

  // Process the command line arguments, if any.
  if (argc == 2) {
    if (snprintf (filename, MAXLEN, "%s", argv[1]) >= MAXLEN) {
      fprintf (stderr, "ERROR: Input filename is too long.\n");
      free (filename);
      return (EXIT_FAILURE);
    }

  } else {
    fprintf (stdout, "\nUsage: ./long inputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    free (filename);
    return (EXIT_SUCCESS);
  }

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);
  bom = allocate_bommem (MAXBOM);

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

  // Open existing SubRip file in binary mode so BOM bytes are examined exactly.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Examine the beginning of the file for a BOM before parsing any SubRip
  // lines. Use enough bytes for the longest BOM in the table.
  memset (temp, 0, MAXLEN * sizeof (char));
  bomread = fread (temp, sizeof (char), 4u, fi);
  if ((bomread < 4u) && ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to examine input SubRip file %s for a Byte Order Mark.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  type = byteordermark (temp, bom);
  if (type < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", filename);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n", filename, bom[type].name);

    // This program parses SubRip syntax one byte at a time. UTF-8 is compatible
    // with that processing; the other BOM-marked encodings are not.
    if (type != 0) {
      fprintf (stderr, "ERROR: Character encoding %s is not supported by this byte-oriented SubRip parser.\n", bom[type].name);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }
  rewind (fi);

  // Count lines of input SubRip file, handling every readline() return value.
  alllines = 0;
  for (;;) {
    status = readline (fi, temp, MAXLEN);

    if (status == -1) break;

    if (status == -2) {
      fprintf (stderr, "ERROR: A line in input SubRip file %s does not fit in the %d-byte input buffer.\n", filename, MAXLEN);
      fclose (fi);
      exit (EXIT_FAILURE);
    }

    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }

    if (alllines == INT_MAX) {
      fprintf (stderr, "ERROR: Input SubRip file contains too many lines.\n");
      fclose (fi);
      exit (EXIT_FAILURE);
    }

    alllines++;
  }

  if (alllines == 0) {
    fprintf (stderr, "ERROR: Input SubRip file %s is empty.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "\n%s: %i lines found including any excess trailing line-feeds.\n", filename, alllines);
  rewind (fi);

  // Allocate memory for array to hold input file.
  input = allocate_strmemp ((size_t) alllines);
  for (line = 0; line < alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line = 0; line < alllines; line++) {
    status = readline (fi, input[line], MAXLEN);

    if (status == -1) {
      fprintf (stderr, "ERROR: Unexpected EOF while reading line %i from input SubRip file %s.\n", line + 1, filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }

    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in input SubRip file %s does not fit in the %d-byte input buffer.\n", line + 1, filename, MAXLEN);
      fclose (fi);
      exit (EXIT_FAILURE);
    }

    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read line %i from input SubRip file %s.\n", line + 1, filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }

  // Close input file.
  if (fclose (fi) == EOF) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Remove the UTF-8 BOM from the first subtitle-number line. It is written
  // explicitly to the output file below.
  if (type == 0) {
    memmove (input[0], &input[0][bom[type].len], strlen (&input[0][bom[type].len]) + 1u);
  }

  // Remove excess line-feeds at end of array input, retaining one blank line
  // to close the final subtitle.
  nlines = alllines;
  for (line = alllines; line > 1; line--) {
    if ((input[line - 1][0] == '\n') && (input[line - 2][0] == '\n')) {
      nlines--;
    } else {
      break;
    }
  }
  fprintf (stdout, "%s: %i lines found excluding excess trailing line-feeds.\n", filename, nlines);

  if ((nlines < 1) || (input[nlines - 1][0] != '\n')) {
    fprintf (stderr, "ERROR: Last subtitle is not closed by a blank line.\n");
    exit (EXIT_FAILURE);
  }

  // Count subtitles while validating enough SubRip structure to keep all
  // subsequent indexing within the input array.
  nsubs = 0;
  line = 0;
  while (line < nlines) {

    // A subtitle must begin with a non-blank subtitle-number line.
    if (input[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at input line %i.\n", line + 1);
      exit (EXIT_FAILURE);
    }

    if (nsubs == INT_MAX) {
      fprintf (stderr, "ERROR: Input SubRip file contains too many subtitles.\n");
      exit (EXIT_FAILURE);
    }
    nsubs++;

    // A timestamp line must follow the subtitle-number line.
    line++;
    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Subtitle %i is missing its timestamp line.\n", nsubs);
      exit (EXIT_FAILURE);
    }

    // Skip timestamp and text lines until the blank line closing the subtitle.
    line++;
    while ((line < nlines) && (input[line][0] != '\n')) {
      line++;
    }

    if (line >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %i is not closed by a blank line.\n", nsubs);
      exit (EXIT_FAILURE);
    }

    line++;  // Move past the closing blank line.
  }

  fprintf (stdout, "\n%i subtitles found in %s.\n\n", nsubs, filename);

  // Count the number of text lines in each validated subtitle.
  ntext = allocate_intmem ((size_t) nsubs);
  line = 0;
  for (sub = 0; sub < nsubs; sub++) {

    line += 2;  // Skip subtitle number and timestamp.
    ntext[sub] = 0;

    while ((line < nlines) && (input[line][0] != '\n')) {
      if (ntext[sub] == INT_MAX) {
        fprintf (stderr, "ERROR: Subtitle %i contains too many text lines.\n", sub + 1);
        exit (EXIT_FAILURE);
      }
      ntext[sub]++;
      line++;
    }

    line++;  // Skip the blank line closing this subtitle.
  }

  // Create the output file exclusively so an existing out.srt is never
  // overwritten between a separate existence check and creation.
  errno = 0;
  fo = fopen ("out.srt", "wx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.srt.\n");
    }
    exit (EXIT_FAILURE);
  }

  // Preserve a UTF-8 BOM if one was present in the input file.
  if (type == 0) {
    if (fwrite (bom[type].sequence, sizeof (uint8_t), (size_t) bom[type].len, fo) != (size_t) bom[type].len) {
      fprintf (stderr, "ERROR: Unable to write Byte Order Mark to out.srt.\n");
      fclose (fo);
      exit (EXIT_FAILURE);
    }
  }

  i = 0;  // Line index of SubRip file.

  // Loop through all subtitles.
  for (sub = 0; sub < nsubs; sub++) {

    // Renumber subtitles consecutively in the output file.
    if (fprintf (fo, "%i\n", sub + 1) < 0) {
      fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub + 1);
      fclose (fo);
      exit (EXIT_FAILURE);
    }
    i++;  // Skip original subtitle-number line.

    // Copy timestamp to output file.
    if (fputs (input[i], fo) == EOF) {
      fprintf (stderr, "ERROR: Unable to write timestamp for subtitle %i to out.srt.\n", sub + 1);
      fclose (fo);
      exit (EXIT_FAILURE);
    }
    i++;

    // Loop through all lines of text for the current subtitle.
    for (line = 0; line < ntext[sub]; line++) {

      len = (int) strlen (input[i]);
      if ((len < 2) || (input[i][len - 1] != '\n')) {
        fprintf (stderr, "ERROR: Text line in subtitle %i is not properly line-terminated.\n", sub + 1);
        fclose (fo);
        exit (EXIT_FAILURE);
      }

      val = (unsigned char) input[i][len - 2];

      // Only exactly two-line subtitles are candidates for joining, as stated
      // in the program description. Leave subtitles with one or 3+ text lines
      // otherwise unchanged.
      if ((line == 0) && (ntext[sub] == 2) &&
          ((val == ',') || (val == '>') ||
           ((val > 34) && (val < 44)) ||       // #, $, %, &, ', (, ), *, +
           ((val > 47) && (val < 60)) ||       // 0 - 9, :, ;
           ((val > 64) && (val < 92)) ||       // A - Z, [
           ((val > 96) && (val < 124)) ||      // a - z, {
           (val == ']'))) {

        // Replace the first line-feed with a space before writing line two.
        if (fwrite (input[i], sizeof (char), (size_t) (len - 1), fo) != (size_t) (len - 1) || fputc (' ', fo) == EOF) {
          fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub + 1);
          fclose (fo);
          exit (EXIT_FAILURE);
        }

      // French accented character at the end of the first line.
      } else if ((line == 0) && (ntext[sub] == 2) && is_french (input[i])) {

        if (fwrite (input[i], sizeof (char), (size_t) (len - 1), fo) != (size_t) (len - 1) || fputc (' ', fo) == EOF) {
          fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub + 1);
          fclose (fo);
          exit (EXIT_FAILURE);
        }

      // Bogus ellipsis case; fix it.
      } else if ((line == 0) && (strcmp (input[i], "---\n") == 0)) {
        if (fputs ("...\n", fo) == EOF) {
          fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub + 1);
          fclose (fo);
          exit (EXIT_FAILURE);
        }

      // A trailing hyphen in the first line of an exactly two-line subtitle
      // indicates a word split across the line break. Remove the hyphen and
      // line-feed and concatenate the second line without inserting a space.
      } else if ((line == 0) && (ntext[sub] == 2) && (val == '-')) {

        if ((len > 2) && (fwrite (input[i], sizeof (char), (size_t) (len - 2), fo) != (size_t) (len - 2))) {
          fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub + 1);
          fclose (fo);
          exit (EXIT_FAILURE);
        }

      } else {
        if (fputs (input[i], fo) == EOF) {
          fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub + 1);
          fclose (fo);
          exit (EXIT_FAILURE);
        }
      }

      i++;
    }

    // End-of-subtitle blank line.
    if (fputc ('\n', fo) == EOF) {
      fprintf (stderr, "ERROR: Unable to finish writing subtitle %i to out.srt.\n", sub + 1);
      fclose (fo);
      exit (EXIT_FAILURE);
    }
    i++;  // Skip input blank line.
  }

  if (fclose (fo) == EOF) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt after writing.\n");
    exit (EXIT_FAILURE);
  }

  // Free allocated memory.
  free (temp);
  free (bom);
  free (ntext);
  free (filename);
  for (line = 0; line < alllines; line++) {
    free (input[line]);
  }
  free (input);

  return (EXIT_SUCCESS);
}

// Check whether a line ends with one of the French accented characters handled
// by this program immediately before its retained line-feed.
// Return 0 if no match, 1 if a match.
int
is_french (const char *line) {

  size_t i, linelen, charlen;
  static const char *character[] = {
    "à", "é", "è", "ù", "â", "ê", "î", "ô", "û", "ë", "ï", "ü", "ç",
    "À", "É", "È", "Ù", "Â", "Ê", "Î", "Ô", "Û", "Ë", "Ï", "Ü", "Ç"
  };

  if (line == NULL) return (0);

  linelen = strlen (line);
  if ((linelen < 2u) || (line[linelen - 1u] != '\n')) return (0);

  for (i = 0u; i < (sizeof (character) / sizeof (character[0])); i++) {
    charlen = strlen (character[i]);
    if ((linelen >= charlen + 1u) && (memcmp (&line[linelen - charlen - 1u], character[i], charlen) == 0)) {
      return (1);
    }
  }

  return (0);
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
// Return the index of the longest matching BOM. This is important because,
// for example, the UTF-16LE signature is a prefix of the UTF-32LE signature.
// Return -1 if no listed BOM is detected.
int
byteordermark (char *text, BOM *bom) {

  int type, i, found, best, bestlen;

  if ((text == NULL) || (bom == NULL)) return (-1);

  best = -1;
  bestlen = 0;

  for (type = 0; type < MAXBOM; type++) {

    found = 1;
    for (i = 0; i < bom[type].len; i++) {
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

// Allocate memory for an array of ints.
int *
allocate_intmem (size_t len) {
  return (allocate_mem (len, sizeof (int), "array of ints"));
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

// Allocate memory for an array of BOM (Byte Order Mark) structs.
BOM *
allocate_bommem (size_t len) {
  return (allocate_mem (len, sizeof (BOM), "array of BOM (Byte Order Mark) structs"));
}
