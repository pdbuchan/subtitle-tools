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

// fixtag.c - Read an existing SubRip (srt) file and look for and fix some common markup tag errors.
//            Tags included: italics, bold, underline, strikethrough, font color, font size, position

//            Note: Media players read font tags as nested. For example:

//            1
//            00:00:01,000 --> 00:00:49,000
//            <font color="yellow"><font color="blue"><font color="red">THIS IS RED </font>THIS IS BLUE </font>THIS IS YELLOW</font>

// gcc -Wall fixtag.c -o fixtag

// Run without command line arguments to see usage notes.
// Output: out.srt

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <limits.h>

// Definition of structs
typedef struct {
  int len;
  char *name;
  uint8_t *sequence;
} BOM;

typedef enum {
  TAG_ITALIC,
  TAG_BOLD,
  TAG_UNDERLINE,
  TAG_STRIKEOUT,
  TAG_FONT
} TAGTYPE;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (char *, BOM *);
int searchandreplace (char **, const char *, const char *);
void append_string (char **, const char *);
void append_missing_closures (char **, int);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
BOM *allocate_bommem (size_t);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of bytes in one physical input line
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, type, alllines, index, nlines, line, nsubs, sub, ntext, status, closeoption;
  size_t len;
  char *temp, *filename, **input, **text;
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
  temp = allocate_strmem (MAXLEN);
  bom = allocate_bommem (MAXBOM);

  // Process command-line arguments.
  closeoption = 0;
  if ((argc == 2) || ((argc == 3) && (strcmp (argv[2], "close") == 0))) {
    if (strlen (argv[1]) >= MAXLEN) {
      fprintf (stderr, "ERROR: Input filename is too long.\n");
      exit (EXIT_FAILURE);
    }
    snprintf (filename, MAXLEN, "%s", argv[1]);
    if (argc == 3) closeoption = 1;
  } else {
    fprintf (stdout, "\nUsage: ./fixtag inputfilename.srt [close]\n\n");
    fprintf (stdout, "       close option: Append any missing markup closure tags to last line of subtitle text.\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    free (filename);
    free (temp);
    free (bom);
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
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Examine the beginning of the file for a BOM before parsing any SubRip
  // structure. Four bytes are enough for the longest BOM in the table.
  memset (temp, 0, MAXLEN * sizeof (char));
  len = fread (temp, sizeof (char), 4u, fi);
  if ((len < 4u) && ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to examine input SubRip file %s for a Byte Order Mark.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  type = byteordermark (temp, bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    // This program parses and rewrites SubRip syntax one byte at a time.
    // UTF-8 is compatible with that processing; the other BOM-marked
    // encodings are not.
    if (type != 0) {
      fprintf (stderr, "ERROR: Character encoding %s is not supported by this byte-oriented SubRip parser.\n", bom[type].name);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }
  rewind (fi);

  // Count physical lines, handling every readline() return value.
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

  fprintf (stdout, "\n%i lines found including any excess trailing line-feeds.\n", alllines);
  rewind (fi);

  // Allocate memory for the input file, plus one line in case a missing final
  // blank separator must be supplied.
  input = allocate_strmemp ((size_t) alllines + 1u);
  for (line = 0; line <= alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into memory.
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

  if (fclose (fi) == EOF) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Remove excess blank lines at the end, retaining one separator.
  nlines = alllines;
  while ((nlines > 1) && (input[nlines - 1][0] == '\n') && (input[nlines - 2][0] == '\n')) {
    nlines--;
  }

  // Ensure the final subtitle is closed by a blank line. readline() accepts a
  // final physical line without LF, so do not modify that line itself.
  if (input[nlines - 1][0] != '\n') {
    input[nlines][0] = '\n';
    input[nlines][1] = '\0';
    nlines++;
    fprintf (stdout, "WARNING: Final closing line-feed for last subtitle was missing but was corrected.\n");
  } else {
    fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);
  }

  // Validate the basic SRT block structure and count subtitles. A subtitle may
  // contain zero or more text lines, but it must have a number line, timestamp
  // line, and terminating blank line.
  nsubs = 0;
  line = 0;
  while (line < nlines) {

    if (input[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at input line %i.\n", line + 1);
      exit (EXIT_FAILURE);
    }

    if ((line + 1 >= nlines) || (input[line + 1][0] == '\n')) {
      fprintf (stderr, "ERROR: Subtitle beginning at input line %i has no timestamp line.\n", line + 1);
      exit (EXIT_FAILURE);
    }

    line += 2;
    while ((line < nlines) && (input[line][0] != '\n')) line++;

    if (line >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %i is not terminated by a blank line.\n", nsubs + 1);
      exit (EXIT_FAILURE);
    }

    nsubs++;
    line++;  // Skip the blank separator.
  }

  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Open output file without overwriting an existing file.
  errno = 0;
  fo = fopen ("out.srt", "wbx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.srt.\n");
    }
    exit (EXIT_FAILURE);
  }

  // Preserve a UTF-8 BOM if one was present in the input.
  if (type == 0) {
    if (fwrite (bom[type].sequence, sizeof (uint8_t), (size_t) bom[type].len, fo) != (size_t) bom[type].len) {
      fprintf (stderr, "ERROR: Unable to write Byte Order Mark to out.srt.\n");
      fclose (fo);
      exit (EXIT_FAILURE);
    }
  }

  // Rewrite all subtitles, repairing common markup mistakes.
  index = 0;
  for (sub = 0; sub < nsubs; sub++) {

    // Replace the original subtitle number with a consecutive number.
    if (fprintf (fo, "%i\n", sub + 1) < 0) {
      fprintf (stderr, "ERROR: Unable to write subtitle number to out.srt.\n");
      fclose (fo);
      exit (EXIT_FAILURE);
    }
    index++;  // Skip original subtitle number.

    // Write the timestamp line with exactly one terminating LF.
    len = strlen (input[index]);
    if ((len > 0u) && (input[index][len - 1u] == '\n')) len--;
    if ((fwrite (input[index], sizeof (char), len, fo) != len) || (fputc ('\n', fo) == EOF)) {
      fprintf (stderr, "ERROR: Unable to write timestamp for subtitle %i.\n", sub + 1);
      fclose (fo);
      exit (EXIT_FAILURE);
    }
    index++;

    // Determine how many text lines belong to this subtitle.
    ntext = 0;
    while (input[index + ntext][0] != '\n') ntext++;

    text = NULL;
    if (ntext > 0) {
      text = allocate_strmemp ((size_t) ntext);

      for (i = 0; i < ntext; i++) {
        len = strlen (input[index + i]);
        if ((len > 0u) && (input[index + i][len - 1u] == '\n')) len--;

        text[i] = allocate_strmem (len + 1u);
        if (len > 0u) memcpy (text[i], input[index + i], len);
        text[i][len] = '\0';
      }
    }

    index += ntext + 1;  // Move past text and the blank separator.

    // Loop through all lines of text of current subtitle.
    // Search-and-replace malformed markup tags.
    for (line = 0; line < ntext; line++) {

      // Italics - malformed
      searchandreplace (&text[line], "< i>", "<i>");
      searchandreplace (&text[line], "<i >", "<i>");
      searchandreplace (&text[line], "< i >", "<i>");
      searchandreplace (&text[line], "<i>", "<i>");  // Fix <I>

      searchandreplace (&text[line], "</ i>", "</i>");
      searchandreplace (&text[line], "</i >", "</i>");
      searchandreplace (&text[line], "< /i>", "</i>");
      searchandreplace (&text[line], "< / i>", "</i>");
      searchandreplace (&text[line], "< / i >", "</i>");
      searchandreplace (&text[line], "</i>", "</i>");  // Fix </I>

      // Bold - malformed
      searchandreplace (&text[line], "< b>", "<b>");
      searchandreplace (&text[line], "<b >", "<b>");
      searchandreplace (&text[line], "< b >", "<b>");
      searchandreplace (&text[line], "<b>", "<b>");  // Fix <B>

      searchandreplace (&text[line], "</ b>", "</b>");
      searchandreplace (&text[line], "</b >", "</b>");
      searchandreplace (&text[line], "< /b>", "</b>");
      searchandreplace (&text[line], "< / b>", "</b>");
      searchandreplace (&text[line], "< / b >", "</b>");
      searchandreplace (&text[line], "</b>", "</b>");  // Fix </B>

      // Underline - malformed
      searchandreplace (&text[line], "< u>", "<u>");
      searchandreplace (&text[line], "<u >", "<u>");
      searchandreplace (&text[line], "< u >", "<u>");
      searchandreplace (&text[line], "<u>", "<u>");  // Fix <U>

      searchandreplace (&text[line], "</ u>", "</u>");
      searchandreplace (&text[line], "</u >", "</u>");
      searchandreplace (&text[line], "< /u>", "</u>");
      searchandreplace (&text[line], "< / u>", "</u>");
      searchandreplace (&text[line], "< / u >", "</u>");
      searchandreplace (&text[line], "</u>", "</u>");  // Fix </U>

      // Strikeout - malformed
      searchandreplace (&text[line], "< s>", "<s>");
      searchandreplace (&text[line], "<s >", "<s>");
      searchandreplace (&text[line], "< s >", "<s>");
      searchandreplace (&text[line], "<s>", "<s>");  // Fix <S>

      searchandreplace (&text[line], "</ s>", "</s>");
      searchandreplace (&text[line], "</s >", "</s>");
      searchandreplace (&text[line], "< /s>", "</s>");
      searchandreplace (&text[line], "< / s>", "</s>");
      searchandreplace (&text[line], "< / s >", "</s>");
      searchandreplace (&text[line], "</s>", "</s>");  // Fix </S>

      // Font color - malformed
      searchandreplace (&text[line], "< font color=", "<font color=");
      searchandreplace (&text[line], "<font color =", "<font color=");
      searchandreplace (&text[line], "<font color= ", "<font color=");
      searchandreplace (&text[line], "<font color = ", "<font color=");
      searchandreplace (&text[line], "< font color = ", "<font color=");
      searchandreplace (&text[line], "<fontcolor=", "<font color=");
      searchandreplace (&text[line], "<font color=", "<font color=");  // Fix uppercase mistakes.

      // Font size - malformed
      searchandreplace (&text[line], "< font size=", "<font size=");
      searchandreplace (&text[line], "<font size =", "<font size=");
      searchandreplace (&text[line], "<font size= ", "<font size=");
      searchandreplace (&text[line], "<font size = ", "<font size=");
      searchandreplace (&text[line], "< font size = ", "<font size=");
      searchandreplace (&text[line], "<fontsize=", "<font size=");
      searchandreplace (&text[line], "<font size=", "<font size=");  // Fix uppercase mistakes.

      // Closing font - malformed
      searchandreplace (&text[line], "</ font>", "</font>");
      searchandreplace (&text[line], "</font >", "</font>");
      searchandreplace (&text[line], "< /font>", "</font>");
      searchandreplace (&text[line], "< /font >", "</font>");
      searchandreplace (&text[line], "</font>", "</font>");  // Fix uppercase mistakes.

      // Position - malformed
      searchandreplace (&text[line], "{ \\an", "{\\an");
      searchandreplace (&text[line], "{\\ an", "{\\an");
      searchandreplace (&text[line], "{\\an ", "{\\an");
      searchandreplace (&text[line], "{ \\ an", "{\\an");
      searchandreplace (&text[line], "{ \\ an ", "{\\an");
      searchandreplace (&text[line], "{\\an", "{\\an");  // Fix uppercase mistakes.

      searchandreplace (&text[line], "1 }", "1}");  // Bottom-left
      searchandreplace (&text[line], "2 }", "2}");  // Bottom-center
      searchandreplace (&text[line], "3 }", "3}");  // Bottom-right
      searchandreplace (&text[line], "4 }", "4}");  // Middle-left
      searchandreplace (&text[line], "5 }", "5}");  // Middle-center
      searchandreplace (&text[line], "6 }", "6}");  // Middle-right
      searchandreplace (&text[line], "7 }", "7}");  // Top-left
      searchandreplace (&text[line], "8 }", "8}");  // Top-center
      searchandreplace (&text[line], "9 }", "9}");  // Top-right

    }  // Next line of current subtitle

    // If requested, append any missing closing tags in reverse opening order
    // so newly added closures remain properly nested.
    if (closeoption) append_missing_closures (text, ntext);

    // Write corrected subtitle text and terminating blank line.
    for (i = 0; i < ntext; i++) {
      if ((fputs (text[i], fo) == EOF) || (fputc ('\n', fo) == EOF)) {
        fprintf (stderr, "ERROR: Unable to write text for subtitle %i.\n", sub + 1);
        fclose (fo);
        exit (EXIT_FAILURE);
      }
      free (text[i]);
    }
    free (text);

    if (fputc ('\n', fo) == EOF) {
      fprintf (stderr, "ERROR: Unable to write subtitle separator to out.srt.\n");
      fclose (fo);
      exit (EXIT_FAILURE);
    }
  }

  if (fclose (fo) == EOF) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt.\n");
    exit (EXIT_FAILURE);
  }

  // Free allocated memory.
  free (temp);
  free (bom);
  free (filename);
  for (line = 0; line <= alllines; line++) {
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

// Detect Byte Order Mark (BOM), if it exists, at beginning of a byte buffer.
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

// Search a string case-insensitively for oldsub and replace all occurrences
// with newsub. The string is dynamically resized if a replacement is longer.
int
searchandreplace (char **string, const char *oldsub, const char *newsub) {

  const char *currentpos, *matchpos;
  char *result, *out;
  size_t oldlen, newlen, sourcelen, matches, resultlen, before;

  if ((string == NULL) || (*string == NULL) ||
      (oldsub == NULL) || (*oldsub == '\0') || (newsub == NULL)) {
    return (EXIT_FAILURE);
  }

  oldlen = strlen (oldsub);
  newlen = strlen (newsub);
  sourcelen = strlen (*string);

  // Count non-overlapping matches in the original source string.
  matches = 0u;
  currentpos = *string;
  while ((matchpos = strcasestr (currentpos, oldsub)) != NULL) {
    matches++;
    currentpos = matchpos + oldlen;
  }

  if (matches == 0u) return (EXIT_SUCCESS);

  if (newlen >= oldlen) {
    size_t growth = newlen - oldlen;
    if ((growth != 0u) && (matches > (SIZE_MAX - sourcelen - 1u) / growth)) {
      fprintf (stderr, "ERROR: String size overflow in searchandreplace().\n");
      exit (EXIT_FAILURE);
    }
    resultlen = sourcelen + (matches * growth);
  } else {
    resultlen = sourcelen - (matches * (oldlen - newlen));
  }

  result = allocate_strmem (resultlen + 1u);
  out = result;
  currentpos = *string;

  while ((matchpos = strcasestr (currentpos, oldsub)) != NULL) {
    before = (size_t) (matchpos - currentpos);
    if (before > 0u) {
      memcpy (out, currentpos, before);
      out += before;
    }
    if (newlen > 0u) {
      memcpy (out, newsub, newlen);
      out += newlen;
    }
    currentpos = matchpos + oldlen;
  }

  strcpy (out, currentpos);

  free (*string);
  *string = result;
  return (EXIT_SUCCESS);
}

// Remove the most recent unmatched opening tag of the requested type.
static void
close_tag (TAGTYPE *stack, size_t *nstack, TAGTYPE tag) {

  size_t i;

  if ((stack == NULL) || (nstack == NULL) || (*nstack == 0u)) return;

  i = *nstack;
  while (i > 0u) {
    i--;
    if (stack[i] == tag) {
      if (i + 1u < *nstack) {
        memmove (&stack[i], &stack[i + 1u], (*nstack - i - 1u) * sizeof (*stack));
      }
      (*nstack)--;
      return;
    }
  }
}

// Add an unmatched opening tag to the stack.
static void
open_tag (TAGTYPE **stack, size_t *nstack, size_t *capacity, TAGTYPE tag) {

  TAGTYPE *tmp;
  size_t newcapacity;

  if ((stack == NULL) || (nstack == NULL) || (capacity == NULL)) {
    fprintf (stderr, "ERROR: Invalid tag-stack state.\n");
    exit (EXIT_FAILURE);
  }

  if (*nstack == *capacity) {
    if (*capacity == 0u) {
      newcapacity = 16u;
    } else {
      if (*capacity > SIZE_MAX / 2u) {
        fprintf (stderr, "ERROR: Tag-stack size overflow.\n");
        exit (EXIT_FAILURE);
      }
      newcapacity = *capacity * 2u;
    }

    if (newcapacity > SIZE_MAX / sizeof (**stack)) {
      fprintf (stderr, "ERROR: Tag-stack allocation size overflow.\n");
      exit (EXIT_FAILURE);
    }

    tmp = realloc (*stack, newcapacity * sizeof (**stack));
    if (tmp == NULL) {
      fprintf (stderr, "ERROR: Cannot enlarge tag stack.\n");
      exit (EXIT_FAILURE);
    }

    *stack = tmp;
    *capacity = newcapacity;
  }

  (*stack)[(*nstack)++] = tag;
}

// Track opening and closing markup tags in source order and append closures for
// any openings that remain unmatched. Closures are added in reverse opening
// order so the generated tags are properly nested.
void
append_missing_closures (char **text, int ntext) {

  TAGTYPE *stack;
  size_t nstack, capacity, i;
  int line;
  const char *p;
  const char *closure;

  if (ntext <= 0) return;
  if (text == NULL) {
    fprintf (stderr, "ERROR: Missing subtitle text in append_missing_closures().\n");
    exit (EXIT_FAILURE);
  }

  stack = NULL;
  nstack = 0u;
  capacity = 0u;

  for (line = 0; line < ntext; line++) {
    if (text[line] == NULL) continue;

    p = text[line];
    while (*p != '\0') {

      if (strncasecmp (p, "<i>", 3u) == 0) {
        open_tag (&stack, &nstack, &capacity, TAG_ITALIC);
        p += 3;
      } else if (strncasecmp (p, "</i>", 4u) == 0) {
        close_tag (stack, &nstack, TAG_ITALIC);
        p += 4;
      } else if (strncasecmp (p, "<b>", 3u) == 0) {
        open_tag (&stack, &nstack, &capacity, TAG_BOLD);
        p += 3;
      } else if (strncasecmp (p, "</b>", 4u) == 0) {
        close_tag (stack, &nstack, TAG_BOLD);
        p += 4;
      } else if (strncasecmp (p, "<u>", 3u) == 0) {
        open_tag (&stack, &nstack, &capacity, TAG_UNDERLINE);
        p += 3;
      } else if (strncasecmp (p, "</u>", 4u) == 0) {
        close_tag (stack, &nstack, TAG_UNDERLINE);
        p += 4;
      } else if (strncasecmp (p, "<s>", 3u) == 0) {
        open_tag (&stack, &nstack, &capacity, TAG_STRIKEOUT);
        p += 3;
      } else if (strncasecmp (p, "</s>", 4u) == 0) {
        close_tag (stack, &nstack, TAG_STRIKEOUT);
        p += 4;
      } else if ((strncasecmp (p, "<font color=", 12u) == 0) ||
                 (strncasecmp (p, "<font size=", 11u) == 0)) {
        open_tag (&stack, &nstack, &capacity, TAG_FONT);
        p++;
      } else if (strncasecmp (p, "</font>", 7u) == 0) {
        close_tag (stack, &nstack, TAG_FONT);
        p += 7;
      } else {
        p++;
      }
    }
  }

  // Append one closure for every unmatched opening, starting with the most
  // recently opened tag.
  i = nstack;
  while (i > 0u) {
    i--;

    switch (stack[i]) {
      case TAG_ITALIC:
        closure = "</i>";
        break;
      case TAG_BOLD:
        closure = "</b>";
        break;
      case TAG_UNDERLINE:
        closure = "</u>";
        break;
      case TAG_STRIKEOUT:
        closure = "</s>";
        break;
      case TAG_FONT:
        closure = "</font>";
        break;
      default:
        fprintf (stderr, "ERROR: Unknown tag type in append_missing_closures().\n");
        free (stack);
        exit (EXIT_FAILURE);
    }

    append_string (&text[ntext - 1], closure);
  }

  free (stack);
}

// Append suffix to a dynamically allocated string.
void
append_string (char **string, const char *suffix) {

  char *tmp;
  size_t oldlen, addlen;

  if ((string == NULL) || (*string == NULL) || (suffix == NULL)) {
    fprintf (stderr, "ERROR: Invalid argument to append_string().\n");
    exit (EXIT_FAILURE);
  }

  oldlen = strlen (*string);
  addlen = strlen (suffix);

  if (addlen > SIZE_MAX - oldlen - 1u) {
    fprintf (stderr, "ERROR: String size overflow in append_string().\n");
    exit (EXIT_FAILURE);
  }

  tmp = realloc (*string, oldlen + addlen + 1u);
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot enlarge string in append_string().\n");
    exit (EXIT_FAILURE);
  }

  memcpy (tmp + oldlen, suffix, addlen + 1u);
  *string = tmp;
}

// Allocate memory for an array of chars.
char *
allocate_strmem (size_t len) {

  char *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate zero bytes in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate memory for an array of pointers to arrays of chars.
char **
allocate_strmemp (size_t len) {

  char **tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate zero pointers in allocate_strmemp().\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmemp().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate memory for an array of BOM structs.
BOM *
allocate_bommem (size_t len) {

  BOM *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate zero BOM structs in allocate_bommem().\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_bommem().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}
