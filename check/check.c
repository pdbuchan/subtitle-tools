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

// check.c - Check for errors in a SubRip (srt) file and report results.

// gcc -Wall check.c -o check

// Run without command line arguments to see usage notes.
// Output: reports to stdout

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
static void *allocate_mem (size_t, size_t, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
char ***allocate_strmempp (size_t);
int *allocate_intmem (size_t);
TIME *allocate_timemem (size_t);
BOM *allocate_bommem (size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXLINES 10  // Maximum number of lines of text per subtitle
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, type, flag, alllines, nlines, line, nsubs, *number, status, sub, *ntext, nwarn;
  long parsed_number;
  char *filename, **input, *temp, ***text, *endptr;
  BOM *bom;
  TIME *start, *end;
  FILE *fi;

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
      fprintf (stderr, "\nERROR: Input filename is too long.\n");
      free (filename);
      return (EXIT_FAILURE);
    }
  
  } else {
    fprintf (stdout, "\nUsage: ./check inputfilename.srt\n");
    fprintf (stdout, "If it finds an error, fix it and re-run check until no more errors are found.\n\n");
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

  // Default to no warnings detected.
  nwarn = 0;

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file.
  fi = fopen (filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllines = 0;  // Count of lines
  for (;;) {
    status = readline (fi, temp, MAXLEN);
    if (status == -1) break;
    if (status == -2) {
      fprintf (stderr, "\nERROR: Line %i does not fit in the %i-byte input buffer.\n", alllines + 1, MAXLEN);
      exit (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "\nERROR: Unable to read input SubRip file %s.\n", filename);
      exit (EXIT_FAILURE);
    }
    alllines++;
  }

  if (alllines == 0) {
    fprintf (stderr, "\nERROR: Input SubRip file %s is empty.\n", filename);
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
      fprintf (stderr, "\nERROR: Unexpected end of input while reading line %i from %s.\n", line + 1, filename);
      exit (EXIT_FAILURE);
    }
    if (status == -2) {
      fprintf (stderr, "\nERROR: Line %i does not fit in the %i-byte input buffer.\n", line + 1, MAXLEN);
      exit (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "\nERROR: Unable to read line %i from input SubRip file %s.\n", line + 1, filename);
      exit (EXIT_FAILURE);
    }
  }  // Next line

  // Close input file.
  fclose (fi);

  // Detect any Byte Order Mark (BOM) at beginning of first line and remove it if detected.
  type = byteordermark (input[0], bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    // This program parses SRT syntax byte-by-byte. UTF-16, UTF-32, and the
    // other BOM-marked encodings listed above require character decoding first.
    if (type != 0) {
      fprintf (stderr, "ERROR: This program can directly parse only UTF-8 or byte-compatible text input.\n");
      fprintf (stderr, "       Convert %s input to UTF-8 before checking it.\n", bom[type].name);
      exit (EXIT_FAILURE);
    }

    memset (temp, 0, MAXLEN * sizeof (char));
    strncpy (temp, &input[0][bom[type].len], MAXLEN);
    memset (input[0], 0, MAXLEN * sizeof (char));
    strncpy (input[0], temp, MAXLEN);
  }

  // Remove excess line-feeds at end of array input.
  nlines = alllines;
  for (line=alllines; line>1; line--) {
    if ((input[line - 1][0] == '\n') && (input[line - 2][0] == '\n')) {
      nlines--;
    } else {
      break;
    }
  }

  // Check for final line-feed which closes last subtitle.
  if (input[nlines - 1][0] != '\n') {
    fprintf (stderr, "\nERROR: Last subtitle was not closed with a line-feed.\n\n");
    exit (EXIT_FAILURE);
  } else {
    fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);
  }

  // Check for more than one line-feed between subtitles.
  flag = 0;  // Flag indicating line-feed found
  for (line=0; line<nlines; line++) {

    // Found a line-feed.
    if (input[line][0] == '\n') {

      // If we just had a line-feed previously, throw an error.
      if (flag) {
        fprintf (stderr, "\nERROR: More than one line-feed after subtitle on line %i.\n", line + 1);
        exit (EXIT_FAILURE);
      } else {
        flag = 1;
      }

    // Found text, not line-feed.
    } else {
      flag = 0;
    }

  }  // Next line of input file

  // Count number of subtitles in SubRip file; assume at least one.
  nsubs = 0;
  for (line=0; line<nlines; line++) {

    nsubs++;

    // Advance through to next subtitle number, if there is one.
    // End of current subtitle is demarcated by a line containing only a line-feed.
    while (input[line][0] != '\n') {
      line++;
      if (line == nlines) break;
    }

  }  // Next sub

  // Allocate memory for various arrays.
  start = allocate_timemem (nsubs);
  end = allocate_timemem (nsubs);
  text = allocate_strmempp (nsubs);  // Array to hold text of subtitles
  for (sub=0; sub<nsubs; sub++) {
    text[sub] = allocate_strmemp (MAXLINES);
    for (i=0; i<MAXLINES; i++) {
      text[sub][i] = allocate_strmem (MAXLEN);
    }
  }
  ntext = allocate_intmem (nsubs);  // Number of lines of text per subtitle
  number = allocate_intmem (nsubs);

  // Loop through each line of input SubRip file.
  sub = 0;  // Subtitle index
  for (line=0; line<nlines; line++) {

    if (sub >= nsubs) {
      fprintf (stderr, "\nERROR: Internal subtitle count mismatch.\n");
      exit (EXIT_FAILURE);
    }

    // Examine subtitle number.
    if (input[line][0] == '\n') {
      fprintf (stderr, "\nERROR: Missing subtitle number on line %i.\n", line + 1);
      exit (EXIT_FAILURE);
    }
    if (!isdigit ((unsigned char) input[line][0])) {
      fprintf (stderr, "\nERROR: Subtitle number has leading non-numeric character(s).\n");
      fprintf (stderr, "\n%s", input[line]);
      exit (EXIT_FAILURE);
    }

    errno = 0;
    parsed_number = strtol (input[line], &endptr, 10);
    if ((errno == ERANGE) || (endptr == input[line]) ||
        (parsed_number < 0) || (parsed_number > INT_MAX)) {
      fprintf (stderr, "\nERROR: Cannot make integer of subtitle number.\n");
      fprintf (stderr, "\n%s", input[line]);
      exit (EXIT_FAILURE);
    }

    while ((*endptr == ' ') || (*endptr == '\t')) endptr++;
    if ((*endptr != '\n') && (*endptr != '\0')) {
      fprintf (stderr, "\nERROR: Subtitle number contains trailing non-numeric character(s).\n");
      fprintf (stderr, "\n%s", input[line]);
      exit (EXIT_FAILURE);
    }
    number[sub] = (int) parsed_number;
    line++;

    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "\nERROR: Missing timestamp for subtitle %i.\n", number[sub]);
      exit (EXIT_FAILURE);
    }

    // Check format and extract start and end times for current subtitle.
    extract_time (input[line], &start[sub], &end[sub]);

