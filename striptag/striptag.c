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

// striptag.c - Read an existing SubRip (srt) file and remove markup tags.
//              Tags included: italics, bold, underline, strikethrough, font color, font size, position

// gcc -std=c11 -Wall -Wextra -Wpedantic striptag.c -o striptag

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Definition of structs
typedef struct {
  int len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (const char *, const BOM *);
int searchandremove (char *, const char *);
static char *ifind_case_insensitive (char *, const char *);
static int starts_case_insensitive (const char *, const char *);
static int write_line (FILE *, const char *);
char *allocate_strmem (int);
char **allocate_strmemp (int);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters per physical line
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int type, alllines, nlines, line, nsubs, sub, status, output_ok;
  int removeital, removebold, removeunderline, removestrikeout, removefont, removepos;
  char *temp, **input;
  const char *filename;
  FILE *fi, *fo;

  // Byte Order Mark (BOM) names and sequences.
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
  static const BOM bom[MAXBOM] = {
    {3, "UTF-8", utf8},
    {2, "UTF-16 (BE)", utf16be},
    {2, "UTF-16 (LE)", utf16le},
    {4, "UTF-32 (BE)", utf32be},
    {4, "UTF-32 (LE)", utf32le},
    {3, "UTF-7", utf7},
    {3, "UTF-1", utf1},
    {4, "UTF-EBCDIC", utfebcdic},
    {3, "SCSU", scsu},
    {3, "BOCU-1", bocu1},
    {4, "GB18030", gb18030}
  };

  // Set tag removal flags all to zero.
  removeital = 0;
  removebold = 0;
  removeunderline = 0;
  removestrikeout = 0;
  removefont = 0;
  removepos = 0;

  // Process the command line arguments, if any.
  if ((argc == 2) || (argc == 3)) {
    filename = argv[1];

    if (argc == 3) {
      if (strcmp (argv[2], "a") == 0) {
        removeital = 1;
        removebold = 1;
        removeunderline = 1;
        removestrikeout = 1;
        removefont = 1;
        removepos = 1;
      } else if (strcmp (argv[2], "i") == 0) {
        removeital = 1;
      } else if (strcmp (argv[2], "b") == 0) {
        removebold = 1;
      } else if (strcmp (argv[2], "u") == 0) {
        removeunderline = 1;
      } else if (strcmp (argv[2], "s") == 0) {
        removestrikeout = 1;
      } else if (strcmp (argv[2], "f") == 0) {
        removefont = 1;
      } else if (strcmp (argv[2], "p") == 0) {
        removepos = 1;
      } else {
        argc = 0;  // Force usage message below.
      }
    }
  } else {
    filename = NULL;
  }

  if ((argc != 2) && (argc != 3)) {
    fprintf (stdout, "\nUsage: ./striptag inputfilename.srt [option]\n\n");
    fprintf (stdout, "Options:\n");
    fprintf (stdout, "          a - remove all markup tags\n");
    fprintf (stdout, "          i - remove italics tags\n");
    fprintf (stdout, "          b - remove bold tags\n");
    fprintf (stdout, "          u - remove underline tags\n");
    fprintf (stdout, "          s - remove strikeout tags\n");
    fprintf (stdout, "          f - remove font color and font size tags\n");
    fprintf (stdout, "          p - remove position tags\n\n");
    fprintf (stdout, "Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  temp = allocate_strmem (MAXLEN);

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file.
  fi = fopen (filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input SubRip file %s.\n", filename);
    free (temp);
    return (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllines = 0;
  for (;;) {
    status = readline (fi, temp, MAXLEN);
    if (status == -1) break;
    if (status == -2) {
      fprintf (stderr, "\nERROR: Line %i does not fit in the %i-byte input buffer.\n", alllines + 1, MAXLEN);
      fclose (fi);
      free (temp);
      return (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "\nERROR: Unable to read input SubRip file %s.\n", filename);
      fclose (fi);
      free (temp);
      return (EXIT_FAILURE);
    }
    alllines++;
  }

  fprintf (stdout, "\n%i lines found including any excess trailing line-feeds.\n", alllines);

  if (alllines == 0) {
    fprintf (stderr, "ERROR: Input SubRip file %s is empty.\n", filename);
    fclose (fi);
    free (temp);
    return (EXIT_FAILURE);
  }

  rewind (fi);

  // Add one slot in case a missing final blank line must be supplied.
  input = allocate_strmemp (alllines + 1);
  for (line=0; line<(alllines + 1); line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0; line<alllines; line++) {
    status = readline (fi, input[line], MAXLEN);
    if (status == -1) {
      fprintf (stderr, "\nERROR: Unexpected EOF while reading line %i from %s.\n", line + 1, filename);
      fclose (fi);
      free (temp);
      for (line=0; line<(alllines + 1); line++) free (input[line]);
      free (input);
      return (EXIT_FAILURE);
    }
    if (status == -2) {
      fprintf (stderr, "\nERROR: Line %i does not fit in the %i-byte input buffer.\n", line + 1, MAXLEN);
      fclose (fi);
      free (temp);
      for (line=0; line<(alllines + 1); line++) free (input[line]);
      free (input);
      return (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "\nERROR: Unable to read line %i from input SubRip file %s.\n", line + 1, filename);
      fclose (fi);
      free (temp);
      for (line=0; line<(alllines + 1); line++) free (input[line]);
      free (input);
      return (EXIT_FAILURE);
    }
  }

  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    free (temp);
    for (line=0; line<(alllines + 1); line++) free (input[line]);
    free (input);
    return (EXIT_FAILURE);
  }

  free (temp);

  // Remove excess blank lines at end while retaining one final separator.
  nlines = alllines;
  while ((nlines > 1) && (input[nlines - 1][0] == '\n') && (input[nlines - 2][0] == '\n')) {
    nlines--;
  }

  // Add a missing final blank line so the last subtitle is safely terminated.
  if (input[nlines - 1][0] != '\n') {
    input[nlines][0] = '\n';
    input[nlines][1] = '\0';
    nlines++;
    fprintf (stdout, "WARNING: Final blank line after the last subtitle was missing but was corrected.\n");
  } else {
    fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);
  }

  // Detect any Byte Order Mark (BOM) at beginning of first line.
  type = byteordermark (input[0], bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n",
             bom[type].name);

    // This parser is byte-oriented. UTF-8 is compatible after removing its BOM,
    // but the other BOM-marked encodings require decoding first.
    if (type != 0) {
      fprintf (stderr, "ERROR: %s input is not supported by this byte-oriented SubRip parser.\n", bom[type].name);
      fprintf (stderr, "       Convert the file to UTF-8 first.\n");
      for (line=0; line<(alllines + 1); line++) free (input[line]);
      free (input);
      return (EXIT_FAILURE);
    }

    // Remove UTF-8 BOM from the first logical line before parsing. It will be
    // written explicitly to the output file later.
    memmove (input[0], &input[0][bom[0].len], strlen (&input[0][bom[0].len]) + 1u);
  }

  // Validate subtitle structure and count subtitles.
  nsubs = 0;
  line = 0;
  while (line < nlines) {
    if (input[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at input line %i.\n", line + 1);
      for (line=0; line<(alllines + 1); line++) free (input[line]);
      free (input);
      return (EXIT_FAILURE);
    }

    // Subtitle number line.
    line++;

    // Timestamp line must exist and may not be blank.
    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Subtitle %i is missing its timestamp line.\n", nsubs + 1);
      for (line=0; line<(alllines + 1); line++) free (input[line]);
      free (input);
      return (EXIT_FAILURE);
    }
    line++;

    // Subtitle text may contain zero or more lines.
    while ((line < nlines) && (input[line][0] != '\n')) line++;

    if (line >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %i is not closed by a blank line.\n", nsubs + 1);
      for (line=0; line<(alllines + 1); line++) free (input[line]);
      free (input);
      return (EXIT_FAILURE);
    }

    line++;  // Skip blank separator.
    nsubs++;
  }

  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Open output file without overwriting an existing file.
  errno = 0;
  fo = fopen ("out.srt", "wx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.srt.\n");
    }
    for (line=0; line<(alllines + 1); line++) free (input[line]);
    free (input);
    return (EXIT_FAILURE);
  }

  output_ok = 1;

  // Preserve a UTF-8 BOM when one was present in the input file.
  if (type == 0) {
    if (fwrite (bom[0].sequence, sizeof (uint8_t), (size_t) bom[0].len, fo) !=
        (size_t) bom[0].len) {
      output_ok = 0;
    }
  }

  // Loop through all subtitles.
  line = 0;
  for (sub=0; (sub<nsubs) && output_ok; sub++) {

    // Renumber subtitles.
    if (fprintf (fo, "%i\n", sub + 1) < 0) {
      output_ok = 0;
      break;
    }
    line++;  // Ignore original subtitle number.

    // Copy timestamp line.
    if (write_line (fo, input[line]) != EXIT_SUCCESS) {
      output_ok = 0;
      break;
    }
    line++;

    // Process and write all text lines of current subtitle.
    while ((line < nlines) && (input[line][0] != '\n')) {

      // Italics
      if (removeital) {
        searchandremove (input[line], "< i>");
        searchandremove (input[line], "<i >");
        searchandremove (input[line], "< i >");
        searchandremove (input[line], "<i>");
        searchandremove (input[line], "</ i>");
        searchandremove (input[line], "</i >");
        searchandremove (input[line], "< /i>");
        searchandremove (input[line], "< / i>");
        searchandremove (input[line], "< / i >");
        searchandremove (input[line], "</i>");
      }

      // Bold
      if (removebold) {
        searchandremove (input[line], "< b>");
        searchandremove (input[line], "<b >");
        searchandremove (input[line], "< b >");
        searchandremove (input[line], "<b>");
        searchandremove (input[line], "</ b>");
        searchandremove (input[line], "</b >");
        searchandremove (input[line], "< /b>");
        searchandremove (input[line], "< / b>");
        searchandremove (input[line], "< / b >");
        searchandremove (input[line], "</b>");
      }

      // Underline
      if (removeunderline) {
        searchandremove (input[line], "< u>");
        searchandremove (input[line], "<u >");
        searchandremove (input[line], "< u >");
        searchandremove (input[line], "<u>");
        searchandremove (input[line], "</ u>");
        searchandremove (input[line], "</u >");
        searchandremove (input[line], "< /u>");
        searchandremove (input[line], "< / u>");
        searchandremove (input[line], "< / u >");
        searchandremove (input[line], "</u>");
      }

      // Strikeout
      if (removestrikeout) {
        searchandremove (input[line], "< s>");
        searchandremove (input[line], "<s >");
        searchandremove (input[line], "< s >");
        searchandremove (input[line], "<s>");
        searchandremove (input[line], "</ s>");
        searchandremove (input[line], "</s >");
        searchandremove (input[line], "< /s>");
        searchandremove (input[line], "< / s>");
        searchandremove (input[line], "< / s >");
        searchandremove (input[line], "</s>");
      }

      // Font color and size
      if (removefont) {
        searchandremove (input[line], "< font color=");
        searchandremove (input[line], "<font color =");
        searchandremove (input[line], "<font color= ");
        searchandremove (input[line], "<font color = ");
        searchandremove (input[line], "< font color = ");
        searchandremove (input[line], "<fontcolor=");
        searchandremove (input[line], "<font color=");

        searchandremove (input[line], "< font size=");
        searchandremove (input[line], "<font size =");
        searchandremove (input[line], "<font size= ");
        searchandremove (input[line], "<font size = ");
        searchandremove (input[line], "< font size = ");
        searchandremove (input[line], "<fontsize=");
        searchandremove (input[line], "<font size=");

        searchandremove (input[line], "</ font>");
        searchandremove (input[line], "</font >");
        searchandremove (input[line], "< /font>");
        searchandremove (input[line], "< /font >");
        searchandremove (input[line], "</font>");
      }

      // Position
      if (removepos) {
        searchandremove (input[line], "{ \\an");
        searchandremove (input[line], "{\\ an");
        searchandremove (input[line], "{\\an ");
        searchandremove (input[line], "{ \\ an");
        searchandremove (input[line], "{\\an");
      }

      if (write_line (fo, input[line]) != EXIT_SUCCESS) {
        output_ok = 0;
        break;
      }
      line++;
    }

    // Write blank line between subtitles.
    if (output_ok && (fputc ('\n', fo) == EOF)) output_ok = 0;
    line++;  // Skip input blank separator.
  }

  if (ferror (fo)) output_ok = 0;
  if (fclose (fo) != 0) output_ok = 0;

  if (!output_ok) {
    fprintf (stderr, "ERROR: Unable to write complete output file out.srt.\n");
    remove ("out.srt");
  }

  for (line = 0; line < (alllines + 1); line++) free (input[line]);
  free (input);

  if (!output_ok) return (EXIT_FAILURE);

  fprintf (stdout, "%i subtitles written.\n\n", nsubs);

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
// Return index of bom array corresponding to type of BOM detected,
// or return -1 if none (or unlisted type) detected.
int
byteordermark (const char *text, const BOM *bom) {

  int type, i, found, best, bestlen;

  if ((text == NULL) || (bom == NULL)) return (-1);

  best = -1;
  bestlen = 0;

  // Keep the longest matching BOM because some shorter BOMs are prefixes of
  // longer ones (for example UTF-16 LE is a prefix of UTF-32 LE).
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

// Search a string for a substring and remove it.
// Matching is case-insensitive. For font and position opening tags, the entire
// tag through the corresponding closing delimiter is removed.
int
searchandremove (char *string, const char *sub) {

  size_t sublen;
  char *matchpos, *endpos, *scan;
  int variable_tag;

  if ((string == NULL) || (sub == NULL) || (*sub == '\0')) {
    return (EXIT_SUCCESS);
  }

  sublen = strlen (sub);
  scan = string;

  variable_tag = starts_case_insensitive (sub, "<font") ||
                 starts_case_insensitive (sub, "< font") ||
                 starts_case_insensitive (sub, "{\\an") ||
                 starts_case_insensitive (sub, "{ \\an") ||
                 starts_case_insensitive (sub, "{\\ an") ||
                 starts_case_insensitive (sub, "{ \\ an");

  while ((matchpos = find_case_insensitive (scan, sub)) != NULL) {

    // Opening font tags contain an attribute value, so remove through '>'.
    if (starts_case_insensitive (sub, "<font") || starts_case_insensitive (sub, "< font")) {
      endpos = strchr (matchpos, '>');
      if (endpos == NULL) {
        scan = matchpos + sublen;
        continue;
      }
      endpos++;

    // Position tags contain the alignment number, so remove through '}'.
    } else if (variable_tag) {
      endpos = strchr (matchpos, '}');
      if (endpos == NULL) {
        scan = matchpos + sublen;
        continue;
      }
      endpos++;

    // Fixed markup tag.
    } else {
      endpos = matchpos + sublen;
    }

    memmove (matchpos, endpos, strlen (endpos) + 1u);
    scan = matchpos;
  }

  return (EXIT_SUCCESS);
}

static char *
find_case_insensitive (char *string, const char *sub) {

  size_t i, j, stringlen, sublen;

  if ((string == NULL) || (sub == NULL)) return (NULL);

  stringlen = strlen (string);
  sublen = strlen (sub);
  if ((sublen == 0u) || (sublen > stringlen)) return (NULL);

  for (i=0; i<=(stringlen - sublen); i++) {
    for (j=0; j<sublen; j++) {
      if (tolower ((unsigned char) string[i + j]) != tolower ((unsigned char) sub[j])) {
        break;
      }
    }
    if (j == sublen) return (&string[i]);
  }

  return (NULL);
}

static int
starts_case_insensitive (const char *string, const char *prefix) {

  size_t i, prefixlen;

  if ((string == NULL) || (prefix == NULL)) return (0);

  prefixlen = strlen (prefix);
  for (i=0; i<prefixlen; i++) {
    if (string[i] == '\0') return (0);
    if (tolower ((unsigned char) string[i]) != tolower ((unsigned char) prefix[i])) {
      return (0);
    }
  }

  return (1);
}

static int
write_line (FILE *fo, const char *line) {

  size_t len;

  if ((fo == NULL) || (line == NULL)) return (EXIT_FAILURE);

  if (fputs (line, fo) == EOF) return (EXIT_FAILURE);

  len = strlen (line);
  if ((len == 0u) || (line[len - 1u] != '\n')) {
    if (fputc ('\n', fo) == EOF) return (EXIT_FAILURE);
  }

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
