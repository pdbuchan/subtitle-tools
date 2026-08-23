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

// time-text.c - Take the timestamps from one SubRip (.srt) file and the subtitle texts from another
//               SubRip file and create a new SubRip file with those timestamps and subtitle texts.
//               If a UTF-8 Byte Order Mark (BOM) exists in the SubRip file containing the text, it
//               will be included in the output file.

// gcc -std=c11 -Wall -Wextra -Wpedantic time-text.c -o time-text

// Usage: ./time-text timeinputfile.srt textinputfile.srt
// Output: out.srt

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

typedef struct {
  size_t number;
  size_t timestamp;
  size_t text_start;
  size_t text_end;
} SUBTITLE;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (const char *, const BOM *);
int load_file (const char *, char ***, size_t *, int *);
int parse_subtitles (const char *, char **, size_t, SUBTITLE **, size_t *);
int subtitle_number (const char *);
int timestamp_line (const char *);
int timestamp (const char *, size_t);
void free_lines (char **, size_t);
void *allocate_memory (size_t, size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXBOM 11   // Number of Byte Order Mark (BOM) types

// Byte Order Mark (BOM) names and sequences.
static const uint8_t utf8[]      = {0xef, 0xbb, 0xbf};
static const uint8_t utf16be[]   = {0xfe, 0xff};
static const uint8_t utf16le[]   = {0xff, 0xfe};
static const uint8_t utf32be[]   = {0x00, 0x00, 0xfe, 0xff};
static const uint8_t utf32le[]   = {0xff, 0xfe, 0x00, 0x00};
static const uint8_t utf7[]      = {0x2b, 0x2f, 0x76};
static const uint8_t utf1[]      = {0xf7, 0x64, 0x4c};
static const uint8_t utfebcdic[] = {0xdd, 0x73, 0x66, 0x73};
static const uint8_t scsu[]      = {0x0e, 0xfe, 0xff};
static const uint8_t bocu1[]     = {0xfb, 0xee, 0x28};
static const uint8_t gb18030[]   = {0x84, 0x31, 0x95, 0x33};

static const BOM bom[MAXBOM] = {
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

int
main (int argc, char **argv) {

  int timebom, textbom, failed;
  size_t i, sub, ntimelines, ntextlines, ntimesubs, ntextsubs;
  char **inputtime, **inputtext;
  SUBTITLE *timesub, *textsub;
  const char *timefilename, *textfilename;
  FILE *fo;

  if (argc != 3) {
    fprintf (stdout, "\nUsage: ./time-text timeinputfilename.srt textinputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  timefilename = argv[1];
  textfilename = argv[2];

  inputtime = NULL;
  inputtext = NULL;
  timesub = NULL;
  textsub = NULL;

  load_file (timefilename, &inputtime, &ntimelines, &timebom);
  load_file (textfilename, &inputtext, &ntextlines, &textbom);

  fprintf (stdout, "\n%s: %zu lines found excluding excess trailing line-feeds.\n", timefilename, ntimelines);
  fprintf (stdout, "%s: %zu lines found excluding excess trailing line-feeds.\n", textfilename, ntextlines);

  if (timebom < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", timefilename);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n", timefilename, bom[timebom].name);
  }

  if (textbom < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", textfilename);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n", textfilename, bom[textbom].name);
  }

  // This is a byte-oriented parser. UTF-8 (with or without a BOM) is supported;
  // other BOM-marked encodings must be converted before use.
  if (timebom > 0) {
    fprintf (stderr, "ERROR: Input file %s is encoded as %s.\n", timefilename, bom[timebom].name);
    fprintf (stderr, "       Convert it to UTF-8 before using this program.\n");
    free_lines (inputtime, ntimelines);
    free_lines (inputtext, ntextlines);
    return (EXIT_FAILURE);
  }
  if (textbom > 0) {
    fprintf (stderr, "ERROR: Input file %s is encoded as %s.\n", textfilename, bom[textbom].name);
    fprintf (stderr, "       Convert it to UTF-8 before using this program.\n");
    free_lines (inputtime, ntimelines);
    free_lines (inputtext, ntextlines);
    return (EXIT_FAILURE);
  }

  // Remove UTF-8 BOMs before parsing the first subtitle number.
  if (timebom == 0) {
    memmove (inputtime[0], inputtime[0] + bom[0].len, strlen (inputtime[0] + bom[0].len) + 1u);
  }
  if (textbom == 0) {
    memmove (inputtext[0], inputtext[0] + bom[0].len, strlen (inputtext[0] + bom[0].len) + 1u);
  }

  parse_subtitles (timefilename, inputtime, ntimelines, &timesub, &ntimesubs);
  parse_subtitles (textfilename, inputtext, ntextlines, &textsub, &ntextsubs);

  fprintf (stdout, "\n%zu subtitles found in %s.\n", ntimesubs, timefilename);
  fprintf (stdout, "%zu subtitles found in %s.\n\n", ntextsubs, textfilename);

  if (ntimesubs != ntextsubs) {
    fprintf (stderr, "ERROR: Files %s and %s have different numbers of subtitles.\n", timefilename, textfilename);
    free (timesub);
    free (textsub);
    free_lines (inputtime, ntimelines);
    free_lines (inputtext, ntextlines);
    return (EXIT_FAILURE);
  }

  errno = 0;
  fo = fopen ("out.srt", "wx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.srt.\n");
    }
    free (timesub);
    free (textsub);
    free_lines (inputtime, ntimelines);
    free_lines (inputtext, ntextlines);
    return (EXIT_FAILURE);
  }

  failed = 0;

  // The output BOM is determined only by the file supplying the desired text.
  if (textbom == 0) {
    if (fwrite (bom[0].sequence, sizeof (bom[0].sequence[0]), bom[0].len, fo) != bom[0].len) {
      failed = 1;
    }
  }

  for (sub=0u; (sub<ntimesubs) && !failed; sub++) {

    if (fprintf (fo, "%zu\n", sub + 1u) < 0) {
      failed = 1;
      break;
    }

    // Timestamp comes from the first input file.
    if (fputs (inputtime[timesub[sub].timestamp], fo) == EOF) {
      failed = 1;
      break;
    }

    // Text comes from the second input file. The number of text lines does not
    // need to match the number in the timestamp source file.
    for (i = textsub[sub].text_start; i < textsub[sub].text_end; i++) {
      if (fputs (inputtext[i], fo) == EOF) {
        failed = 1;
        break;
      }
    }

    if (!failed && (fputc ('\n', fo) == EOF)) {
      failed = 1;
    }
  }

  if (fclose (fo) == EOF) {
    failed = 1;
  }

  if (failed) {
    fprintf (stderr, "ERROR: Unable to write complete output file out.srt.\n");
    if (remove ("out.srt") != 0) {
      fprintf (stderr, "WARNING: Unable to remove incomplete output file out.srt.\n");
    }
  }

  free (timesub);
  free (textsub);
  free_lines (inputtime, ntimelines);
  free_lines (inputtext, ntextlines);

  if (failed) {
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Read an input file into an array of lines. Excess blank lines at the end are
// removed while retaining one blank line to close the final subtitle.
int
load_file (const char *filename, char ***lines, size_t *nlines, int *bomtype) {

  int status;
  size_t count, line;
  char temp[MAXLEN];
  char **input;
  FILE *fi;

  if ((filename == NULL) || (lines == NULL) || (nlines == NULL) || (bomtype == NULL)) {
    fprintf (stderr, "ERROR: Invalid argument passed to load_file().\n");
    exit (EXIT_FAILURE);
  }

  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  count = 0u;
  for (;;) {
    status = readline (fi, temp, MAXLEN);
    if (status == 0) {
      count++;
    } else if (status == -1) {
      break;
    } else if (status == -2) {
      fprintf (stderr, "ERROR: Line %zu in input SubRip file %s is too long for the %d-byte input buffer.\n", count + 1u, filename, MAXLEN);
      fclose (fi);
      exit (EXIT_FAILURE);
    } else {
      fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }

  if (count == 0u) {
    fprintf (stderr, "ERROR: Input SubRip file %s is empty.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  if (fseek (fi, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind input SubRip file %s.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  input = allocate_memory (count, sizeof (*input));
  for (line = 0u; line < count; line++) {
    input[line] = allocate_memory (MAXLEN, sizeof (*input[line]));

    status = readline (fi, input[line], MAXLEN);
    if (status != 0) {
      fprintf (stderr, "ERROR: Cannot read line %zu from input SubRip file %s.\n", line + 1u, filename);
      free_lines (input, line + 1u);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }

  if (fclose (fi) == EOF) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    free_lines (input, count);
    exit (EXIT_FAILURE);
  }

  // Remove excess trailing blank lines while retaining one closing blank line.
  while ((count > 1u) && (input[count - 1u][0] == '\n') && (input[count - 2u][0] == '\n')) {
    free (input[count - 1u]);
    input[count - 1u] = NULL;
    count--;
  }

  *bomtype = byteordermark (input[0], bom);
  *lines = input;
  *nlines = count;

  return (EXIT_SUCCESS);
}

// Parse and validate the basic SubRip block structure. The timestamp source and
// text source are parsed independently because their text-line counts may differ.
int
parse_subtitles (const char *filename, char **lines, size_t nlines, SUBTITLE **subtitles, size_t *nsubs) {

  size_t count, line, start;
  SUBTITLE *sub;

  if ((filename == NULL) || (lines == NULL) || (subtitles == NULL) || (nsubs == NULL) || (nlines == 0u)) {
    fprintf (stderr, "ERROR: Invalid argument passed to parse_subtitles().\n");
    exit (EXIT_FAILURE);
  }

  if (lines[nlines - 1u][0] != '\n') {
    fprintf (stderr, "ERROR: Final subtitle in %s is not closed by a blank line.\n", filename);
    exit (EXIT_FAILURE);
  }

  // First pass: validate and count subtitles.
  count = 0u;
  line = 0u;
  while (line < nlines) {

    if (lines[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at line %zu in %s.\n", line + 1u, filename);
      exit (EXIT_FAILURE);
    }
    if (!subtitle_number (lines[line])) {
      fprintf (stderr, "ERROR: Invalid subtitle number at line %zu in %s: %s", line + 1u, filename, lines[line]);
      exit (EXIT_FAILURE);
    }
    line++;

    if ((line >= nlines) || (lines[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Missing timestamp line for subtitle %zu in %s.\n", count + 1u, filename);
      exit (EXIT_FAILURE);
    }
    if (!timestamp_line (lines[line])) {
      fprintf (stderr, "ERROR: Malformed timestamp at line %zu in %s: %s", line + 1u, filename, lines[line]);
      exit (EXIT_FAILURE);
    }
    line++;

    start = line;
    while ((line < nlines) && (lines[line][0] != '\n')) {
      line++;
    }

    if (line == start) {
      fprintf (stderr, "ERROR: Subtitle %zu in %s contains no text.\n", count + 1u, filename);
      exit (EXIT_FAILURE);
    }
    if (line >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %zu in %s is not closed by a blank line.\n", count + 1u, filename);
      exit (EXIT_FAILURE);
    }

    line++;  // Skip separator.
    count++;
  }

  sub = allocate_memory (count, sizeof (*sub));

  // Second pass: record each block's useful line positions.
  line = 0u;
  count = 0u;
  while (line < nlines) {
    sub[count].number = line++;
    sub[count].timestamp = line++;
    sub[count].text_start = line;
    while (lines[line][0] != '\n') {
      line++;
    }
    sub[count].text_end = line;
    line++;
    count++;
  }

  *subtitles = sub;
  *nsubs = count;

  return (EXIT_SUCCESS);
}

// Return nonzero if a line contains only a positive decimal subtitle number and
// an optional terminating line-feed.
int
subtitle_number (const char *line) {

  size_t i;

  if ((line == NULL) || (line[0] < '1') || (line[0] > '9')) {
    return (0);
  }

  i = 0u;
  while ((line[i] >= '0') && (line[i] <= '9')) {
    i++;
  }

  if (line[i] == '\n') {
    i++;
  }

  return (line[i] == '\0');
}

// Return nonzero if a line has a basic SubRip timestamp of the form
// hh:mm:ss,mmm --> hh:mm:ss,mmm. Two-digit milliseconds are also accepted to
// accommodate SRT files occasionally encountered in that form.
int
timestamp_line (const char *line) {

  const char *arrow;
  size_t leftlen, rightlen;

  if (line == NULL) {
    return (0);
  }

  arrow = strstr (line, " --> ");
  if (arrow == NULL) {
    return (0);
  }
  if (strstr (arrow + 5, " --> ") != NULL) {
    return (0);
  }

  leftlen = (size_t) (arrow - line);
  rightlen = strlen (arrow + 5);
  if ((rightlen > 0u) && (arrow[5 + rightlen - 1u] == '\n')) {
    rightlen--;
  }

  return (timestamp (line, leftlen) && timestamp (arrow + 5, rightlen));
}

// Validate one timestamp component string. Hours are two digits; minutes and
// seconds must be 00-59; milliseconds may contain two or three digits.
int
timestamp (const char *text, size_t len) {

  int minute, second;
  size_t i;

  if ((text == NULL) || ((len != 11u) && (len != 12u))) {
    return (0);
  }

  if ((text[2] != ':') || (text[5] != ':') || (text[8] != ',')) {
    return (0);
  }

  for (i = 0u; i < len; i++) {
    if ((i == 2u) || (i == 5u) || (i == 8u)) {
      continue;
    }
    if ((text[i] < '0') || (text[i] > '9')) {
      return (0);
    }
  }

  minute = ((text[3] - '0') * 10) + (text[4] - '0');
  second = ((text[6] - '0') * 10) + (text[7] - '0');

  if ((minute > 59) || (second > 59)) {
    return (0);
  }

  return (1);
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

    if (ch == EOF) {
      if (ferror (fi)) {
        line[0] = '\0';
        return (-3);
      }

      if (i == 0) {
        line[0] = '\0';
        return (-1);
      }

      line[i] = '\0';
      return (0);
    }

    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      if (i >= (limit - 1)) {
        line[limit - 1] = '\0';
        return (-2);
      }

      line[i++] = '\n';
      line[i] = '\0';
      return (0);
    }

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

// Detect a Byte Order Mark at the beginning of a buffer. The longest matching
// signature is selected because UTF-16LE is a prefix of UTF-32LE.
int
byteordermark (const char *text, const BOM *list) {

  int type, best;
  size_t bestlen;

  if ((text == NULL) || (list == NULL)) {
    return (-1);
  }

  best = -1;
  bestlen = 0u;

  for (type = 0; type < MAXBOM; type++) {
    if ((list[type].len > bestlen) &&
        (memcmp (text, list[type].sequence, list[type].len) == 0)) {
      best = type;
      bestlen = list[type].len;
    }
  }

  return (best);
}

// Free an array of allocated lines.
void
free_lines (char **lines, size_t nlines) {

  size_t i;

  if (lines == NULL) {
    return;
  }

  for (i = 0u; i < nlines; i++) {
    free (lines[i]);
  }
  free (lines);
}

// Allocate zero-initialized memory and terminate on failure.
void *
allocate_memory (size_t nmemb, size_t size) {

  void *tmp;

  if ((nmemb == 0u) || (size == 0u)) {
    fprintf (stderr, "ERROR: Invalid zero-sized allocation requested.\n");
    exit (EXIT_FAILURE);
  }

  if (nmemb > (SIZE_MAX / size)) {
    fprintf (stderr, "ERROR: Requested allocation is too large.\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (nmemb, size);
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Unable to allocate memory.\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}