//fprintf (stderr, "Sub: %i  %02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", sub + 1, start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);

    // Move past timestamp.
    line++;

    // Count lines of text of current subtitle.
    ntext[sub] = 0;
    while (line < nlines) {

      // Copy line of text from current subtitle.
      if (input[line][0] != '\n') {
        if (ntext[sub] >= MAXLINES) {
          fprintf (stderr, "\nERROR: Subtitle %i contains more than %i lines of text.\n",
                   number[sub], MAXLINES);
          exit (EXIT_FAILURE);
        }
        strncpy (text[sub][ntext[sub]], input[line], MAXLEN);
        ntext[sub]++;
      } else {
        break;
      }

      line++;  // Next line of input file
    }

    // Warn if current subtitle has no lines of text.
    if (ntext[sub] < 1) {
      nwarn++;
      fprintf (stdout, "\nWARNING: Subtitle has no lines of text.\n");
      fprintf (stdout, "         While not prohibited, subtitles are generally 1 to 2 lines of text.\n");
      fprintf (stdout, "\n%i\n", sub + 1);
      fprintf (stdout, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);
    }

    // Warn if current subtitle has more than two lines of text.
    if (ntext[sub] > 2) {
      nwarn++;
      fprintf (stdout, "\nWARNING: Subtitle has %i lines of text.\n", ntext[sub]);
      fprintf (stdout, "         While not prohibited, subtitles are generally limited to 2 lines for readability.\n");
      fprintf (stdout, "\n%i\n", sub + 1);
      fprintf (stdout, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);
    }

    sub++;

  }  // Next subtitle

  // Check to see if subtitles numbers are all in ascending order.
  // Most media players ignore the value as long as an integer number is used.
  for (sub=1; sub<nsubs; sub++) {

    if (number[sub] <= number[sub - 1]) {
      nwarn++;
      fprintf (stdout, "\nWARNING: Subtitle number %i is less than or equal to preceding subtitle number.\n", number[sub]);
      fprintf (stdout, "         Most media players ignore the value as long as an integer number is used.\n");
      fprintf (stdout, "\n%i\n", number[sub - 1]);
      fprintf (stdout, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub - 1].h, start[sub - 1].m, start[sub - 1].s, start[sub - 1].ms, end[sub - 1].h, end[sub - 1].m, end[sub - 1].s, end[sub - 1].ms);
      for (line=0; line<ntext[sub - 1]; line++) {
        fprintf (stdout, "%s", text[sub - 1][line]);
      }
      fprintf (stdout, "\n");
      fprintf (stdout, "%i\n", number[sub]);
      fprintf (stdout, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);
      for (line=0; line<ntext[sub]; line++) {
        fprintf (stdout, "%s", text[sub][line]);
      }
    }  // End if
  }  // Next sub

  // Check to see if subtitle numbers are arithmetically increasing by a value of 1.
  // Most media players ignore the value as long as an integer number is used.
  for (sub=1; sub<nsubs; sub++) {

    if ((number[sub] - number[sub - 1]) > 1) {
      nwarn++;
      fprintf (stdout, "\nWARNING: Subtitle number %i is more than 1 greater than preceding subtitle number.\n", number[sub]);
      fprintf (stdout, "         Most media players ignore the value as long as an integer number is used.\n");
      fprintf (stdout, "\n%i\n", number[sub - 1]);
      fprintf (stdout, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub - 1].h, start[sub - 1].m, start[sub - 1].s, start[sub - 1].ms, end[sub - 1].h, end[sub - 1].m, end[sub - 1].s, end[sub - 1].ms);
      for (line=0; line<ntext[sub - 1]; line++) {
        fprintf (stdout, "%s", text[sub - 1][line]);
      }
      fprintf (stdout, "\n");
      fprintf (stdout, "%i\n", number[sub]);
      fprintf (stdout, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);
      for (line=0; line<ntext[sub]; line++) {
        fprintf (stdout, "%s", text[sub][line]);
      }
    }  // End if
  }  // Next sub

  // Check to see if start and end times are chronological for current subtitle.
  for (sub=0; sub<nsubs; sub++) {
    if ((end[sub].totalms - start[sub].totalms) <= 0) {
      fprintf (stderr, "\nERROR: Start and end times are either non-chronological or the same.\n");
      fprintf (stderr, "\n%i\n", sub + 1);
      fprintf (stderr, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);
      exit (EXIT_FAILURE);
    }
  }  // Next subtitle

  // Check that start of current subtitle occurs after or exactly at end of previous subtitle.
  for (sub=1; sub<nsubs; sub++) {

    // Not chronological
    if ((start[sub].totalms - end[sub - 1].totalms) < 0) {
      fprintf (stderr, "\nERROR: Subtitle %i does not chronologically follow subtitle %i.\n", sub + 1, sub);
      fprintf (stderr, "\n%i\n", sub);
      fprintf (stderr, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub - 1].h, start[sub - 1].m, start[sub - 1].s, start[sub - 1].ms, end[sub - 1].h, end[sub - 1].m, end[sub - 1].s, end[sub - 1].ms);
      fprintf (stderr, "\n%i\n", sub + 1);
      fprintf (stderr, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);
      exit (EXIT_FAILURE);
    }

  }  // Next subtitle

  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  fprintf (stdout, "%i warnings for file %s.\n\n", nwarn, filename);

  // Free allocated memory.
  free (temp);
  free (bom);
  free (filename);
  free (start);
  free (end);
  for (sub = 0; sub < nsubs; sub++) {
    for (i = 0; i < MAXLINES; i++) {
      free (text[sub][i]);
    }
    free (text[sub]);
  }
  free (text);
  free (ntext);
  for (line = 0; line < alllines; line++) {
    free (input[line]);
  }
  free (input);
  free (number);

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

  // Loop through all types of Byte Order Marks. Keep the longest matching BOM
  // because some shorter BOMs are prefixes of longer ones (e.g., UTF-16 LE is
  // a prefix of UTF-32 LE).
  for (type=0; type<MAXBOM; type++) {

    found = 1;  // Default to current type detected.
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

  int i, loc[9] = {0, 1, 3, 4, 6, 7, 9, 10, 11};
  char *temp;

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);

  // Proper format
  //           1         2         3         4         5         6         7         8
  // 012345678901234567890123456789012345678901234567890123456789012345678901234567890
  // 01:12:15,025 --> 01:12:17,645

  // Check for arrow.
  if (strncmp (&text[12], " --> ", 5) != 0) {
    fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
    fprintf (stderr, "       %s\n", text);
    exit (EXIT_FAILURE);
  }

  // Starting timestamp.
  memset (temp, 0, MAXLEN * sizeof (char));
  memcpy (temp, text, 12 * sizeof (char));

  // Check for fatal format errors.
  if ((temp[2] != ':') || (temp[5] != ':') || (temp[8] != ',')) {
    fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
    fprintf (stderr, "       %s\n", text);
    exit (EXIT_FAILURE);
  }
  for (i=0; i<9; i++) {
    if ((temp[loc[i]] < '0') || (temp[loc[i]] > '9')) {
      fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
      fprintf (stderr, "       %s\n", text);
      exit (EXIT_FAILURE);
    }
  }

  // Format appears ok, so parse timestamp.
  parsetimestamp (temp, start);

  // Ending timestamp.
  memset (temp, 0, MAXLEN * sizeof (char));
  memcpy (temp, &text[17], 12 * sizeof (char));

  // Check for fatal format errors.
  if ((temp[2] != ':') || (temp[5] != ':') || (temp[8] != ',')) {
    fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
    fprintf (stderr, "       %s\n", text);
    exit (EXIT_FAILURE);
  }
  for (i=0; i<9; i++) {
    if ((temp[loc[i]] < '0') || (temp[loc[i]] > '9')) {
      fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
      fprintf (stderr, "       %s\n", text);
      exit (EXIT_FAILURE);
    }
  }

  // Format appears ok, so parse timestamp.
  parsetimestamp (temp, end);

  // Free allocated memory.
  free (temp);

  return (EXIT_SUCCESS);
}

