/*  Copyright (C) 2026 P. David Buchan (pdbuchan@gmail.com)

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

// ellipsis.c - Read an existing SubRip (.srt) file and remove any subtitles consisting of text with one of the following:
//              "...\n"
//              "...\n...\n"
//              " ...\n...\n"
//              "...\n ...\n"
//              or bogus ellipsis marks:
//              "---\n"
//              "---\n---\n" 
//              " ---\n---\n"
//              "---\n ---\n"
//              If a Byte Order Mark (BOM) exists in the SubRip file containing the desired text, it will be included in the output file.
//              Write a new SubRip file.

// gcc -Wall ellipsis.c -o ellipsis

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

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);
static void *allocate_mem (size_t, size_t, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
int *allocate_intmem (size_t);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters in a string
#define BOM_BUFFER_SIZE 4  // Maximum number of bytes in a recognized BOM

int
main (int argc, char **argv) {

  int i, c, type, status, alllines, nlines, line, nsubs, sub, *sublines, *skiplist;
  int text_start;
  size_t nread, textlen, pos, len;
  char *temp, *filename, **input, **time, **text;
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

  // Allocate memory for filename.
  filename = allocate_strmem (MAXLEN);

  // Process the command line arguments, if any.
  if (argc == 2) {
    if (snprintf (filename, MAXLEN, "%s", argv[1]) >= MAXLEN) {
      fprintf (stderr, "ERROR: Input filename is too long.\n");
      free (filename);
      return (EXIT_FAILURE);
    }

  } else {
    fprintf (stdout, "\nUsage: ./ellipsis inputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    free (filename);
    return (EXIT_SUCCESS);
  }

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);

  fprintf (stdout, "\nInput file: %s\n", filename);

  // Open existing SubRip file.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Read up to the maximum BOM length. A short file is valid input; it may
  // still contain a two- or three-byte BOM.
  nread = fread (bom_input, sizeof (bom_input[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
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
      fprintf (stderr, "ERROR: This program can directly parse only UTF-8 or byte-compatible text input.\n");
      fprintf (stderr, "       Convert %s input to UTF-8 before processing it.\n", bom[type].name);
      fclose (fi);
      exit (EXIT_FAILURE);
    }

    // Position the stream immediately after the UTF-8 BOM.
    if (fseek (fi, (long) bom[type].len, SEEK_SET) != 0) {
      fprintf (stderr, "ERROR: Unable to position input SubRip file %s after its BOM.\n", filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  }

  // Count lines of input SubRip file, handling every readline() return value.
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

  // Return to the beginning of subtitle text. For UTF-8 BOM input, skip the
  // three BOM bytes again; otherwise return to byte zero.
  if (type == 0) {
    if (fseek (fi, (long) bom[type].len, SEEK_SET) != 0) {
      fprintf (stderr, "ERROR: Unable to position input SubRip file %s after its BOM.\n", filename);
      fclose (fi);
      exit (EXIT_FAILURE);
    }
  } else {
    rewind (fi);
  }

  // Allocate memory for array to hold input file.
  input = allocate_strmemp ((size_t) alllines);
  for (line = 0; line < alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
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

  // Close input file.
  if (fclose (fi) == EOF) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Remove excess line-feeds at end of array input, retaining the one blank
  // line that closes the final subtitle.
  nlines = alllines;
  for (line = alllines; line > 1; line--) {
    if ((input[line - 1][0] == '\n') && (input[line - 2][0] == '\n')) {
      nlines--;
    } else {
      break;
    }
  }
  fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);

  if (input[nlines - 1][0] != '\n') {
    fprintf (stderr, "ERROR: Last subtitle is not closed by a blank line.\n");
    exit (EXIT_FAILURE);
  }

  // Count subtitles while validating enough structure to keep subsequent
  // indexing within the input array.
  nsubs = 0;
  line = 0;
  while (line < nlines) {

    if (input[line][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at input line %i.\n", line + 1);
      exit (EXIT_FAILURE);
    }

    // Subtitle number line.
    line++;

    if ((line >= nlines) || (input[line][0] == '\n')) {
      fprintf (stderr, "ERROR: Subtitle beginning at input line %i has no timestamp line.\n", line);
      exit (EXIT_FAILURE);
    }

    // Timestamp line.
    line++;

    // Subtitle text may contain zero or more lines.
    while ((line < nlines) && (input[line][0] != '\n')) {
      line++;
    }

    if (line >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %i is not closed by a blank line.\n", nsubs + 1);
      exit (EXIT_FAILURE);
    }

    // Move past the blank line separating subtitles.
    line++;

    if (nsubs == INT_MAX) {
      fprintf (stderr, "ERROR: Input SubRip file contains too many subtitles.\n");
      exit (EXIT_FAILURE);
    }
    nsubs++;
  }

  if (nsubs == 0) {
    fprintf (stderr, "ERROR: No subtitles found in input file.\n");
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Allocate arrays for subtitle timestamps, text, line counts, and skip flags.
  text = allocate_strmemp ((size_t) nsubs);
  time = allocate_strmemp ((size_t) nsubs);
  sublines = allocate_intmem ((size_t) nsubs);
  skiplist = allocate_intmem ((size_t) nsubs);

  // Extract timestamp lines and concatenate each subtitle's text. Allocate the
  // exact amount of space required so a subtitle is not limited to MAXLEN
  // characters in total.
  line = 0;
  for (sub = 0; sub < nsubs; sub++) {

    // Skip subtitle number.
    line++;

    len = strlen (input[line]);
    time[sub] = allocate_strmem (len + 1u);
    memcpy (time[sub], input[line], len + 1u);
    line++;

    text_start = line;
    textlen = 0u;
    sublines[sub] = 0;

    while ((line < nlines) && (input[line][0] != '\n')) {
      len = strlen (input[line]);
      if (textlen > SIZE_MAX - len - 1u) {
        fprintf (stderr, "ERROR: Subtitle text is too large to store.\n");
        exit (EXIT_FAILURE);
      }
      textlen += len;

      if (sublines[sub] == INT_MAX) {
        fprintf (stderr, "ERROR: Subtitle contains too many text lines.\n");
        exit (EXIT_FAILURE);
      }
      sublines[sub]++;
      line++;
    }

    text[sub] = allocate_strmem (textlen + 1u);
    pos = 0u;
    for (i = text_start; i < line; i++) {
      len = strlen (input[i]);
      memcpy (&text[sub][pos], input[i], len);
      pos += len;
    }
    text[sub][pos] = '\0';

    // Move past the blank line ending the subtitle.
    line++;
  }

  // Mark subtitles whose entire textual content consists only of ellipsis
  // markers (or the historical "---" substitute) in one of the accepted forms.
  for (sub = 0; sub < nsubs; sub++) {
    if (((sublines[sub] == 1) && (strcmp (text[sub], "...\n") == 0)) ||
        ((sublines[sub] == 2) && (strcmp (text[sub], "...\n...\n") == 0)) ||
        ((sublines[sub] == 2) && (strcmp (text[sub], "...\n ...\n") == 0)) ||
        ((sublines[sub] == 2) && (strcmp (text[sub], " ...\n...\n") == 0)) ||
        // Bogus ellipsis marks
        ((sublines[sub] == 1) && (strcmp (text[sub], "---\n") == 0)) ||
        ((sublines[sub] == 2) && (strcmp (text[sub], "---\n---\n") == 0)) ||
        ((sublines[sub] == 2) && (strcmp (text[sub], "---\n ---\n") == 0)) ||
        ((sublines[sub] == 2) && (strcmp (text[sub], " ---\n---\n") == 0))) {
      skiplist[sub] = 1;
    }
  }

  // Create the output file exclusively so an existing out.srt is never
  // overwritten between a separate existence check and creation.
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

  // Preserve a UTF-8 BOM if one was present in the input file.
  if (type == 0) {
    if (fwrite (bom[type].sequence, sizeof (uint8_t), bom[type].len, fo) != bom[type].len) {
      fprintf (stderr, "ERROR: Unable to write Byte Order Mark to out.srt.\n");
      fclose (fo);
      exit (EXIT_FAILURE);
    }
  }

  // Write all subtitles except those marked for removal, renumbering the
  // remaining subtitles consecutively.
  c = 0;
  for (sub = 0; sub < nsubs; sub++) {

    if (skiplist[sub]) continue;

    if (fprintf (fo, "%i\n", c + 1) < 0 || fprintf (fo, "%s", time[sub]) < 0 || fprintf (fo, "%s\n", text[sub]) < 0) {
      fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub + 1);
      fclose (fo);
      exit (EXIT_FAILURE);
    }

    c++;
  }

  if (fclose (fo) == EOF) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt after writing.\n");
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "%i subtitles written.\n\n", c);

  // Free allocated memory.
  free (temp);
  free (filename);
  for (line = 0; line < alllines; line++) {
    free (input[line]);
  }
  free (input);
  for (i = 0; i < nsubs; i++) {
    free (time[i]);
    free (text[i]);
  }
  free (time);
  free (text);
  free (sublines);
  free (skiplist);

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

