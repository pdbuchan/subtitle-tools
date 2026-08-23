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

// combine.c - Read an existing SubRip (.srt) file, combine subtitles with identical textual content and consecutive timestamps.
//             Within each group of matching subs, take the starting time-stamp from the first subtitle and ending time-stamp from the last.
//             Write a new SubRip file.

// gcc -Wall combine.c -o combine

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

// Definition of structs
typedef struct {
  int len;
  char *name;
  uint8_t *sequence;
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
int byteordermark (char *, BOM *);
int extract_time (char*, TIME *, TIME *);
int parsetimestamp (char *, TIME *);
int timetoms (TIME *);
int seek (int *, int, int *, int);
char *allocate_strmem (int);
char **allocate_strmemp (int);
int *allocate_intmem (int);
BOM *allocate_bommem (int);
TIME *allocate_timemem (int);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters in a string
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, c, type, alllines, nlines, line, nsubs, sub, *grouplist, group, endpt;
  int status;
  size_t used, add, need;
  char *temp, *filename, **input, **text, *newtext;
  BOM *bom;
  TIME *start, *end;
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
    fprintf (stdout, "\nUsage: ./combine inputfilename.srt\n");
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

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file.
  fi = fopen (filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllines = 0;
  for (;;) {
    status = readline (fi, temp, MAXLEN);
    if (status == -1) break;
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i does not fit in the %i-byte input buffer.\n", alllines + 1, MAXLEN);
      exit (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
      exit (EXIT_FAILURE);
    }
    alllines++;
  }

  if (alllines == 0) {
    fprintf (stderr, "ERROR: Input SubRip file %s is empty.\n", filename);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "\n%i lines found including any excess trailing line-feeds.\n", alllines);
  rewind (fi);

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
      exit (EXIT_FAILURE);
    }
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i does not fit in the %i-byte input buffer.\n", line + 1, MAXLEN);
      exit (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read line %i from input SubRip file %s.\n", line + 1, filename);
      exit (EXIT_FAILURE);
    }
  }

  // Close input file.
  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Detect any Byte Order Mark (BOM) at beginning of first line.
  type = byteordermark (input[0], bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    // This program parses SRT syntax byte-by-byte. UTF-16, UTF-32, and the
    // other BOM-marked encodings listed above require character decoding first.
    if (type != 0) {
      fprintf (stderr, "ERROR: This program can directly parse only UTF-8 or byte-compatible text input.\n");
      fprintf (stderr, "       Convert %s input to UTF-8 before combining it.\n", bom[type].name);
      exit (EXIT_FAILURE);
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
    exit (EXIT_FAILURE);
  }
  if (input[0][0] == '\n') {
    fprintf (stderr, "ERROR: Input SubRip file begins with a blank line.\n");
    exit (EXIT_FAILURE);
  }

  // Reject extra blank lines between subtitles. Without this check, an empty
  // block could be counted as a subtitle and later cause invalid indexing.
  for (line=1; line<nlines; line++) {
    if ((input[line][0] == '\n') && (input[line - 1][0] == '\n')) {
      fprintf (stderr, "ERROR: More than one blank line occurs between subtitles near line %i.\n", line + 1);
      exit (EXIT_FAILURE);
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
    exit (EXIT_FAILURE);
  }
  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Allocate memory for various arrays.
  start = allocate_timemem (nsubs);
  end = allocate_timemem (nsubs);
  text = allocate_strmemp (nsubs);
  for (i=0; i<nsubs; i++) {
    text[i] = allocate_strmem (1);
  }
  grouplist = allocate_intmem (nsubs);

  // Parse all subtitles and accumulate their text safely.
  line = 0;
  for (sub=0; sub<nsubs; sub++) {

    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Subtitle %i has no subtitle-number line.\n", sub + 1);
      exit (EXIT_FAILURE);
    }

    // Skip subtitle-number line. combine renumbers subtitles in the output.
    line++;

    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Subtitle %i has no timestamp line.\n", sub + 1);
      exit (EXIT_FAILURE);
    }

    // Extract start and end times.
    extract_time (input[line], &start[sub], &end[sub]);
    if (end[sub].totalms <= start[sub].totalms) {
      fprintf (stderr, "ERROR: Subtitle %i has non-chronological or identical start and end times.\n", sub + 1);
      exit (EXIT_FAILURE);
    }
    line++;

    // Accumulate text lines until the blank line that closes this subtitle.
    // Grow the destination as needed rather than imposing a fixed total-text
    // size or risking overflow with strncat().
    while ((line < nlines) && (input[line][0] != '\n')) {
      used = strlen (text[sub]);
      add = strlen (input[line]);
      if (add > (SIZE_MAX - used - 1u)) {
        fprintf (stderr, "ERROR: Subtitle %i text is too large to store.\n", sub + 1);
        exit (EXIT_FAILURE);
      }
      need = used + add + 1u;
      newtext = realloc (text[sub], need);
      if (newtext == NULL) {
        fprintf (stderr, "ERROR: Unable to allocate memory for subtitle %i text.\n", sub + 1);
        exit (EXIT_FAILURE);
      }
      text[sub] = newtext;
      memcpy (&text[sub][used], input[line], add + 1u);
      line++;
    }

    if ((line >= nlines) || (input[line][0] != '\n')) {
      fprintf (stderr, "ERROR: Subtitle %i is not closed with a blank line.\n", sub + 1);
      exit (EXIT_FAILURE);
    }

    line++;  // Skip blank line closing this subtitle.
  }

  if (line != nlines) {
    fprintf (stderr, "ERROR: Unexpected data remains after the final subtitle.\n");
    exit (EXIT_FAILURE);
  }

  // Loop through all subs making a group list in which all matching subs are assigned the same group number.
  group = 0;  // First group number
  grouplist[0] = group;  // Assign group number to first sub.
  for (sub=1; sub<nsubs; sub++) {

    // Compare current subtitle timestamps and text with previous.
    // If current sub's start time matches previous end time and text is identical then this is a duplicate subtitle.
    if ((start[sub].totalms == end[sub - 1].totalms) && (strcmp (text[sub], text[sub - 1]) == 0)) {
      grouplist[sub] = group;

    // Unique subtitle found so start a new group.
    } else {
      group++;
      grouplist[sub] = group;
    }
  }

  // Open the output file without overwriting an existing file.
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

  // Write UTF-8 Byte Order Mark (BOM) to output file if one was detected in input.
  if (type == 0) {
    if (fwrite (bom[type].sequence, 1u, (size_t) bom[type].len, fo) != (size_t) bom[type].len) {
      fprintf (stderr, "ERROR: Unable to write Byte Order Mark to out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }
  }

  // Write subtitles to output file with any duplicates combined.
  sub = 0;  // Original subtitle number
  c = 0;  // New subtitle number
  do {

    // Find last matching subtitle endpoint.
    endpt = sub;
    if (seek (grouplist, sub, &endpt, nsubs) == EXIT_FAILURE) {
      fprintf (stderr, "ERROR: Unable to determine end of subtitle group.\n");
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }

    // Write subtitle number, timestamps, text, and closing blank line.
    fprintf (fo, "%i\n", c + 1);
    fprintf (fo, "%02i:%02i:%02i,%03i --> ", start[sub].h, start[sub].m, start[sub].s, start[sub].ms);
    fprintf (fo, "%02i:%02i:%02i,%03i\n", end[endpt].h, end[endpt].m, end[endpt].s, end[endpt].ms);
    fprintf (fo, "%s\n", text[sub]);

    if (ferror (fo)) {
      fprintf (stderr, "ERROR: Unable to write output file out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }

    sub = endpt + 1;
    c++;

  } while (sub < nsubs);

  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt after writing.\n");
    remove ("out.srt");
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "%i subtitles written.\n\n", c);

  // Free allocated memory.
  free (temp);
  free (bom);
  free (filename);
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
  free (grouplist);

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
byteordermark (char *text, BOM *bom) {

  int type, i, found, best, bestlen;

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

  // Parse the ending timestamp. Ignore any material after its 11- or 12-byte
  // timestamp field.
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

  timetoms (time);
  return (EXIT_SUCCESS);
}

// Calculate totalms from h, m, s, ms in TIME struct.
int
timetoms (TIME *time) {

  if (time == NULL) return (EXIT_FAILURE);

  time->totalms = (int64_t) time->h * 60 * 60 * 1000;
  time->totalms += (int64_t) time->m * 60 * 1000;
  time->totalms += (int64_t) time->s * 1000;
  time->totalms += time->ms;

  return (EXIT_SUCCESS);
}

// Recursively find last subtitle with same group number as starting subtitle.
int
seek (int *grouplist, int first, int *try, int max) {

  if ((grouplist == NULL) || (try == NULL) || (first < 0) ||
      (first >= max) || ((*try) < first) || ((*try) >= max)) {
    return (EXIT_FAILURE);
  }

  // Find the final adjacent subtitle in this group iteratively. The original
  // recursive implementation could exhaust the call stack for a very large
  // run of duplicate subtitles.
  while (((*try) + 1) < max && (grouplist[first] == grouplist[(*try) + 1])) {
    (*try)++;
  }

  return (EXIT_SUCCESS);
}

// Allocate memory for an array of chars.
char *
allocate_strmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (char));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of pointers to arrays of chars.
char **
allocate_strmemp (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmemp().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (char *));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmemp().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of pointers to arrays of pointers to arrays of chars.
char ***
allocate_strmempp (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmempp().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (char **));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_strmempp().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of ints.
int *
allocate_intmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_intmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (int));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_intmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of pointers to arrays of ints.
int **
allocate_intmemp (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_intmemp().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (int *));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_intmemp().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of BOM (Byte Order Mark) structs.
BOM *
allocate_bommem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_bommem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (BOM));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_bommem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of TIME structs.
TIME *
allocate_timemem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_timemem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (TIME));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_timemem().\n");
    exit (EXIT_FAILURE);
  }
}