// Parse timestamp into TIME struct, and also return total time in milliseconds.
int
parsetimestamp (char *timestamp, TIME *time) {

  char *xx, *xxx, *endptr;

  // Allocate memory for various arrays.
  xx = allocate_strmem (3);
  xxx = allocate_strmem (4);

  // Hours
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, timestamp, 2);
  errno = 0;
  time->h = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of hours: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE); 
  }

  // Minutes
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, &timestamp[3], 2);
  errno = 0;
  time->m = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of minutes: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }
  if ((time->m < 0) || (time->m > 59)) {
    fprintf (stderr, "ERROR: Minutes are outside valid range 00-59: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Seconds
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, &timestamp[6], 2);
  errno = 0;
  time->s = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of seconds: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }
  if ((time->s < 0) || (time->s > 59)) {
    fprintf (stderr, "ERROR: Seconds are outside valid range 00-59: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Milliseconds
  memset (xxx, 0, 4 * sizeof (char));
  strncpy (xxx, &timestamp[9], 3);
  errno = 0;
  time->ms = (int) strtol (xxx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xxx)) {
    fprintf (stderr, "ERROR: Cannot make integer of milliseconds: %s\n", xxx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Total milliseconds.
  timetoms (time);

  // Free allocated memory.
  free (xx);
  free (xxx);

  return (EXIT_SUCCESS);
}

// Calculate totalms from h, m, s, ms in TIME struct.
int
timetoms (TIME *time) {

  time->totalms = (int64_t) time->h * 60 * 60 * 1000;
  time->totalms += (int64_t) time->m * 60 * 1000;
  time->totalms += (int64_t) time->s * 1000;
  time->totalms += time->ms;
    
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

// Allocate memory for an array of pointers to arrays of pointers to arrays of chars.
char ***
allocate_strmempp (size_t len) {
  return (allocate_mem (len, sizeof (char **), "array of pointers to arrays of pointers to arrays of chars"));
}

// Allocate memory for an array of TIME structs.
TIME * 
allocate_timemem (size_t len) {
  return (allocate_mem (len, sizeof (TIME), "array of TIME structs"));
}

// Allocate memory for an array of BOM (Byte Order Mark) structs.
BOM * 
allocate_bommem (size_t len) {
  return (allocate_mem (len, sizeof (BOM), "array of BOM (Byte Order Mark) structs"));
}
