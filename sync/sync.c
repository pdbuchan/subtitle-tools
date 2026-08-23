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

// sync.c - Read an existing SubRip (.srt) file and synchronize all timestamps to user-input anchor points.
//          Subtitle durations are preserved.

// Synchronization is accomplished by using "first" and "last" timestamps as anchor-points.
// Choose "first" and "last" subtitles that are near or at beginning and end of the feature in order to maximize scaling accuracy.

// gcc -std=c11 -Wall sync.c -o sync

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
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

typedef struct {
  int h;
  int m;
  int s;
  int ms;
  int64_t totalms;
} TIME;

// Function prototypes
int inputtext (char *);
int readline (FILE *, char *, int);
int byteordermark (const uint8_t *, size_t, const BOM *);
int extract_time (const char *, TIME *, TIME *);
int parsetimestamp (const char *, TIME *);
int timetoms (TIME *);
int mstotime (TIME *);
int is_blank_line (const char *);
int valid_subtitle_number (const char *);
int write_output (char **, size_t, size_t, const TIME *, const TIME *, int);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
int64_t *allocate_int64mem (size_t);
TIME *allocate_timemem (size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

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
  {3u, "UTF-8", utf8},
  {2u, "UTF-16 (BE)", utf16be},
  {2u, "UTF-16 (LE)", utf16le},
  {4u, "UTF-32 (BE)", utf32be},
  {4u, "UTF-32 (LE)", utf32le},
  {3u, "UTF-7", utf7},
  {3u, "UTF-1", utf1},
  {4u, "UTF-EBCDIC", utfebcdic},
  {3u, "SCSU", scsu},
  {3u, "BOCU-1", bocu1},
  {4u, "GB18030", gb18030}
};

