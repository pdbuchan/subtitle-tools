/*  Copyright (C) 2025-2026 P. David Buchan (pdbuchan@gmail.com)

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

// txtfiles2srt.c - Take the timestamps from the filenames in a collection of individual subtitle text files, each containing the text for a subtitle,
//                  and produce a SubRip (.srt) file. No Byte Order Mark (BOM) is prepended. Each text file should contain only non-blank lines of text.
//                  LF and CRLF line endings are accepted and normalized to LF in the output file.

// gcc -std=c11 -Wall -Wextra -Wpedantic txtfiles2srt.c -o txtfiles2srt

// Usage: ./txtfiles2srt filelistfilename
// Input: filelistfilename is a text file containing only a list of the subtitle text files. Each filename is expected to be:
//        hh_mm_ss_ms__hh_mm_ss_ms.txt. For example: 00_13_11_959__00_13_15_213.txt
//        Directory components may precede the filename.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

// Definition of structs.
typedef struct {
  int h;
  int m;
  int s;
  int ms;
  int64_t totalms;
} TIME;

typedef struct {
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes.
int readline (FILE *, char *, int);
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);
int extract_time (const char *, TIME *, TIME *);
int parsetimestamp (const char *, TIME *);
int validate_text_file (const char *, const BOM *, size_t);
char *duplicate_string (const char *);
void free_filenames (char **, size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line, including terminating line-feed.
#define BOM_BUFFER_SIZE 4  // Maximum number of bytes in a listed Byte Order Mark.

int
main (int argc, char **argv) {

  int status, type, line, sub;
  size_t nsubs, capacity, nread;
  char temp[MAXLEN];
  uint8_t bom_input[BOM_BUFFER_SIZE] = {0};
  char **textfilenames;
  TIME *start, *end;
  const char *list_filename;
  FILE *fi_list, *fi, *fo;

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

  if (argc != 2) {
    fprintf (stdout, "\nUsage: ./txtfiles2srt filelistfilename\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  list_filename = argv[1];
  textfilenames = NULL;
  start = NULL;
  end = NULL;
  nsubs = 0u;
  capacity = 0u;

  // Open input file containing list of text filenames.
  fi_list = fopen (list_filename, "rb");
  if (fi_list == NULL) {
    fprintf (stderr, "ERROR: Unable to open input file %s.\n", list_filename);
    return (EXIT_FAILURE);
  }

  // Examine the raw bytes at the start of the filename-list file before any
  // line-oriented parsing. UTF-8 is supported after its BOM is skipped; other
  // recognized BOM-marked encodings require transcoding first.
  nread = fread (bom_input, sizeof (bom_input[0]), BOM_BUFFER_SIZE, fi_list);
  if (ferror (fi_list)) {
    fprintf (stderr, "ERROR: Unable to inspect input file %s for a BOM.\n", list_filename);
    fclose (fi_list);
    return (EXIT_FAILURE);
  }

  type = byteordermark (bom_input, nread, bom, nbom);
  if ((type >= 0) && (type != 0)) {
    fprintf (stderr, "ERROR: Input file %s uses %s. Convert it to UTF-8 first.\n", list_filename, bom[type].name);
    fclose (fi_list);
    return (EXIT_FAILURE);
  }

  // Position the stream at the first byte of filename-list text. When no BOM
  // was found, return to byte zero; otherwise skip the accepted UTF-8 BOM.
  if (fseek (fi_list, (type == 0) ? (long) bom[type].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to position input file %s after BOM detection.\n", list_filename);
    fclose (fi_list);
    return (EXIT_FAILURE);
  }

  // Read the filename list. Blank lines are not valid list entries.
  line = 0;
  for (;;) {
    status = readline (fi_list, temp, MAXLEN);
    if (status == -1) break;

    line++;
    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in %s is too long.\n", line, list_filename);
      fclose (fi_list);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read line %i from %s.\n", line, list_filename);
      fclose (fi_list);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    size_t len = strlen (temp);
    if ((len > 0u) && (temp[len - 1u] == '\n')) {
      temp[len - 1u] = '\0';
    }

    if (temp[0] == '\0') {
      fprintf (stderr, "ERROR: Blank line found at line %i of filename list %s.\n", line, list_filename);
      fclose (fi_list);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    if (nsubs == capacity) {
      size_t new_capacity = (capacity == 0u) ? 16u : capacity * 2u;
      char **new_names;

      if ((new_capacity < capacity) || (new_capacity > (SIZE_MAX / sizeof (*textfilenames)))) {
        fprintf (stderr, "ERROR: Too many filenames in %s.\n", list_filename);
        fclose (fi_list);
        free_filenames (textfilenames, nsubs);
        return (EXIT_FAILURE);
      }

      new_names = realloc (textfilenames, new_capacity * sizeof (*textfilenames));
      if (new_names == NULL) {
        fprintf (stderr, "ERROR: Unable to allocate memory for filename list.\n");
        fclose (fi_list);
        free_filenames (textfilenames, nsubs);
        return (EXIT_FAILURE);
      }

      textfilenames = new_names;
      capacity = new_capacity;
    }

    textfilenames[nsubs] = duplicate_string (temp);
    if (textfilenames[nsubs] == NULL) {
      fprintf (stderr, "ERROR: Unable to allocate memory for filename at line %i.\n", line);
      fclose (fi_list);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }
    nsubs++;
  }

  if (fclose (fi_list) != 0) {
    fprintf (stderr, "ERROR: Unable to close input file %s.\n", list_filename);
    free_filenames (textfilenames, nsubs);
    return (EXIT_FAILURE);
  }

  if (nsubs == 0u) {
    fprintf (stderr, "ERROR: Filename list %s is empty.\n", list_filename);
    free_filenames (textfilenames, nsubs);
    return (EXIT_FAILURE);
  }

  if (nsubs > ((size_t) INT_MAX)) {
    fprintf (stderr, "ERROR: Too many subtitles to number with int.\n");
    free_filenames (textfilenames, nsubs);
    return (EXIT_FAILURE);
  }

  start = calloc (nsubs, sizeof (*start));
  end = calloc (nsubs, sizeof (*end));
  if ((start == NULL) || (end == NULL)) {
    fprintf (stderr, "ERROR: Unable to allocate timestamp arrays.\n");
    free (start);
    free (end);
    free_filenames (textfilenames, nsubs);
    return (EXIT_FAILURE);
  }

  // Validate every timestamp filename and every text file before creating output.
  for (sub = 0; sub < (int) nsubs; sub++) {
    if (extract_time (textfilenames[sub], &start[sub], &end[sub]) != EXIT_SUCCESS) {
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    if (start[sub].totalms >= end[sub].totalms) {
      fprintf (stderr, "ERROR: Subtitle %i has a start time that is not earlier than its end time:\n", sub + 1);
      fprintf (stderr, "       %s\n", textfilenames[sub]);
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    if ((sub > 0) && (start[sub].totalms < start[sub - 1].totalms)) {
      fprintf (stderr, "ERROR: Subtitle filenames are not in chronological order at subtitle %i:\n", sub + 1);
      fprintf (stderr, "       %s\n", textfilenames[sub]);
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    if (validate_text_file (textfilenames[sub], bom, nbom) != EXIT_SUCCESS) {
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }
  }

  fprintf (stdout, "\n%s: %zu subtitles found.\n", list_filename, nsubs);

  // Create output file without overwriting an existing file.
  errno = 0;
  fo = fopen ("out.srt", "wx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.srt.\n");
    }
    free (start);
    free (end);
    free_filenames (textfilenames, nsubs);
    return (EXIT_FAILURE);
  }

  // Write subtitles. The output intentionally contains no BOM.
  for (sub = 0; sub < (int) nsubs; sub++) {
    fi = fopen (textfilenames[sub], "rb");
    if (fi == NULL) {
      fprintf (stderr, "ERROR: Unable to reopen input file %s.\n", textfilenames[sub]);
      fclose (fo);
      remove ("out.srt");
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    // Inspect the file again because it may have changed since validation.
    memset (bom_input, 0, sizeof (bom_input));
    nread = fread (bom_input, sizeof (bom_input[0]), BOM_BUFFER_SIZE, fi);
    if (ferror (fi)) {
      fprintf (stderr, "ERROR: Unable to inspect input file %s for a BOM.\n", textfilenames[sub]);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    type = byteordermark (bom_input, nread, bom, nbom);
    if ((type >= 0) && (type != 0)) {
      fprintf (stderr, "ERROR: Input file %s changed to unsupported %s encoding.\n", textfilenames[sub], bom[type].name);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    if (fseek (fi, (type == 0) ? (long) bom[type].len : 0L, SEEK_SET) != 0) {
      fprintf (stderr, "ERROR: Unable to position input file %s after BOM detection.\n", textfilenames[sub]);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    if ((fprintf (fo, "%i\n", sub + 1) < 0) ||
        (fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms) < 0)) {
      fprintf (stderr, "ERROR: Unable to write to output file out.srt.\n");
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    for (;;) {
      size_t len;

      status = readline (fi, temp, MAXLEN);
      if (status == -1) break;

      if (status != 0) {
        fprintf (stderr, "ERROR: Input file %s changed or became unreadable while writing output.\n", textfilenames[sub]);
        fclose (fi);
        fclose (fo);
        remove ("out.srt");
        free (start);
        free (end);
        free_filenames (textfilenames, nsubs);
        return (EXIT_FAILURE);
      }

      len = strlen (temp);
      if ((len > 0u) && (temp[len - 1u] == '\n')) {
        if (fputs (temp, fo) == EOF) {
          fprintf (stderr, "ERROR: Unable to write to output file out.srt.\n");
          fclose (fi);
          fclose (fo);
          remove ("out.srt");
          free (start);
          free (end);
          free_filenames (textfilenames, nsubs);
          return (EXIT_FAILURE);
        }
      } else {
        if ((fputs (temp, fo) == EOF) || (fputc ('\n', fo) == EOF)) {
          fprintf (stderr, "ERROR: Unable to write to output file out.srt.\n");
          fclose (fi);
          fclose (fo);
          remove ("out.srt");
          free (start);
          free (end);
          free_filenames (textfilenames, nsubs);
          return (EXIT_FAILURE);
        }
      }
    }

    if (fclose (fi) != 0) {
      fprintf (stderr, "ERROR: Unable to close input file %s.\n", textfilenames[sub]);
      fclose (fo);
      remove ("out.srt");
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }

    if (fputc ('\n', fo) == EOF) {
      fprintf (stderr, "ERROR: Unable to write to output file out.srt.\n");
      fclose (fo);
      remove ("out.srt");
      free (start);
      free (end);
      free_filenames (textfilenames, nsubs);
      return (EXIT_FAILURE);
    }
  }

  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt.\n");
    remove ("out.srt");
    free (start);
    free (end);
    free_filenames (textfilenames, nsubs);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "\n");

  free (start);
  free (end);
  free_filenames (textfilenames, nsubs);
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

// Extract and validate start/end timestamps from the basename of a text filename.
// Expected basename: hh_mm_ss_ms__hh_mm_ss_ms.txt
int
extract_time (const char *filename, TIME *start, TIME *end) {

  const char *base, *slash, *backslash;
  char first[13], second[13];
  size_t len;

  if ((filename == NULL) || (start == NULL) || (end == NULL)) {
    return (EXIT_FAILURE);
  }

  base = filename;
  slash = strrchr (filename, '/');
  backslash = strrchr (filename, '\\');
  if ((slash != NULL) && (slash[1] != '\0')) base = slash + 1;
  if ((backslash != NULL) && (backslash[1] != '\0') && (backslash + 1 > base)) {
    base = backslash + 1;
  }

  len = strlen (base);
  if ((len != 30u) || (base[12] != '_') || (base[13] != '_') || (strcmp (&base[26], ".txt") != 0)) {
    fprintf (stderr, "ERROR: Subtitle filename has invalid format:\n       %s\n", filename);
    return (EXIT_FAILURE);
  }

  memcpy (first, base, 12u);
  first[12] = '\0';
  memcpy (second, &base[14], 12u);
  second[12] = '\0';

  if ((parsetimestamp (first, start) != EXIT_SUCCESS) || (parsetimestamp (second, end) != EXIT_SUCCESS)) {
    fprintf (stderr, "       Filename: %s\n", filename);
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Parse and validate one filename timestamp in hh_mm_ss_ms format.
int
parsetimestamp (const char *timestamp, TIME *time) {

  static const int digit_pos[] = {0, 1, 3, 4, 6, 7, 9, 10, 11};
  size_t i;

  if ((timestamp == NULL) || (time == NULL)) return (EXIT_FAILURE);

  if ((strlen (timestamp) != 12u) || (timestamp[2] != '_') || (timestamp[5] != '_') || (timestamp[8] != '_')) {
    fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
    return (EXIT_FAILURE);
  }

  for (i = 0u; i < (sizeof (digit_pos) / sizeof (digit_pos[0])); i++) {
    int pos = digit_pos[i];
    if ((timestamp[pos] < '0') || (timestamp[pos] > '9')) {
      fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
      return (EXIT_FAILURE);
    }
  }

  time->h = ((timestamp[0] - '0') * 10) + (timestamp[1] - '0');
  time->m = ((timestamp[3] - '0') * 10) + (timestamp[4] - '0');
  time->s = ((timestamp[6] - '0') * 10) + (timestamp[7] - '0');
  time->ms = ((timestamp[9] - '0') * 100) + ((timestamp[10] - '0') * 10) + (timestamp[11] - '0');

  if ((time->m > 59) || (time->s > 59)) {
    fprintf (stderr, "ERROR: Timestamp is out of range: %s\n", timestamp);
    return (EXIT_FAILURE);
  }

  time->totalms = (int64_t) time->h * INT64_C (3600000);
  time->totalms += (int64_t) time->m * INT64_C (60000);
  time->totalms += (int64_t) time->s * INT64_C (1000);
  time->totalms += (int64_t) time->ms;

  return (EXIT_SUCCESS);
}

// Validate one subtitle text file. UTF-8 BOMs are accepted and stripped when
// later writing output; other known BOM-marked encodings are rejected.
int
validate_text_file (const char *filename, const BOM *bom, size_t nbom) {

  FILE *fi;
  char line[MAXLEN];
  int status, bomtype, lineno, ntext;
  size_t nread;
  uint8_t bom_input[BOM_BUFFER_SIZE] = {0};

  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Inspect raw bytes before readline() so multi-byte encodings containing NUL
  // bytes cannot be mistaken for ordinary text.
  nread = fread (bom_input, sizeof (bom_input[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to inspect input file %s for a BOM.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  bomtype = byteordermark (bom_input, nread, bom, nbom);
  if ((bomtype >= 0) && (bomtype != 0)) {
    fprintf (stderr, "ERROR: Input file %s uses %s. Convert it to UTF-8 first.\n", filename, bom[bomtype].name);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  if (fseek (fi, (bomtype == 0) ? (long) bom[bomtype].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to position input file %s after BOM detection.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  lineno = 0;
  ntext = 0;
  for (;;) {
    status = readline (fi, line, MAXLEN);
    if (status == -1) break;
    lineno++;

    if (status == -2) {
      fprintf (stderr, "ERROR: Line %i in input file %s is too long.\n", lineno, filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (status == -3) {
      fprintf (stderr, "ERROR: Unable to read line %i from input file %s.\n", lineno, filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }

    if (line[0] == '\n') {
      fprintf (stderr, "ERROR: Blank line found at line %i of subtitle text file %s.\n", lineno, filename);
      fclose (fi);
      return (EXIT_FAILURE);
    }

    ntext++;
  }

  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  if (ntext == 0) {
    fprintf (stderr, "ERROR: Subtitle text file %s is empty.\n", filename);
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Duplicate a null-terminated string using standard C allocation.
char *
duplicate_string (const char *text) {

  size_t len;
  char *copy;

  if (text == NULL) return (NULL);

  len = strlen (text);
  if (len == SIZE_MAX) return (NULL);

  copy = malloc (len + 1u);
  if (copy == NULL) return (NULL);

  memcpy (copy, text, len + 1u);
  return (copy);
}
// Free a dynamically allocated list of filename strings.
void
free_filenames (char **names, size_t count) {

  size_t i;

  if (names == NULL) return;

  for (i = 0u; i < count; i++) {
    free (names[i]);
  }
  free (names);
}

