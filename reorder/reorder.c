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

// reorder.c - Re-order non-chronological subtitles in a SubRip (srt) by sorting on start times.
// gcc -Wall reorder.c -o reorder

// Usage: ./reorder filename.srt
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>

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
int readline (FILE *, char *, int);
int byteordermark (const char *, const BOM *, int);
int extract_time (const char *, TIME *, TIME *);
int parsetimestamp (const char *, TIME *);
int timetoms (TIME *);
int heapsort (int, int64_t *, int *);
int heapify (int64_t *, int *, int, int);
int swap_int (int *, int *);
int swap_int64 (int64_t *, int64_t *);
int append_text (char **, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
int *allocate_intmem (size_t);
int64_t *allocate_int64mem (size_t);
TIME *allocate_timemem (size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per physical input line

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

static const BOM boms[] = {
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

#define MAXBOM ((int) (sizeof (boms) / sizeof (boms[0])))

int
main (int argc, char **argv) {

  int i, type, status, alllines, nlines, line, nsubs, sub;
  int *aux;
  int64_t *startms;
  char **input, *temp, **text;
  TIME *start, *end;
  FILE *fi, *fo;
  const char *filename;

  // Process the command line arguments, if any.
  if (argc != 2) {
    fprintf (stdout, "\nUsage: ./reorder inputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }
  filename = argv[1];

  temp = allocate_strmem (MAXLEN);

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file.
  fi = fopen (filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input SubRip file %s.\n", filename);
    free (temp);
    return (EXIT_FAILURE);
  }

  // Count lines of input SubRip file. readline() discards the remainder of an
  // overlong physical line before returning -2, so report it immediately.
  alllines = 0;
  for (;;) {
    status = readline (fi, temp, MAXLEN);
    if (status == -1) break;
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in input SubRip file does not fit in the %i-byte input buffer.\n", alllines + 1, MAXLEN);
      fclose (fi);
      free (temp);
      return (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
      fclose (fi);
      free (temp);
      return (EXIT_FAILURE);
    }
    if (alllines == INT_MAX) {
      fprintf (stderr, "ERROR: Input SubRip file contains too many lines.\n");
      fclose (fi);
      free (temp);
      return (EXIT_FAILURE);
    }
    alllines++;
  }

  fprintf (stdout, "\n%i lines found including any excess trailing line-feeds.\n", alllines);

  if (alllines == 0) {
    fprintf (stderr, "ERROR: Input SubRip file is empty.\n");
    fclose (fi);
    free (temp);
    return (EXIT_FAILURE);
  }

  if (fseek (fi, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind input SubRip file %s.\n", filename);
    fclose (fi);
    free (temp);
    return (EXIT_FAILURE);
  }

  // Allocate memory for array to hold input file.
  input = allocate_strmemp ((size_t) alllines);
  for (line=0; line<alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0; line<alllines; line++) {
    status = readline (fi, input[line], MAXLEN);
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in input SubRip file does not fit in the %i-byte input buffer.\n", line + 1, MAXLEN);
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read line %i from input SubRip file %s.\n", line + 1, filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (status == -1) {
      fprintf (stderr, "ERROR: Unexpected EOF while reading line %i from input SubRip file %s.\n", line + 1, filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Remove excess blank lines at the end while retaining the one blank line
  // that closes the final subtitle.
  nlines = alllines;
  for (line=alllines; line>1; line--) {
    if ((input[line - 1][0] == '\n') && (input[line - 2][0] == '\n')) {
      nlines--;
    } else {
      break;
    }
  }
  fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);

  // Detect any Byte Order Mark (BOM) at beginning of first line. Use the
  // longest complete match because UTF-16LE is a prefix of UTF-32LE.
  type = byteordermark (input[0], boms, MAXBOM);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", boms[type].name);
    if (type != 0) {
      fprintf (stderr, "ERROR: This program parses SubRip syntax as single-byte/UTF-8 text and cannot safely process %s input.\n", boms[type].name);
      return (EXIT_FAILURE);
    }
  }

  // Validate the basic SubRip block structure and count subtitles. Existing
  // subtitle numbers are intentionally ignored because output is renumbered.
  nsubs = 0;
  line = 0;
  while (line < nlines) {
    if (input[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at input line %i.\n", line + 1);
      return (EXIT_FAILURE);
    }

    // Subtitle number/identifier line.
    line++;

    // Timestamp line must exist and cannot be blank.
    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Missing timestamp line for subtitle %i.\n", nsubs + 1);
      return (EXIT_FAILURE);
    }
    line++;

    // Subtitle text may contain zero or more lines, but the block must end in
    // a blank line so that subsequent subtitle boundaries are unambiguous.
    while ((line < nlines) && (input[line][0] != '\n')) line++;
    if (line >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %i is not closed by a blank line.\n", nsubs + 1);
      return (EXIT_FAILURE);
    }
    line++;

    if (nsubs == INT_MAX) {
      fprintf (stderr, "ERROR: Input SubRip file contains too many subtitles.\n");
      return (EXIT_FAILURE);
    }
    nsubs++;
  }

  if (nsubs < 1) {
    fprintf (stderr, "ERROR: No subtitles found.\n");
    return (EXIT_FAILURE);
  }
  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  start = allocate_timemem ((size_t) nsubs);
  end = allocate_timemem ((size_t) nsubs);
  text = allocate_strmemp ((size_t) nsubs);
  startms = allocate_int64mem ((size_t) nsubs);
  aux = allocate_intmem ((size_t) nsubs);

  for (sub=0; sub<nsubs; sub++) {
    text[sub] = allocate_strmem (1u);
  }

  // Parse each subtitle. Store the complete text block in one dynamically
  // sized string so subtitles are not limited to an arbitrary number of lines.
  line = 0;
  for (sub=0; sub<nsubs; sub++) {

    // Ignore existing subtitle number/identifier.
    line++;

    if (extract_time (input[line], &start[sub], &end[sub]) == EXIT_FAILURE) {
      fprintf (stderr, "ERROR: Invalid timestamp on input line %i.\n", line + 1);
      return (EXIT_FAILURE);
    }
    if (end[sub].totalms < start[sub].totalms) {
      fprintf (stderr, "ERROR: End timestamp precedes start timestamp for subtitle %i.\n", sub + 1);
      return (EXIT_FAILURE);
    }
    line++;

    startms[sub] = start[sub].totalms;
    aux[sub] = sub;

    while ((line < nlines) && (input[line][0] != '\n')) {
      if (append_text (&text[sub], input[line]) == EXIT_FAILURE) {
        fprintf (stderr, "ERROR: Unable to store text for subtitle %i.\n", sub + 1);
        return (EXIT_FAILURE);
      }
      line++;
    }

    // Skip the blank line that closes this subtitle. Structure was validated
    // above, so line is guaranteed to be within bounds here.
    line++;
  }

  // Sort start times and the parallel original-subtitle index. Equal start
  // times retain original order by using the original index as a tie-breaker.
  heapsort (nsubs, startms, aux);

  // Create output file without overwriting an existing out.srt.
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

  // Preserve a UTF-8 BOM when one was present in the input.
  if (type == 0) {
    if (fwrite (boms[type].sequence, 1u, boms[type].len, fo) != boms[type].len) {
      fprintf (stderr, "ERROR: Unable to write UTF-8 BOM to out.srt.\n");
      fclose (fo);
      return (EXIT_FAILURE);
    }
  }

  // Write subtitles in ascending start-time order and renumber from 1.
  for (sub=0; sub<nsubs; sub++) {
    i = aux[sub];

    if (fprintf (fo, "%i\n", sub + 1) < 0 ||
        fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[i].h, start[i].m, start[i].s, start[i].ms, end[i].h, end[i].m, end[i].s, end[i].ms) < 0 ||
        fputs (text[i], fo) == EOF ||
        fputc ('\n', fo) == EOF) {
      fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub + 1);
      fclose (fo);
      return (EXIT_FAILURE);
    }
  }

  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt.\n");
    return (EXIT_FAILURE);
  }

  // Free allocated memory.
  free (temp);
  free (start);
  free (end);
  free (startms);
  free (aux);
  for (sub = 0; sub < nsubs; sub++) free (text[sub]);
  free (text);
  for (line = 0; line < alllines; line++) free (input[line]);
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

// Detect Byte Order Mark (BOM), if it exists, at beginning of text.
// Return the index of the longest matching BOM, or -1 if none is detected.
int
byteordermark (const char *text, const BOM *bom, int nbom) {

  int type, best;
  size_t i, bestlen;

  if ((text == NULL) || (bom == NULL) || (nbom <= 0)) return (-1);

  best = -1;
  bestlen = 0u;

  for (type=0; type<nbom; type++) {
    for (i=0u; i<bom[type].len; i++) {
      if ((uint8_t) text[i] != bom[type].sequence[i]) break;
    }
    if ((i == bom[type].len) && (bom[type].len > bestlen)) {
      best = type;
      bestlen = bom[type].len;
    }
  }

  return (best);
}

// Extract and parse start and end timestamps.
int
extract_time (const char *text, TIME *start, TIME *end) {

  size_t len;

  if ((text == NULL) || (start == NULL) || (end == NULL)) return (EXIT_FAILURE);

  len = strlen (text);
  if ((len != 29u) && !((len == 30u) && (text[29] == '\n'))) {
    fprintf (stderr, "ERROR: Timestamp is malformed.\n       %s", text);
    if ((len == 0u) || (text[len - 1u] != '\n')) fputc ('\n', stderr);
    return (EXIT_FAILURE);
  }

  if (strncmp (&text[12], " --> ", 5u) != 0) {
    fprintf (stderr, "ERROR: Timestamp is malformed.\n       %s", text);
    if (text[len - 1u] != '\n') fputc ('\n', stderr);
    return (EXIT_FAILURE);
  }

  if ((parsetimestamp (text, start) == EXIT_FAILURE) || (parsetimestamp (&text[17], end) == EXIT_FAILURE)) {
    fprintf (stderr, "ERROR: Timestamp is malformed.\n       %s", text);
    if (text[len - 1u] != '\n') fputc ('\n', stderr);
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Parse a timestamp in hh:mm:ss,mmm form and calculate total milliseconds.
int
parsetimestamp (const char *timestamp, TIME *time) {

  static const int digitpos[9] = {0, 1, 3, 4, 6, 7, 9, 10, 11};
  int i;

  if ((timestamp == NULL) || (time == NULL)) return (EXIT_FAILURE);

  if ((timestamp[2] != ':') || (timestamp[5] != ':') || (timestamp[8] != ',')) {
    return (EXIT_FAILURE);
  }

  for (i=0; i<9; i++) {
    if ((timestamp[digitpos[i]] < '0') || (timestamp[digitpos[i]] > '9')) {
      return (EXIT_FAILURE);
    }
  }

  time->h = ((timestamp[0] - '0') * 10) + (timestamp[1] - '0');
  time->m = ((timestamp[3] - '0') * 10) + (timestamp[4] - '0');
  time->s = ((timestamp[6] - '0') * 10) + (timestamp[7] - '0');
  time->ms = ((timestamp[9] - '0') * 100) + ((timestamp[10] - '0') * 10) + (timestamp[11] - '0');

  if ((time->m > 59) || (time->s > 59)) return (EXIT_FAILURE);

  return (timetoms (time));
}

// Calculate totalms from h, m, s, ms in TIME struct using 64-bit arithmetic.
int
timetoms (TIME *time) {

  if (time == NULL) return (EXIT_FAILURE);

  time->totalms = ((int64_t) time->h * 60LL * 60LL * 1000LL) +
                  ((int64_t) time->m * 60LL * 1000LL) +
                  ((int64_t) time->s * 1000LL) +
                  (int64_t) time->ms;

  return (EXIT_SUCCESS);
}

// Heap sort in-place. The parallel aux array contains original subtitle
// indexes. Equal start times are ordered by the original index.
int
heapsort (int n, int64_t *data, int *aux) {

  int i;

  if ((n < 0) || (data == NULL) || (aux == NULL)) return (EXIT_FAILURE);

  for (i=(n / 2) - 1; i>=0; i--) {
    heapify (data, aux, n, i);
  }

  for (i=n - 1; i>0; i--) {
    swap_int64 (&data[0], &data[i]);
    swap_int (&aux[0], &aux[i]);
    heapify (data, aux, i, 0);
  }

  return (EXIT_SUCCESS);
}

// Heapify a subtree rooted at index i. Use original subtitle index as a
// tie-breaker so equal start times retain their input order after sorting.
int
heapify (int64_t *data, int *aux, int n, int i) {

  int largest, left, right;

  if ((data == NULL) || (aux == NULL) || (n < 0) || (i < 0)) return (EXIT_FAILURE);

  for (;;) {
    largest = i;
    left = (2 * i) + 1;
    right = (2 * i) + 2;

    if ((left < n) && ((data[left] > data[largest]) || ((data[left] == data[largest]) && (aux[left] > aux[largest])))) {
      largest = left;
    }

    if ((right < n) && ((data[right] > data[largest]) || ((data[right] == data[largest]) && (aux[right] > aux[largest])))) {
      largest = right;
    }

    if (largest == i) break;

    swap_int64 (&data[i], &data[largest]);
    swap_int (&aux[i], &aux[largest]);
    i = largest;
  }

  return (EXIT_SUCCESS);
}

// Swap integer values between two variables.
int
swap_int (int *a, int *b) {

  int temp;

  if ((a == NULL) || (b == NULL)) return (EXIT_FAILURE);

  temp = *a;
  *a = *b;
  *b = temp;

  return (EXIT_SUCCESS);
}

// Swap 64-bit integer values between two variables.
int
swap_int64 (int64_t *a, int64_t *b) {

  int64_t temp;

  if ((a == NULL) || (b == NULL)) return (EXIT_FAILURE);

  temp = *a;
  *a = *b;
  *b = temp;

  return (EXIT_SUCCESS);
}

// Append a line to a dynamically sized subtitle-text string.
int
append_text (char **dest, const char *src) {

  size_t oldlen, addlen;
  char *tmp;

  if ((dest == NULL) || (*dest == NULL) || (src == NULL)) return (EXIT_FAILURE);

  oldlen = strlen (*dest);
  addlen = strlen (src);

  if (oldlen > (SIZE_MAX - addlen - 1u)) return (EXIT_FAILURE);

  tmp = realloc (*dest, oldlen + addlen + 1u);
  if (tmp == NULL) return (EXIT_FAILURE);

  memcpy (tmp + oldlen, src, addlen + 1u);
  *dest = tmp;

  return (EXIT_SUCCESS);
}

// Allocate zero-initialized memory for an array of chars.
char *
allocate_strmem (size_t len) {

  char *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate a zero-length character array.\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for character array.\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate zero-initialized memory for an array of char pointers.
char **
allocate_strmemp (size_t len) {

  char **tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate a zero-length character-pointer array.\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for character-pointer array.\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate zero-initialized memory for an array of ints.
int *
allocate_intmem (size_t len) {

  int *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate a zero-length integer array.\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for integer array.\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate zero-initialized memory for an array of int64_t.
int64_t *
allocate_int64mem (size_t len) {

  int64_t *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate a zero-length int64_t array.\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for int64_t array.\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}

// Allocate zero-initialized memory for an array of TIME structs.
TIME *
allocate_timemem (size_t len) {

  TIME *tmp;

  if (len == 0u) {
    fprintf (stderr, "ERROR: Cannot allocate a zero-length TIME array.\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc (len, sizeof (*tmp));
  if (tmp == NULL) {
    fprintf (stderr, "ERROR: Cannot allocate memory for TIME array.\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}
