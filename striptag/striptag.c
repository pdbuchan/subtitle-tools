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
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);
int searchandremove (char *, const char *);
static char *find_case_insensitive (char *, const char *);
static int starts_case_insensitive (const char *, const char *);
static int write_line (FILE *, const char *);
static void *allocate_mem (size_t, size_t, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters per physical line
#define BOM_BUFFER_SIZE 4  // Maximum number of bytes in a recognized BOM

int
main (int argc, char **argv) {

  int type, alllines, nlines, line, nsubs, sub, status, output_ok;
  int removeital, removebold, removeunderline, removestrikeout, removefont, removepos;
  size_t nread;
  char *temp, **input;
  const char *filename;
  uint8_t bom_input[BOM_BUFFER_SIZE] = {0};
  FILE *fi, *fo;

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
  const size_t nbom = sizeof (bom) / sizeof (bom[0]);

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

  // Open existing SubRip file in binary mode so BOM bytes are examined exactly.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input SubRip file %s.\n", filename);
    free (temp);
    return (EXIT_FAILURE);
  }

  // Read up to the maximum BOM length. A short file is valid input; it may
  // still contain a two- or three-byte BOM.
  nread = fread (bom_input, sizeof (bom_input[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
    fclose (fi);
    free (temp);
    return (EXIT_FAILURE);
  }

  // Detect a BOM at the beginning of the file. UTF-8 is accepted and skipped.
  // Other BOM-marked encodings require character decoding before the SRT syntax
  // can safely be parsed byte-by-byte, so reject them with an explicit message.
  type = byteordermark (bom_input, nread, bom, nbom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
    rewind (fi);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    if (type != 0) {
      fprintf (stderr, "ERROR: %s input is not supported by this byte-oriented SubRip parser.\n", bom[type].name);
      fprintf (stderr, "       Convert the file to UTF-8 first.\n");
      fclose (fi);
      free (temp);
      return (EXIT_FAILURE);
    }

    // Position the stream immediately after the UTF-8 BOM.
    if (fseek (fi, (long) bom[type].len, SEEK_SET) != 0) {
      fprintf (stderr, "ERROR: Unable to position input SubRip file %s after its BOM.\n", filename);
      fclose (fi);
      free (temp);
      return (EXIT_FAILURE);
    }
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

  // Return to the beginning of subtitle text. For UTF-8 BOM input, skip the
  // three BOM bytes again; otherwise return to byte zero.
  if (type == 0) {
    if (fseek (fi, (long) bom[type].len, SEEK_SET) != 0) {
      fprintf (stderr, "ERROR: Unable to position input SubRip file %s after its BOM.\n", filename);
      fclose (fi);
      free (temp);
      return (EXIT_FAILURE);
    }
  } else {
    rewind (fi);
  }

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
    if (fwrite (bom[type].sequence, sizeof (uint8_t), bom[type].len, fo) !=
        bom[type].len) {
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
