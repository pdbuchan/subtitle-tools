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

// txt2srt.c - Take the timestamps from a SubRip (.srt) file and the text from a
//             text file and create a new .srt file. The text file must have the
//             same number of subtitles as the SubRip file, with subtitle text
//             blocks separated by single blank lines.
//             If a UTF-8 Byte Order Mark (BOM) exists in the text file, it is
//             preserved in the output file.

// gcc -std=c11 -Wall -Wextra -Wpedantic txt2srt.c -o txt2srt

// Usage: ./txt2srt inputfilename.srt inputfilename.txt
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

typedef struct {
  size_t start;
  size_t end;
} TEXTBLOCK;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (const uint8_t *, size_t, const BOM *);
int load_file (const char *, const char *, char ***, size_t *, int *);
int parse_srt (const char *, char **, size_t, SUBTITLE **, size_t *);
int parse_text (const char *, char **, size_t, TEXTBLOCK **, size_t *);
int subtitle_number (const char *);
int timestamp_line (const char *);
int timestamp (const char *, size_t);
void free_lines (char **, size_t);
void *allocate_memory (size_t, size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per physical input line
#define MAXBOM 11   // Number of Byte Order Mark (BOM) types
#define BOMPREFIX 4 // Maximum number of bytes in a listed BOM

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

  int srtbom, txtbom, failed;
  size_t i, sub, nsrtlines, ntxtlines, nsrtsubs, ntxtsubs;
  char **srtinput, **txtinput;
  SUBTITLE *srtsub;
  TEXTBLOCK *txtsub;
  const char *srtfilename, *txtfilename;
  FILE *fo;

  if (argc != 3) {
    fprintf (stdout, "\nUsage: ./txt2srt inputfilename.srt inputfilename.txt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  srtfilename = argv[1];
  txtfilename = argv[2];

  srtinput = NULL;
  txtinput = NULL;
  srtsub = NULL;
  txtsub = NULL;

  load_file (srtfilename, "SubRip", &srtinput, &nsrtlines, &srtbom);
  load_file (txtfilename, "text", &txtinput, &ntxtlines, &txtbom);

  fprintf (stdout, "\n%s: %zu lines read.\n", srtfilename, nsrtlines);
  fprintf (stdout, "%s: %zu lines read.\n", txtfilename, ntxtlines);

  if (srtbom < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", srtfilename);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n",
             srtfilename, bom[srtbom].name);
  }

  if (txtbom < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", txtfilename);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n", txtfilename, bom[txtbom].name);
  }

  // This is a byte-oriented parser. UTF-8 (with or without a BOM) is supported;
  // other recognized BOM-marked encodings must be converted before use.
  if (srtbom > 0) {
    fprintf (stderr, "ERROR: Input file %s is encoded as %s.\n", srtfilename, bom[srtbom].name);
    fprintf (stderr, "       Convert it to UTF-8 before using this program.\n");
    free_lines (srtinput, nsrtlines);
    free_lines (txtinput, ntxtlines);
    return (EXIT_FAILURE);
  }
  if (txtbom > 0) {
    fprintf (stderr, "ERROR: Input file %s is encoded as %s.\n", txtfilename, bom[txtbom].name);
    fprintf (stderr, "       Convert it to UTF-8 before using this program.\n");
    free_lines (srtinput, nsrtlines);
    free_lines (txtinput, ntxtlines);
    return (EXIT_FAILURE);
  }

  // Remove UTF-8 BOMs before parsing. The text-file BOM is written explicitly
  // to the output later so it appears exactly once.
  if (srtbom == 0) {
    memmove (srtinput[0], srtinput[0] + bom[0].len, strlen (srtinput[0] + bom[0].len) + 1u);
  }
  if (txtbom == 0) {
    memmove (txtinput[0], txtinput[0] + bom[0].len, strlen (txtinput[0] + bom[0].len) + 1u);
  }

  parse_srt (srtfilename, srtinput, nsrtlines, &srtsub, &nsrtsubs);
  parse_text (txtfilename, txtinput, ntxtlines, &txtsub, &ntxtsubs);

  fprintf (stdout, "\n%zu subtitles found in %s.\n", nsrtsubs, srtfilename);
  fprintf (stdout, "%zu subtitle text blocks found in %s.\n\n", ntxtsubs, txtfilename);

  if (nsrtsubs != ntxtsubs) {
    fprintf (stderr, "ERROR: Files %s and %s contain different numbers of subtitles.\n", srtfilename, txtfilename);
    free (srtsub);
    free (txtsub);
    free_lines (srtinput, nsrtlines);
    free_lines (txtinput, ntxtlines);
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
    free (srtsub);
    free (txtsub);
    free_lines (srtinput, nsrtlines);
    free_lines (txtinput, ntxtlines);
    return (EXIT_FAILURE);
  }

  failed = 0;

  // The output BOM is determined only by the text file supplying the desired
  // subtitle text.
  if (txtbom == 0) {
    if (fwrite (bom[0].sequence, sizeof (bom[0].sequence[0]), bom[0].len, fo) != bom[0].len) {
      failed = 1;
    }
  }

  for (sub = 0u; (sub < nsrtsubs) && !failed; sub++) {

    // Renumber subtitles consecutively in the generated file.
    if (fprintf (fo, "%zu\n", sub + 1u) < 0) {
      failed = 1;
      break;
    }

    // Timestamp comes from the SubRip source file.
    if (fputs (srtinput[srtsub[sub].timestamp], fo) == EOF) {
      failed = 1;
      break;
    }

    // Text comes from the plain-text source. Its number of lines does not need
    // to match the number of text lines in the SubRip timestamp source.
    for (i = txtsub[sub].start; i < txtsub[sub].end; i++) {
      if (fputs (txtinput[i], fo) == EOF) {
        failed = 1;
        break;
      }

      // readline() accepts a final physical line without a line-feed. Add one
      // here so every SRT text line is terminated before the block separator.
      if (!failed) {
        size_t len;

        len = strlen (txtinput[i]);
        if ((len == 0u) || (txtinput[i][len - 1u] != '\n')) {
          if (fputc ('\n', fo) == EOF) {
            failed = 1;
            break;
          }
        }
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

  free (srtsub);
  free (txtsub);
  free_lines (srtinput, nsrtlines);
  free_lines (txtinput, ntxtlines);

  if (failed) {
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Read an input file into an array of lines. BOM detection is performed from
// the actual first bytes of the file so short UTF-16/UTF-32 inputs are not
// misidentified merely because an allocated line buffer contains trailing zeroes.
int
load_file (const char *filename, const char *kind, char ***lines, size_t *nlines, int *bomtype) {

  int status;
  size_t count, line, prefixlen;
  uint8_t prefix[BOMPREFIX];
  char temp[MAXLEN];
  char **input;
  FILE *fi;

  if ((filename == NULL) || (kind == NULL) || (lines == NULL) ||
      (nlines == NULL) || (bomtype == NULL)) {
    fprintf (stderr, "ERROR: Invalid argument passed to load_file().\n");
    exit (EXIT_FAILURE);
  }

  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input %s file %s.\n", kind, filename);
    exit (EXIT_FAILURE);
  }

  prefixlen = fread (prefix, sizeof (prefix[0]), BOMPREFIX, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to read input %s file %s.\n", kind, filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }
  *bomtype = byteordermark (prefix, prefixlen, bom);

  if (fseek (fi, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind input %s file %s.\n", kind, filename);
    fclose (fi);
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
      fprintf (stderr, "ERROR: Line %zu in input %s file %s is too long for the %d-byte input buffer.\n", count + 1u, kind, filename, MAXLEN);
      fclose (fi);
      exit (EXIT_FAILURE);
    } else {
      fprintf (stderr, "ERROR: Unable to read input %s file %s.\n", kind, filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }

  if (count == 0u) {
    fprintf (stderr, "ERROR: Input %s file %s is empty.\n", kind, filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  if (fseek (fi, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind input %s file %s.\n", kind, filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  input = allocate_memory (count, sizeof (*input));
  for (line = 0u; line < count; line++) {
    input[line] = allocate_memory (MAXLEN, sizeof (*input[line]));

    status = readline (fi, input[line], MAXLEN);
    if (status != 0) {
      fprintf (stderr, "ERROR: Cannot read line %zu from input %s file %s.\n", line + 1u, kind, filename);
      free_lines (input, line + 1u);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }

  if (fclose (fi) == EOF) {
    fprintf (stderr, "ERROR: Unable to close input %s file %s.\n", kind, filename);
    free_lines (input, count);
    exit (EXIT_FAILURE);
  }

  *lines = input;
  *nlines = count;

  return (EXIT_SUCCESS);
}

// Parse and validate the SubRip file supplying timestamps. Excess blank lines
// at the end are ignored, but one blank line must close the final subtitle.
int
parse_srt (const char *filename, char **lines, size_t nlines, SUBTITLE **subtitles, size_t *nsubs) {

  size_t count, line, start, used;
  SUBTITLE *sub;

  if ((filename == NULL) || (lines == NULL) || (subtitles == NULL) || (nsubs == NULL) || (nlines == 0u)) {
    fprintf (stderr, "ERROR: Invalid argument passed to parse_srt().\n");
    exit (EXIT_FAILURE);
  }

  used = nlines;
  while ((used > 1u) && (lines[used - 1u][0] == '\n') && (lines[used - 2u][0] == '\n')) {
    used--;
  }

  if (lines[used - 1u][0] != '\n') {
    fprintf (stderr, "ERROR: Final subtitle in %s is not closed by a blank line.\n", filename);
    exit (EXIT_FAILURE);
  }

  count = 0u;
  line = 0u;
  while (line < used) {

    if (lines[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at line %zu in %s.\n", line + 1u, filename);
      exit (EXIT_FAILURE);
    }
    if (!subtitle_number (lines[line])) {
      fprintf (stderr, "ERROR: Invalid subtitle number at line %zu in %s: %s", line + 1u, filename, lines[line]);
      exit (EXIT_FAILURE);
    }
    line++;

    if ((line >= used) || (lines[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Missing timestamp line for subtitle %zu in %s.\n", count + 1u, filename);
      exit (EXIT_FAILURE);
    }
    if (!timestamp_line (lines[line])) {
      fprintf (stderr, "ERROR: Malformed timestamp at line %zu in %s: %s", line + 1u, filename, lines[line]);
      exit (EXIT_FAILURE);
    }
    line++;

    start = line;
    while ((line < used) && (lines[line][0] != '\n')) {
      line++;
    }

    if (line == start) {
      fprintf (stderr, "ERROR: Subtitle %zu in %s contains no text.\n", count + 1u, filename);
      exit (EXIT_FAILURE);
    }
    if (line >= used) {
      fprintf (stderr, "ERROR: Subtitle %zu in %s is not closed by a blank line.\n", count + 1u, filename);
      exit (EXIT_FAILURE);
    }

    line++;  // Skip separator.
    count++;
  }

  sub = allocate_memory (count, sizeof (*sub));

  line = 0u;
  count = 0u;
  while (line < used) {
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

// Parse the plain-text subtitle blocks. Blocks must be separated by exactly one
// blank line. Excess blank lines at the very end are ignored. The final block
// may end at EOF without a trailing blank line.
int
parse_text (const char *filename, char **lines, size_t nlines, TEXTBLOCK **subtitles, size_t *nsubs) {

  size_t count, line, used;
  TEXTBLOCK *sub;

  if ((filename == NULL) || (lines == NULL) || (subtitles == NULL) || (nsubs == NULL) || (nlines == 0u)) {
    fprintf (stderr, "ERROR: Invalid argument passed to parse_text().\n");
    exit (EXIT_FAILURE);
  }

  used = nlines;
  while ((used > 0u) && (lines[used - 1u][0] == '\n')) {
    used--;
  }

  if (used == 0u) {
    fprintf (stderr, "ERROR: Input text file %s contains no subtitle text.\n", filename);
    exit (EXIT_FAILURE);
  }

  count = 0u;
  line = 0u;
  while (line < used) {

    if (lines[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at line %zu in %s.\n", line + 1u, filename);
      exit (EXIT_FAILURE);
    }

    while ((line < used) && (lines[line][0] != '\n')) {
      line++;
    }
    count++;

    if (line < used) {
      line++;  // Skip exactly one separator.
      if ((line < used) && (lines[line][0] == '\n')) {
        fprintf (stderr, "ERROR: More than one blank line separates subtitle text blocks near line %zu in %s.\n", line + 1u, filename);
        exit (EXIT_FAILURE);
      }
    }
  }

  sub = allocate_memory (count, sizeof (*sub));

  line = 0u;
  count = 0u;
  while (line < used) {
    sub[count].start = line;
    while ((line < used) && (lines[line][0] != '\n')) {
      line++;
    }
    sub[count].end = line;
    count++;
    if (line < used) {
      line++;
    }
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

// Validate one timestamp. Hours are two digits; minutes and seconds must be
// 00-59; milliseconds may contain two or three digits.
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

// Detect a Byte Order Mark in a prefix of known length. The longest matching
// signature is selected because UTF-16LE is a prefix of UTF-32LE.
int
byteordermark (const uint8_t *text, size_t nbytes, const BOM *list) {

  int type, best;
  size_t bestlen;

  if ((text == NULL) || (list == NULL)) {
    return (-1);
  }

  best = -1;
  bestlen = 0u;

  for (type = 0; type < MAXBOM; type++) {
    if ((list[type].len <= nbytes) && (list[type].len > bestlen) && (memcmp (text, list[type].sequence, list[type].len) == 0)) {
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