int
main (int argc, char **argv) {

  int rc, type;
  size_t i, alllines, nlines, line, nsubs;
  size_t prefixlen;
  int64_t oldfirstms, oldlastms, newfirstms, newlastms, *duration;
  int64_t oldspan, newspan;
  char temp[MAXLEN], timestamp[MAXLEN], **input;
  uint8_t prefix[4] = {0u, 0u, 0u, 0u};
  long double ratio, scaled;
  TIME *start, *end, inputtime;
  FILE *fi;
  const char *filename;

  // Process the command line arguments, if any.
  if (argc == 2) {
    filename = argv[1];
  } else {
    fprintf (stdout, "\nUsage: ./sync inputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file in binary mode so a BOM can be examined exactly.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Examine at most the first four bytes for a BOM before line-oriented parsing.
  prefixlen = fread (prefix, sizeof (*prefix), sizeof (prefix), fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  type = byteordermark (prefix, prefixlen, bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    // The parser below is byte-oriented. UTF-8 is compatible after removing its BOM,
    // whereas the other BOM-marked encodings require character decoding first.
    if (type != 0) {
      fprintf (stderr, "ERROR: %s input is not supported by this byte-oriented parser.\n", bom[type].name);
      fprintf (stderr, "       Convert the input file to UTF-8 before running sync.\n");
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  if (fseek (fi, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind input SubRip file %s.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Count lines of input SubRip file, while distinguishing EOF from errors.
  alllines = 0u;
  for (;;) {
    rc = readline (fi, temp, MAXLEN);
    if (rc == 0) {
      alllines++;
    } else if (rc == -1) {
      break;
    } else if (rc == -2) {
      fprintf (stderr, "ERROR: Line %zu in input SubRip file %s does not fit in the %d-byte input buffer.\n", alllines + 1u, filename, MAXLEN);
      fclose (fi);
      return (EXIT_FAILURE);
    } else {
      fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  if (alllines == 0u) {
    fprintf (stderr, "ERROR: Input SubRip file %s is empty.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "\n%zu lines found including any excess trailing line-feeds.\n", alllines);

  if (fseek (fi, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind input SubRip file %s.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Allocate memory for array to hold input file.
  input = allocate_strmemp (alllines);
  for (line=0u; line<alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0u; line<alllines; line++) {
    rc = readline (fi, input[line], MAXLEN);
    if (rc != 0) {
      if (rc == -1) {
        fprintf (stderr, "ERROR: Unexpected end of file while reading line %zu from %s.\n", line + 1u, filename);
      } else if (rc == -2) {
        fprintf (stderr, "ERROR: Line %zu in input SubRip file %s does not fit in the %d-byte input buffer.\n", line + 1u, filename, MAXLEN);
      } else {
        fprintf (stderr, "ERROR: Unable to read line %zu from input SubRip file %s.\n", line + 1u, filename);
      }
      fclose (fi);
      for (i=0u; i<alllines; i++) free (input[i]);
      free (input);
      return (EXIT_FAILURE);
    }
  }

  // Close input file.
  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    for (i=0u; i<alllines; i++) free (input[i]);
    free (input);
    return (EXIT_FAILURE);
  }

  // Remove the UTF-8 BOM from the first logical line; it is written explicitly later.
  if (type == 0) {
    size_t len = strlen (input[0]);
    if ((len < bom[0].len) || (memcmp (input[0], bom[0].sequence, bom[0].len) != 0)) {
      fprintf (stderr, "ERROR: UTF-8 BOM could not be removed from first input line.\n");
      for (i=0u; i<alllines; i++) free (input[i]);
      free (input);
      return (EXIT_FAILURE);
    }
    memmove (input[0], input[0] + bom[0].len, len - bom[0].len + 1u);
  }

  // Remove excess blank lines at end while retaining one closing blank line.
  nlines = alllines;
  while ((nlines > 1u) && is_blank_line (input[nlines - 1u]) && is_blank_line (input[nlines - 2u])) {
    nlines--;
  }
  fprintf (stdout, "%zu lines found excluding excess trailing line-feeds.\n", nlines);

  if (!is_blank_line (input[nlines - 1u])) {
    fprintf (stderr, "ERROR: Final subtitle in %s is not terminated by a blank line.\n", filename);
    for (i=0u; i<alllines; i++) free (input[i]);
    free (input);
    return (EXIT_FAILURE);
  }

  // Validate SubRip block structure and count subtitles.
  nsubs = 0u;
  line = 0u;
  while (line < nlines) {

    if (is_blank_line (input[line])) {
      fprintf (stderr, "ERROR: Unexpected blank line at input line %zu.\n", line + 1u);
      for (i=0u; i<alllines; i++) free (input[i]);
      free (input);
      return (EXIT_FAILURE);
    }

    if (!valid_subtitle_number (input[line])) {
      fprintf (stderr, "ERROR: Invalid subtitle number at input line %zu: %s", line + 1u, input[line]);
      for (i=0u; i<alllines; i++) free (input[i]);
      free (input);
      return (EXIT_FAILURE);
    }
    line++;

    if ((line >= nlines) || is_blank_line (input[line])) {
      fprintf (stderr, "ERROR: Missing timestamp line for subtitle %zu.\n", nsubs + 1u);
      for (i=0u; i<alllines; i++) free (input[i]);
      free (input);
      return (EXIT_FAILURE);
    }
    line++;

    while ((line < nlines) && !is_blank_line (input[line])) {
      line++;
    }

    if (line >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %zu is not terminated by a blank line.\n", nsubs + 1u);
      for (i=0u; i<alllines; i++) free (input[i]);
      free (input);
      return (EXIT_FAILURE);
    }

    line++;  // Move past subtitle separator.
    nsubs++;

    if ((line < nlines) && is_blank_line (input[line])) {
      fprintf (stderr, "ERROR: Unexpected extra blank line before subtitle %zu.\n", nsubs + 1u);
      for (i=0u; i<alllines; i++) free (input[i]);
      free (input);
      return (EXIT_FAILURE);
    }
  }

  fprintf (stdout, "\n%zu subtitles found.\n", nsubs);

  // Allocate timestamp and duration arrays.
  start = allocate_timemem (nsubs);
  end = allocate_timemem (nsubs);
  duration = allocate_int64mem (nsubs);

  // Extract original timestamps and preserve each subtitle's duration.
  line = 0u;
  for (i=0u; i<nsubs; i++) {
    line++;  // Skip subtitle number.

    extract_time (input[line], &start[i], &end[i]);
    if (end[i].totalms <= start[i].totalms) {
      fprintf (stderr, "ERROR: Subtitle %zu has a non-positive duration.\n", i + 1u);
      for (line=0u; line<alllines; line++) free (input[line]);
      free (input);
      free (start);
      free (end);
      free (duration);
      return (EXIT_FAILURE);
    }
    duration[i] = end[i].totalms - start[i].totalms;
    line++;

    while ((line < nlines) && !is_blank_line (input[line])) line++;
    line++;  // Move past subtitle separator.
  }

  // Ask for current and new timestamps for anchor points.
  fprintf (stdout, "\nCurrent start timestamp for first anchor point subtitle (hh:mm:ss,ms)? ");
  inputtext (timestamp);
  parsetimestamp (timestamp, &inputtime);
  oldfirstms = inputtime.totalms;

  fprintf (stdout, "Current start timestamp for last anchor point subtitle (hh:mm:ss,ms)? ");
  inputtext (timestamp);
  parsetimestamp (timestamp, &inputtime);
  oldlastms = inputtime.totalms;

  fprintf (stdout, "New start timestamp for first anchor point subtitle (hh:mm:ss,ms)? ");
  inputtext (timestamp);
  parsetimestamp (timestamp, &inputtime);
  newfirstms = inputtime.totalms;

  fprintf (stdout, "New start timestamp for last anchor point subtitle (hh:mm:ss,ms)? ");
  inputtext (timestamp);
  parsetimestamp (timestamp, &inputtime);
  newlastms = inputtime.totalms;

  oldspan = oldlastms - oldfirstms;
  newspan = newlastms - newfirstms;

  if (oldspan <= 0) {
    fprintf (stderr, "ERROR: The current last anchor timestamp must be later than the current first anchor timestamp.\n");
    for (line=0u; line<alllines; line++) free (input[line]);
    free (input);
    free (start);
    free (end);
    free (duration);
    return (EXIT_FAILURE);
  }

  if (newspan <= 0) {
    fprintf (stderr, "ERROR: The new last anchor timestamp must be later than the new first anchor timestamp.\n");
    for (line=0u; line<alllines; line++) free (input[line]);
    free (input);
    free (start);
    free (end);
    free (duration);
    return (EXIT_FAILURE);
  }

  // Synchronize all start timestamps with an affine transformation. Durations are
  // deliberately preserved rather than scaled. Round transformed starts to the
  // nearest millisecond instead of truncating fractional milliseconds.
  ratio = (long double) newspan / (long double) oldspan;
  for (i=0u; i<nsubs; i++) {
    scaled = (long double) newfirstms + (((long double) start[i].totalms - (long double) oldfirstms) * ratio);

    if ((scaled < 0.0L) || (scaled > (long double) INT64_MAX)) {
      fprintf (stderr, "ERROR: Synchronization produces an out-of-range start timestamp for subtitle %zu.\n", i + 1u);
      for (line=0u; line<alllines; line++) free (input[line]);
      free (input);
      free (start);
      free (end);
      free (duration);
      return (EXIT_FAILURE);
    }

    start[i].totalms = (int64_t) (scaled + 0.5L);

    if (start[i].totalms > (INT64_MAX - duration[i])) {
      fprintf (stderr, "ERROR: Synchronization produces an out-of-range end timestamp for subtitle %zu.\n", i + 1u);
      for (line=0u; line<alllines; line++) free (input[line]);
      free (input);
      free (start);
      free (end);
      free (duration);
      return (EXIT_FAILURE);
    }

    end[i].totalms = start[i].totalms + duration[i];

    if ((mstotime (&start[i]) != EXIT_SUCCESS) || (mstotime (&end[i]) != EXIT_SUCCESS)) {
      fprintf (stderr, "ERROR: Synchronization produces a timestamp too large to represent for subtitle %zu.\n", i + 1u);
      for (line=0u; line<alllines; line++) free (input[line]);
      free (input);
      free (start);
      free (end);
      free (duration);
      return (EXIT_FAILURE);
    }
  }

  // Save synchronized subtitles.
  rc = write_output (input, nlines, nsubs, start, end, type == 0);

  // Free allocated memory.
  for (line=0u; line<alllines; line++) free (input[line]);
  free (input);
  free (start);
  free (end);
  free (duration);

  if (rc != EXIT_SUCCESS) return (EXIT_FAILURE);

  fprintf (stdout, "\n");
  return (EXIT_SUCCESS);
}

// Obtain a text string from standard input. It can include spaces.
int
inputtext (char *text) {

  int ch;
  size_t len;

  if (text == NULL) {
    fprintf (stderr, "ERROR: NULL text buffer passed to inputtext().\n");
    exit (EXIT_FAILURE);
  }

  if (fgets (text, MAXLEN, stdin) == NULL) {
    if (ferror (stdin)) {
      fprintf (stderr, "ERROR: Unable to read text from standard input.\n");
    } else {
      fprintf (stderr, "ERROR: Unexpected end of standard input.\n");
    }
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

  // If the buffer is full, determine whether the input was exactly MAXLEN - 1
  // characters or was genuinely too long.
  if (len == (size_t) (MAXLEN - 1)) {
    ch = getchar ();

    // Exactly MAXLEN - 1 characters followed by newline or EOF.
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

    fprintf (stderr, "ERROR: Input text is too long; maximum is %d characters.\n", MAXLEN - 1);
    exit (EXIT_FAILURE);
  }

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

// Detect a Byte Order Mark at the beginning of a byte sequence. If more than
// one BOM is a prefix of another, return the longest complete match.
int
byteordermark (const uint8_t *text, size_t nbytes, const BOM *list) {

  int type, best;
  size_t bestlen;

  if ((text == NULL) || (list == NULL)) return (-1);

  best = -1;
  bestlen = 0u;

  for (type=0; type<MAXBOM; type++) {
    if ((list[type].len <= nbytes) && (list[type].len > bestlen) && (memcmp (text, list[type].sequence, list[type].len) == 0)) {
      best = type;
      bestlen = list[type].len;
    }
  }

  return (best);
}

// Extract and parse start and end timestamps from a SubRip timestamp line.
// Two-digit millisecond fields are accepted for compatibility with malformed
// files encountered by the original program; output is normalized to 3 digits.
int
extract_time (const char *text, TIME *start, TIME *end) {

  const char *arrow, *rightend;
  size_t leftlen, rightlen;
  char left[13], right[13];

  if ((text == NULL) || (start == NULL) || (end == NULL)) {
    fprintf (stderr, "ERROR: Invalid argument passed to extract_time().\n");
    exit (EXIT_FAILURE);
  }

  arrow = strstr (text, " --> ");
  if (arrow == NULL) {
    fprintf (stderr, "ERROR: Timestamp is malformed.\n       %s", text);
    exit (EXIT_FAILURE);
  }

  if (strstr (arrow + 5, " --> ") != NULL) {
    fprintf (stderr, "ERROR: Timestamp contains more than one separator.\n       %s", text);
    exit (EXIT_FAILURE);
  }

  leftlen = (size_t) (arrow - text);
  rightend = text + strlen (text);
  if ((rightend > (arrow + 5)) && (rightend[-1] == '\n')) rightend--;
  rightlen = (size_t) (rightend - (arrow + 5));

  if (((leftlen != 11u) && (leftlen != 12u)) ||
      ((rightlen != 11u) && (rightlen != 12u))) {
    fprintf (stderr, "ERROR: Timestamp is malformed.\n       %s", text);
    exit (EXIT_FAILURE);
  }

  memcpy (left, text, leftlen);
  left[leftlen] = '\0';
  memcpy (right, arrow + 5, rightlen);
  right[rightlen] = '\0';

  parsetimestamp (left, start);
  parsetimestamp (right, end);

  return (EXIT_SUCCESS);
}

// Parse timestamp into TIME struct, and also return total time in milliseconds.
// Accept HH:MM:SS,MM and HH:MM:SS,MMM.
int
parsetimestamp (const char *timestamp, TIME *time) {

  size_t len, i;
  int msdigits;

  if ((timestamp == NULL) || (time == NULL)) {
    fprintf (stderr, "ERROR: Invalid argument passed to parsetimestamp().\n");
    exit (EXIT_FAILURE);
  }

  len = strlen (timestamp);
  if ((len != 11u) && (len != 12u)) {
    fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  if ((timestamp[2] != ':') || (timestamp[5] != ':') || (timestamp[8] != ',')) {
    fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  for (i=0u; i<len; i++) {
    if ((i == 2u) || (i == 5u) || (i == 8u)) continue;
    if ((timestamp[i] < '0') || (timestamp[i] > '9')) {
      fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
      exit (EXIT_FAILURE);
    }
  }

  time->h = ((timestamp[0] - '0') * 10) + (timestamp[1] - '0');
  time->m = ((timestamp[3] - '0') * 10) + (timestamp[4] - '0');
  time->s = ((timestamp[6] - '0') * 10) + (timestamp[7] - '0');

  msdigits = (int) (len - 9u);
  time->ms = ((timestamp[9] - '0') * 10) + (timestamp[10] - '0');
  if (msdigits == 3) {
    time->ms = (time->ms * 10) + (timestamp[11] - '0');
  }

  if ((time->m > 59) || (time->s > 59) || (time->ms > 999)) {
    fprintf (stderr, "ERROR: Timestamp is out of range: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  timetoms (time);
  return (EXIT_SUCCESS);
}

// Calculate totalms from h, m, s, ms in TIME struct.
int
timetoms (TIME *time) {

  if (time == NULL) return (EXIT_FAILURE);

  time->totalms = ((int64_t) time->h * 60LL * 60LL * 1000LL);
  time->totalms += ((int64_t) time->m * 60LL * 1000LL);
  time->totalms += ((int64_t) time->s * 1000LL);
  time->totalms += (int64_t) time->ms;

  return (EXIT_SUCCESS);
}

// Calculate h, m, s, ms from totalms in TIME struct.
int
mstotime (TIME *time) {

  int64_t totalms, hours;

  if ((time == NULL) || (time->totalms < 0)) return (EXIT_FAILURE);

  totalms = time->totalms;
  hours = totalms / (60LL * 60LL * 1000LL);
  if (hours > (int64_t) INT_MAX) return (EXIT_FAILURE);

  time->h = (int) hours;
  totalms -= hours * 60LL * 60LL * 1000LL;

  time->m = (int) (totalms / (60LL * 1000LL));
  totalms -= (int64_t) time->m * 60LL * 1000LL;

  time->s = (int) (totalms / 1000LL);
  totalms -= (int64_t) time->s * 1000LL;

  time->ms = (int) totalms;

  return (EXIT_SUCCESS);
}

// Return non-zero only for the blank line that separates SubRip subtitles.
int
is_blank_line (const char *line) {
  return ((line != NULL) && (line[0] == '\n') && (line[1] == '\0'));
}

// Check that a subtitle-number line contains only decimal digits followed by LF.
int
valid_subtitle_number (const char *line) {

  size_t i;

  if ((line == NULL) || (line[0] == '\0') || (line[0] == '\n')) return (0);

  i = 0u;
  while ((line[i] != '\0') && (line[i] != '\n')) {
    if ((line[i] < '0') || (line[i] > '9')) return (0);
    i++;
  }

  return ((i > 0u) && (line[i] == '\n') && (line[i + 1u] == '\0'));
}

// Write synchronized subtitles to out.srt. On an output error, remove a partial
// file so a failed run does not leave something that appears usable.
int
write_output (char **input, size_t nlines, size_t nsubs, const TIME *start,
              const TIME *end, int write_bom) {

  FILE *fo;
  size_t sub, line;
  int failed;

  errno = 0;
  fo = fopen ("out.srt", "wx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.srt.\n");
    }
    return (EXIT_FAILURE);
  }

  failed = 0;

  if (write_bom && (fwrite (bom[0].sequence, sizeof (*bom[0].sequence), bom[0].len, fo) != bom[0].len)) {
    failed = 1;
  }

  line = 0u;
  for (sub=0u; (sub<nsubs) && !failed; sub++) {
    if (fprintf (fo, "%zu\n", sub + 1u) < 0) {
      failed = 1;
      break;
    }
    line++;  // Skip original subtitle number.

    if (fprintf (fo, "%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms) < 0) {
      failed = 1;
      break;
    }
    line++;  // Skip original timestamp line.

    while ((line < nlines) && !is_blank_line (input[line])) {
      if (fputs (input[line], fo) == EOF) {
        failed = 1;
        break;
      }
      line++;
    }

    if (!failed && (fputc ('\n', fo) == EOF)) failed = 1;
    if (line < nlines) line++;  // Skip original subtitle separator.
  }

  if (fclose (fo) != 0) failed = 1;

  if (failed) {
    fprintf (stderr, "ERROR: Unable to write complete output file out.srt.\n");
    if (remove ("out.srt") != 0) {
      fprintf (stderr, "WARNING: Unable to remove incomplete output file out.srt.\n");
    }
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Allocate memory for an array of chars.
char *
allocate_strmem (size_t len) {

  char *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate a zero-length array in allocate_strmem().\n");
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
    fprintf (stderr, "ERROR: Cannot allocate a zero-length array in allocate_strmemp().\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmemp().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate memory for an array of int64_t.
int64_t *
allocate_int64mem (size_t len) {

  int64_t *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate a zero-length array in allocate_int64mem().\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_int64mem().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate memory for an array of TIME structs.
TIME *
allocate_timemem (size_t len) {

  TIME *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate a zero-length array in allocate_timemem().\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_timemem().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}
