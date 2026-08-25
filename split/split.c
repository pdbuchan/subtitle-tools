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

// split.c - Read an existing SubRip (.srt) file and split each subtitle into two identical subs.
//           This program is used to create srt files for testing combine.c.

// gcc -Wall split.c -o split

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

// Definition of structs
typedef struct {
  int len;
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
int byteordermark (const char *, const BOM *);
int extract_time (char *, TIME *, TIME *);
int parsetimestamp (char *, TIME *);
int timetoms (TIME *);
int mstotime (TIME *);
static void *allocate_mem (size_t, size_t, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
TIME *allocate_timemem (size_t);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters in a physical input line
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, c, type, alllines, nlines, line, nsubs, sub, status;
  size_t used, add, need;
  char *temp, **input, **text, *newtext;
  const char *filename;
  TIME *start, *end, mid;
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

  // Process the command line arguments, if any.
  if (argc == 2) {
    filename = argv[1];
  } else {
    fprintf (stdout, "\nUsage: ./split inputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  temp = allocate_strmem (MAXLEN);

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file.
  fi = fopen (filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
    free (temp);
    return (EXIT_FAILURE);
  }

  // Count lines of input SubRip file and handle every readline() status.
  alllines = 0;
  for (;;) {
    status = readline (fi, temp, MAXLEN);
    if (status == -1) break;
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i does not fit in the %i-byte input buffer.\n", alllines + 1, MAXLEN);
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
    alllines++;
  }

  if (alllines == 0) {
    fprintf (stderr, "ERROR: Input SubRip file %s is empty.\n", filename);
    fclose (fi);
    free (temp);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "\n%i lines found including any excess trailing line-feeds.\n", alllines);

  if (fseek (fi, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind input SubRip file %s.\n", filename);
    fclose (fi);
    free (temp);
    return (EXIT_FAILURE);
  }
  clearerr (fi);

  // Allocate memory for array to hold input file.
  input = allocate_strmemp (alllines);
  for (line=0; line<alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0; line<alllines; line++) {
    status = readline (fi, input[line], MAXLEN);
    if (status == -1) {
      fprintf (stderr, "ERROR: Unexpected end of input while reading line %i from %s.\n", line + 1, filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i does not fit in the %i-byte input buffer.\n", line + 1, MAXLEN);
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read line %i from input SubRip file %s.\n", line + 1, filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Detect any Byte Order Mark (BOM) before parsing SRT structure.
  type = byteordermark (input[0], bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    // This program parses SRT syntax byte-by-byte. UTF-16, UTF-32, and the
    // other BOM-marked encodings listed above require character decoding first.
    if (type != 0) {
      fprintf (stderr, "ERROR: This program can directly parse only UTF-8 or byte-compatible text input.\n");
      fprintf (stderr, "       Convert %s input to UTF-8 before splitting it.\n", bom[type].name);
      return (EXIT_FAILURE);
    }

    // Remove the UTF-8 BOM from the first subtitle-number line. It will be
    // written explicitly to the output file below.
    memmove (input[0], &input[0][bom[type].len], strlen (&input[0][bom[type].len]) + 1u);
  }

  // Remove excess line-feeds at end of array input, retaining one blank line
  // to close the final subtitle.
  nlines = alllines;
  for (line=alllines; line>1; line--) {
    if ((input[line - 1][0] == '\n') && (input[line - 2][0] == '\n')) {
      nlines--;
    } else {
      break;
    }
  }

  if ((nlines < 1) || (input[nlines - 1][0] != '\n')) {
    fprintf (stderr, "ERROR: Last subtitle was not closed with a blank line.\n");
    return (EXIT_FAILURE);
  }
  if (input[0][0] == '\n') {
    fprintf (stderr, "ERROR: Input SubRip file begins with a blank line.\n");
    return (EXIT_FAILURE);
  }

  // Reject extra blank lines between subtitles. Otherwise an empty block can
  // be counted as a subtitle and later cause invalid indexing.
  for (line=1; line<nlines; line++) {
    if ((input[line][0] == '\n') && (input[line - 1][0] == '\n')) {
      fprintf (stderr, "ERROR: More than one blank line occurs between subtitles near line %i.\n", line + 1);
      return (EXIT_FAILURE);
    }
  }

  fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);

  // Each retained blank line closes one subtitle.
  nsubs = 0;
  for (line=0; line<nlines; line++) {
    if (input[line][0] == '\n') nsubs++;
  }
  if (nsubs < 1) {
    fprintf (stderr, "ERROR: No subtitles found in input SubRip file.\n");
    return (EXIT_FAILURE);
  }
  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Allocate memory for parsed subtitle data.
  start = allocate_timemem (nsubs);
  end = allocate_timemem (nsubs);
  text = allocate_strmemp (nsubs);
  for (i=0; i<nsubs; i++) {
    text[i] = allocate_strmem (1);
  }

  // Parse all subtitles and accumulate their text safely.
  line = 0;
  for (sub=0; sub<nsubs; sub++) {

    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Subtitle %i has no subtitle-number line.\n", sub + 1);
      return (EXIT_FAILURE);
    }

    // Ignore the existing subtitle number; split renumbers its output.
    line++;

    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Subtitle %i has no timestamp line.\n", sub + 1);
      return (EXIT_FAILURE);
    }

    extract_time (input[line], &start[sub], &end[sub]);
    if (end[sub].totalms <= start[sub].totalms) {
      fprintf (stderr, "ERROR: Subtitle %i has non-chronological or identical start and end times.\n", sub + 1);
      return (EXIT_FAILURE);
    }
    if ((end[sub].totalms - start[sub].totalms) < 2) {
      fprintf (stderr, "ERROR: Subtitle %i is shorter than 2 ms and cannot be split into two positive-duration subtitles.\n", sub + 1);
      return (EXIT_FAILURE);
    }
    line++;

    while ((line < nlines) && (input[line][0] != '\n')) {
      used = strlen (text[sub]);
      add = strlen (input[line]);
      if (add > (SIZE_MAX - used - 1u)) {
        fprintf (stderr, "ERROR: Subtitle %i text is too large to store.\n", sub + 1);
        return (EXIT_FAILURE);
      }
      need = used + add + 1u;
      newtext = realloc (text[sub], need);
      if (newtext == NULL) {
        fprintf (stderr, "ERROR: Unable to allocate memory for subtitle %i text.\n", sub + 1);
        return (EXIT_FAILURE);
      }
      text[sub] = newtext;
      memcpy (&text[sub][used], input[line], add + 1u);
      line++;
    }

    if ((line >= nlines) || (input[line][0] != '\n')) {
      fprintf (stderr, "ERROR: Subtitle %i is not closed with a blank line.\n", sub + 1);
      return (EXIT_FAILURE);
    }

    line++;  // Skip blank line closing this subtitle.
  }

  if (line != nlines) {
    fprintf (stderr, "ERROR: Unexpected data remains after the final subtitle.\n");
    return (EXIT_FAILURE);
  }

  // Open output file without overwriting an existing file.
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

  // Write UTF-8 BOM to output file if one was present in input.
  if (type == 0) {
    if (fwrite (bom[type].sequence, 1u, (size_t) bom[type].len, fo) != (size_t) bom[type].len) {
      fprintf (stderr, "ERROR: Unable to write Byte Order Mark to out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }
  }

  // Split each subtitle at an integer-millisecond midpoint and write two
  // adjacent subtitles with identical text.
  c = 1;
  for (sub=0; sub<nsubs; sub++) {

    // Avoid adding two potentially large totals; this form is overflow-safe
    // for valid non-negative SRT timestamps.
    mid.totalms = start[sub].totalms + ((end[sub].totalms - start[sub].totalms) / 2);
    if (mstotime (&mid) == EXIT_FAILURE) {
      fprintf (stderr, "ERROR: Unable to represent midpoint for subtitle %i.\n", sub + 1);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }

    // First half.
    fprintf (fo, "%i\n", c++);
    fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, mid.h, mid.m, mid.s, mid.ms);
    fprintf (fo, "%s\n", text[sub]);

    // Second half.
    fprintf (fo, "%i\n", c++);
    fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", mid.h, mid.m, mid.s, mid.ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);
    fprintf (fo, "%s\n", text[sub]);

    if (ferror (fo)) {
      fprintf (stderr, "ERROR: Unable to write output file out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }
  }

  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt after writing.\n");
    remove ("out.srt");
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "%i subtitles written.\n\n", c - 1);

  // Free allocated memory.
  free (temp);
  for (line = 0; line < alllines; line++) {
    free (input[line]);
  }
  free (input);
  free (start);
  free (end);
  for (i = 0; i < nsubs; i++) {
    free (text[i]);
  }
  free (text);

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
  // longer ones (for example, UTF-16 LE is a prefix of UTF-32 LE).
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

// Extract and parse start and end timestamps.
// Perform some basic format checks.
int
extract_time (char *text, TIME *start, TIME *end) {

  size_t start_len, end_len, remaining;
  char *temp, *arrow, *endtext;

  if ((text == NULL) || (start == NULL) || (end == NULL)) {
    return (EXIT_FAILURE);
  }

  // Locate the separator instead of assuming a fixed offset. This preserves
  // support for the occasionally encountered two-digit millisecond form.
  arrow = strstr (text, " --> ");
  if (arrow == NULL) {
    fprintf (stderr, "ERROR: Timestamp is malformed.\n");
    fprintf (stderr, "       %s", text);
    exit (EXIT_FAILURE);
  }

  start_len = (size_t) (arrow - text);
  if ((start_len != 11u) && (start_len != 12u)) {
    fprintf (stderr, "ERROR: Starting timestamp is malformed.\n");
    fprintf (stderr, "       %s", text);
    exit (EXIT_FAILURE);
  }
  if (start_len == 11u) {
    fprintf (stderr, "WARNING: Starting timestamp uses two millisecond digits.\n");
    fprintf (stderr, "         %s", text);
  }

  temp = allocate_strmem (13);
  memcpy (temp, text, start_len);
  temp[start_len] = '\0';
  parsetimestamp (temp, start);

  endtext = arrow + 5;
  remaining = strlen (endtext);
  if (remaining < 11u) {
    fprintf (stderr, "ERROR: Ending timestamp is malformed.\n");
    fprintf (stderr, "       %s", text);
    free (temp);
    exit (EXIT_FAILURE);
  }

  if (remaining >= 12u) {
    if (isdigit ((unsigned char) endtext[11])) {
      end_len = 12u;
    } else if ((endtext[11] == '\n') || (endtext[11] == ' ')) {
      end_len = 11u;
      fprintf (stderr, "WARNING: Ending timestamp uses two millisecond digits.\n");
      fprintf (stderr, "         %s", text);
    } else {
      fprintf (stderr, "ERROR: Ending timestamp is malformed.\n");
      fprintf (stderr, "       %s", text);
      free (temp);
      exit (EXIT_FAILURE);
    }
  } else {
    end_len = 11u;
    fprintf (stderr, "WARNING: Ending timestamp uses two millisecond digits.\n");
    fprintf (stderr, "         %s", text);
  }

  memset (temp, 0, 13u * sizeof (char));
  memcpy (temp, endtext, end_len);
  temp[end_len] = '\0';
  parsetimestamp (temp, end);

  free (temp);
  return (EXIT_SUCCESS);
}

// Parse timestamp into TIME struct, and also return total time in milliseconds.
int
parsetimestamp (char *timestamp, TIME *time) {

  size_t len;
  int i;
  const int loc[8] = {0, 1, 3, 4, 6, 7, 9, 10};

  if ((timestamp == NULL) || (time == NULL)) return (EXIT_FAILURE);

  len = strlen (timestamp);
  if ((len != 11u) && (len != 12u)) {
    fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  if ((timestamp[2] != ':') || (timestamp[5] != ':') || (timestamp[8] != ',')) {
    fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  for (i=0; i<8; i++) {
    if (!isdigit ((unsigned char) timestamp[loc[i]])) {
      fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
      exit (EXIT_FAILURE);
    }
  }
  if ((len == 12u) && !isdigit ((unsigned char) timestamp[11])) {
    fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  time->h = ((timestamp[0] - '0') * 10) + (timestamp[1] - '0');
  time->m = ((timestamp[3] - '0') * 10) + (timestamp[4] - '0');
  time->s = ((timestamp[6] - '0') * 10) + (timestamp[7] - '0');
  time->ms = ((timestamp[9] - '0') * 10) + (timestamp[10] - '0');
  if (len == 12u) time->ms = (time->ms * 10) + (timestamp[11] - '0');

  if ((time->m < 0) || (time->m > 59)) {
    fprintf (stderr, "ERROR: Minutes are outside valid range 00-59: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }
  if ((time->s < 0) || (time->s > 59)) {
    fprintf (stderr, "ERROR: Seconds are outside valid range 00-59: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  return (timetoms (time));
}

// Calculate totalms from h, m, s, ms in TIME struct.
int
timetoms (TIME *time) {

  if (time == NULL) return (EXIT_FAILURE);

  time->totalms = (int64_t) time->h * 60 * 60 * 1000;
  time->totalms += (int64_t) time->m * 60 * 1000;
  time->totalms += (int64_t) time->s * 1000;
  time->totalms += (int64_t) time->ms;

  return (EXIT_SUCCESS);
}

// Calculate h, m, s, ms from totalms in TIME struct.
int
mstotime (TIME *time) {

  int64_t totalms, hours;

  if ((time == NULL) || (time->totalms < 0)) return (EXIT_FAILURE);

  totalms = time->totalms;

  hours = totalms / INT64_C (3600000);
  if (hours > INT_MAX) return (EXIT_FAILURE);
  time->h = (int) hours;
  totalms %= INT64_C (3600000);

  time->m = (int) (totalms / INT64_C (60000));
  totalms %= INT64_C (60000);

  time->s = (int) (totalms / INT64_C (1000));
  time->ms = (int) (totalms % INT64_C (1000));

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

// Allocate memory for an array of TIME structs.
TIME *
allocate_timemem (size_t len) {
  return (allocate_mem (len, sizeof (TIME), "array of TIME structs"));
}
