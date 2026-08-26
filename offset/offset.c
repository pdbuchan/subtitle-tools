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

// offset.c - Read an existing SubRip (srt) file, apply a positive or negative offset to the time stamps,
// and save in an output file. Ignores existing subtitle numbers and renumbers them from 1 to N.

// gcc -Wall offset.c -o offset

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
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
int parse_int_string (const char *, int *);
int readline (FILE *, char *, int);
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);
int extract_time (char*, TIME *, TIME *);
int parsetimestamp (char *, TIME *);
int timetoms (TIME *);
int mstotime (TIME *);
static void *allocate_mem (size_t, size_t, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
int *allocate_intmem (size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXLINES 10  // Maximum number of lines of text per subtitle

// Byte Order Marks are at most four bytes long in the table below.
#define BOM_BUFFER_SIZE 4

int
main (int argc, char **argv) {

  int i, type, alllines, nlines, line, nsubs;
  size_t nread;
  char *temp, *filename, **input;
  uint8_t bom_buffer[BOM_BUFFER_SIZE] = {0};
  TIME start, end, offset;
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
    fprintf (stdout, "\nUsage: ./offset inputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    fprintf (stdout, "       Note: You can use all 0 offset values if you want to just renumber the subtitles.\n\n");
    free (filename);
    return (EXIT_SUCCESS);
  }

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Ask for desired offsets.
  // All can be zero.
  fprintf (stdout, "\nWhat is desired offset hours? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  if (parse_int_string (temp, &offset.h) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Cannot make integer of offset hours: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "What is desired offset minutes? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  if (parse_int_string (temp, &offset.m) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Cannot make integer of offset minutes: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "What is desired offset seconds? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  if (parse_int_string (temp, &offset.s) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Cannot make integer of offset seconds: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "What is desired offset milliseconds? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  if (parse_int_string (temp, &offset.ms) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Cannot make integer of offset milliseconds: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  // Calculate total milliseconds for offset.
  timetoms (&offset);

  // Open existing SubRip file in binary mode so BOM bytes are examined exactly.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Examine the beginning of the file for a BOM before parsing any SubRip
  // lines. Read up to the maximum BOM length; a short file is valid input.
  nread = fread (bom_buffer, sizeof (bom_buffer[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to examine input SubRip file %s for a Byte Order Mark.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  type = byteordermark (bom_buffer, nread, bom, nbom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    // This program parses SubRip syntax as single-byte/UTF-8 text. UTF-8 is
    // compatible with that processing; other BOM-marked encodings are not.
    if (type != 0) {
      fprintf (stderr, "ERROR: Character encoding %s is not supported by this byte-oriented SubRip parser.\n", bom[type].name);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }

  // Position the stream at the first byte of SubRip text. For UTF-8 input,
  // skip its BOM so it never becomes part of the first subtitle-number line.
  if (fseek (fi, (type == 0) ? (long) bom[type].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to position input SubRip file %s after Byte Order Mark detection.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllines = 0;
  for (;;) {
    int status = readline (fi, temp, MAXLEN);

    if (status == -1) break;
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in input SubRip file does not fit in the %d-byte input buffer.\n", alllines + 1, MAXLEN);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
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

  // Return to the first byte of SubRip text for the second reading pass.
  clearerr (fi);
  if (fseek (fi, (type == 0) ? (long) bom[type].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to reposition input SubRip file %s.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  // Allocate memory for array to hold input file.
  input = allocate_strmemp (alllines);
  for (line=0; line<alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0; line<alllines; line++) {
    int status = readline (fi, input[line], MAXLEN);

    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in input SubRip file does not fit in the %d-byte input buffer.\n", line + 1, MAXLEN);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read line %i from input SubRip file %s.\n", line + 1, filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
    if (status == -1) {
      fprintf (stderr, "ERROR: Unexpected end of input while rereading line %i from %s.\n", line + 1, filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }

  // Close input file.
  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Remove excess line-feeds at end of array input, leaving the one blank line
  // that terminates the final subtitle.
  nlines = alllines;
  while ((nlines > 1) && (input[nlines - 1][0] == '\n') && (input[nlines - 2][0] == '\n')) {
    nlines--;
  }
  fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);

  // Count and structurally validate subtitles. Existing subtitle numbers are
  // intentionally ignored because the output is renumbered from 1 to N.
  nsubs = 0;
  line = 0;
  while (line < nlines) {

    if (input[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at input line %i.\n", line + 1);
      exit (EXIT_FAILURE);
    }

    // Skip existing subtitle number.
    line++;
    if (line >= nlines || input[line][0] == '\n') {
      fprintf (stderr, "ERROR: Missing timestamp for subtitle %i.\n", nsubs + 1);
      exit (EXIT_FAILURE);
    }

    // Skip timestamp and any subtitle text.
    line++;
    while ((line < nlines) && (input[line][0] != '\n')) {
      line++;
    }

    if (line >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %i is not terminated by a blank line.\n", nsubs + 1);
      exit (EXIT_FAILURE);
    }

    line++;  // Skip subtitle-closing blank line.
    nsubs++;
  }

  if (nsubs == 0) {
    fprintf (stderr, "ERROR: No subtitles found in input file.\n");
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Open output file without overwriting an existing file.
  fo = fopen ("out.srt", "wx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.srt.\n");
    }
    exit (EXIT_FAILURE);
  }

  // Write Byte Order Mark (BOM) to output file if a UTF-8 BOM was detected.
  if (type == 0) {
    if (fwrite (bom[type].sequence, sizeof (uint8_t), bom[type].len, fo) != bom[type].len) {
      fprintf (stderr, "ERROR: Unable to write Byte Order Mark to out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }
  }

  // Loop through all subtitles.
  line = 0;  // Line index of input file
  for (i=0; i<nsubs; i++) {

    // Write new subtitle number and skip the original one.
    if (fprintf (fo, "%i\n", i + 1) < 0) {
      fprintf (stderr, "ERROR: Unable to write output file out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }
    line++;

    // Extract start and end times.
    extract_time (input[line], &start, &end);
    line++;

    // Apply offset to millisecond totals for start and end.
    start.totalms += offset.totalms;
    end.totalms += offset.totalms;

    // A negative SubRip timestamp cannot be represented. Check before
    // converting the totals back to component fields.
    if ((start.totalms < 0) || (end.totalms < 0)) {
      fprintf (stderr, "ERROR: Underflow in timestamp. Too much negative offset applied?\n");
      fprintf (stderr, "\n%i\n", i + 1);
      fprintf (stderr, "Shifted totals: start=%" PRId64 " ms, end=%" PRId64 " ms\n\n", start.totalms, end.totalms);
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }

    // Update hours, minutes, seconds, milliseconds of start and end structs.
    if ((mstotime (&start) == EXIT_FAILURE) || (mstotime (&end) == EXIT_FAILURE)) {
      fprintf (stderr, "ERROR: Unable to convert shifted timestamp.\n");
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }

    // Write updated start and end timestamps.
    if (fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start.h, start.m, start.s, start.ms, end.h, end.m, end.s, end.ms) < 0) {
      fprintf (stderr, "ERROR: Unable to write output file out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }

    // Write all text of current subtitle.
    while ((line < nlines) && (input[line][0] != '\n')) {
      if (fputs (input[line], fo) == EOF) {
        fprintf (stderr, "ERROR: Unable to write output file out.srt.\n");
        fclose (fo);
        remove ("out.srt");
        exit (EXIT_FAILURE);
      }
      line++;
    }

    if ((line >= nlines) || (input[line][0] != '\n')) {
      fprintf (stderr, "ERROR: Internal subtitle-structure error while writing subtitle %i.\n", i + 1);
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }

    // End-of-subtitle blank line.
    if (fputc ('\n', fo) == EOF) {
      fprintf (stderr, "ERROR: Unable to write output file out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      exit (EXIT_FAILURE);
    }
    line++;
  }

  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt after writing.\n");
    remove ("out.srt");
    exit (EXIT_FAILURE);
  }

  // Free allocated memory.
  free (temp);
  free (filename);
  for (line = 0; line < alllines; line++) {
    free (input[line]);
  }
  free (input);

  return (EXIT_SUCCESS);
}

// Obtain a text string from standard input. It can include spaces.
int
inputtext (char *text) {

  int ch;
  size_t len;

  if (fgets (text, MAXLEN, stdin) == NULL) {
    fprintf (stderr, "Unable to read text from standard input.\n");
    exit (EXIT_FAILURE);
  }

  len = strlen (text);

  // Remove trailing newline, and a preceding carriage return if present.
  if ((len > 0) && (text[len - 1] == '\n')) {
    text[--len] = '\0';
    if ((len > 0) && (text[len - 1] == '\r')) {
      text[--len] = '\0';
    }
    return (EXIT_SUCCESS);
  }

  // If the buffer is full, determine whether the input was exactly
  // MAXLEN - 1 characters or was genuinely too long.
  if (len == MAXLEN - 1) {

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

    fprintf (stderr, "Input text is too long; maximum is %d characters.\n", MAXLEN - 1);
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Parse a complete decimal integer string.
int
parse_int_string (const char *text, int *value) {

  char *endptr;
  long parsed;

  if ((text == NULL) || (value == NULL)) return (EXIT_FAILURE);

  errno = 0;
  parsed = strtol (text, &endptr, 10);
  if ((errno == ERANGE) || (endptr == text)) return (EXIT_FAILURE);

  while (isspace ((unsigned char) *endptr)) endptr++;
  if ((*endptr != '\0') || (parsed < INT_MIN) || (parsed > INT_MAX)) {
    return (EXIT_FAILURE);
  }

  *value = (int) parsed;
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

// Allocate memory for an array of ints.
int *
allocate_intmem (size_t len) {
  return (allocate_mem (len, sizeof (int), "array of ints"));
}
