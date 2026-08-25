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

// tag.c - Take formatting tags from one SubRip (.srt) file and text from another SubRip file and create a new SubRip file.
//         If a UTF-8 Byte Order Mark (BOM) exists in the SubRip file containing the desired text, it will be included in the output file.
//         NOTE: The two .srt files must have the same number of subtitles and the same number of text lines per subtitle.
//               Since text will likely be different between srt files, tag.c only transfers opening tags that appear at the
//               beginning of a line and closing tags that appear at the end of a line. Position tags at the beginning are also copied.

// gcc -std=c11 -Wall -Wextra -Wpedantic tag.c -o tag

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
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

typedef struct {
  size_t number_line;
  size_t timestamp_line;
  size_t text_start;
  size_t ntext;
} SUBTITLE;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (const uint8_t *, size_t, const BOM *);
char **read_file (const char *, const BOM *, int *, size_t *, size_t *);
SUBTITLE *parse_subtitles (char **, size_t, const char *, size_t *);
int valid_subtitle_number (const char *);
int write_tagged_line (FILE *, const char *, const char *);
void output_error (FILE *);
static void *allocate_mem (size_t, size_t, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
SUBTITLE *allocate_subtitlemem (size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per physical line
#define MAXBOM 11   // Number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  size_t i, line, alllinestag, alllinestext, nlinestag, nlinestext;
  size_t nsubstag, nsubstext;
  int tagtype, texttype;
  char **inputtag, **inputtext;
  SUBTITLE *tag_sub, *text_sub;
  FILE *fo;

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
    {3u, "UTF-8",        utf8},
    {2u, "UTF-16 (BE)",  utf16be},
    {2u, "UTF-16 (LE)",  utf16le},
    {4u, "UTF-32 (BE)",  utf32be},
    {4u, "UTF-32 (LE)",  utf32le},
    {3u, "UTF-7",        utf7},
    {3u, "UTF-1",        utf1},
    {4u, "UTF-EBCDIC",   utfebcdic},
    {3u, "SCSU",         scsu},
    {3u, "BOCU-1",       bocu1},
    {4u, "GB18030",      gb18030}
  };

  if (argc != 3) {
    fprintf (stdout, "\nUsage: ./tag taginputfilename.srt textinputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  // Read and validate the two SubRip files. The UTF-8 BOM, if present, is
  // removed from the first logical line before parsing. Other BOM-marked
  // encodings are rejected because the parser is byte-oriented.
  inputtag = read_file (argv[1], bom, &tagtype, &alllinestag, &nlinestag);
  inputtext = read_file (argv[2], bom, &texttype, &alllinestext, &nlinestext);

  fprintf (stdout, "\n%s: %zu lines found including any excess trailing line-feeds.\n", argv[1], alllinestag);
  fprintf (stdout, "%s: %zu lines found excluding excess trailing line-feeds.\n", argv[1], nlinestag);
  if (tagtype < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", argv[1]);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n", argv[1], bom[tagtype].name);
  }

  fprintf (stdout, "\n%s: %zu lines found including any excess trailing line-feeds.\n", argv[2], alllinestext);
  fprintf (stdout, "%s: %zu lines found excluding excess trailing line-feeds.\n", argv[2], nlinestext);
  if (texttype < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", argv[2]);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n", argv[2], bom[texttype].name);
  }

  tag_sub = parse_subtitles (inputtag, nlinestag, argv[1], &nsubstag);
  text_sub = parse_subtitles (inputtext, nlinestext, argv[2], &nsubstext);

  fprintf (stdout, "\n%zu subtitles found in %s.\n", nsubstag, argv[1]);
  fprintf (stdout, "%zu subtitles found in %s.\n\n", nsubstext, argv[2]);

  if (nsubstag != nsubstext) {
    fprintf (stderr, "ERROR: Files %s and %s have different numbers of subtitles.\n", argv[1], argv[2]);
    exit (EXIT_FAILURE);
  }

  // The transfer is line-oriented, so corresponding subtitles must contain
  // the same number of text lines.
  for (i=0; i<nsubstag; i++) {
    if (tag_sub[i].ntext != text_sub[i].ntext) {
      fprintf (stderr, "ERROR: Subtitle %zu has %zu text line%s in %s but %zu text line%s in %s.\n", i + 1u, tag_sub[i].ntext, (tag_sub[i].ntext == 1u) ? "" : "s", argv[1], text_sub[i].ntext, (text_sub[i].ntext == 1u) ? "" : "s", argv[2]);
      exit (EXIT_FAILURE);
    }
  }

  // Create output exclusively so an existing file cannot be overwritten.
  errno = 0;
  fo = fopen ("out.srt", "wbx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to create output file out.srt.\n");
    }
    exit (EXIT_FAILURE);
  }

  // The output encoding follows the file supplying the desired text.
  if (texttype == 0) {
    if (fwrite (bom[0].sequence, 1u, bom[0].len, fo) != bom[0].len) {
      output_error (fo);
    }
  }

  for (i=0; i<nsubstag; i++) {

    // Renumber subtitles consecutively.
    if (fprintf (fo, "%zu\n", i + 1u) < 0) {
      output_error (fo);
    }

    // Timestamps come from the file supplying the desired text.
    if (fputs (inputtext[text_sub[i].timestamp_line], fo) == EOF) {
      output_error (fo);
    }

    // Transfer beginning/end tags from each corresponding formatted line to
    // the desired text line.
    for (line=0; line<tag_sub[i].ntext; line++) {
      if (write_tagged_line (fo, inputtag[tag_sub[i].text_start + line], inputtext[text_sub[i].text_start + line]) != EXIT_SUCCESS) {
        output_error (fo);
      }
    }

    if (fputc ('\n', fo) == EOF) {
      output_error (fo);
    }
  }

  if (fclose (fo) != 0) {
    remove ("out.srt");
    fprintf (stderr, "ERROR: Unable to close output file out.srt successfully.\n");
    exit (EXIT_FAILURE);
  }

  // Free allocated memory.
  for (line = 0; line < alllinestag; line++) {
    free (inputtag[line]);
  }
  free (inputtag);

  for (line = 0; line < alllinestext; line++) {
    free (inputtext[line]);
  }
  free (inputtext);
  free (tag_sub);
  free (text_sub);

  return (EXIT_SUCCESS);
}

// Read a SubRip/text file into memory using the common subtitle readline()
// semantics. Return the total physical line count and a logical line count in
// which excess trailing blank lines have been removed while retaining one
// closing blank line.
char **
read_file (const char *filename, const BOM *bom, int *type, size_t *alllines, size_t *nlines) {

  int status;
  size_t line, len, nread;
  uint8_t prefix[4] = {0u, 0u, 0u, 0u};
  char temp[MAXLEN];
  char **input;
  FILE *fi;

  if ((filename == NULL) || (bom == NULL) || (type == NULL) ||
      (alllines == NULL) || (nlines == NULL)) {
    fprintf (stderr, "ERROR: Invalid argument supplied to read_file().\n");
    exit (EXIT_FAILURE);
  }

  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Inspect at most the first four bytes before line-oriented parsing.
  nread = fread (prefix, 1u, sizeof (prefix), fi);
  if (ferror (fi)) {
    fclose (fi);
    fprintf (stderr, "ERROR: Unable to read input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  *type = byteordermark (prefix, nread, bom);
  if ((*type >= 0) && (*type != 0)) {
    fclose (fi);
    fprintf (stderr, "ERROR: Input file %s uses %s encoding.\n", filename, bom[*type].name);
    fprintf (stderr, "       This byte-oriented program requires UTF-8 or an unmarked compatible single-byte encoding.\n");
    exit (EXIT_FAILURE);
  }

  rewind (fi);
  if (ferror (fi)) {
    fclose (fi);
    fprintf (stderr, "ERROR: Unable to rewind input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // First pass: count physical lines and handle every readline() status.
  *alllines = 0u;
  for (;;) {
    status = readline (fi, temp, MAXLEN);
    if (status == 0) {
      (*alllines)++;
    } else if (status == -1) {
      break;
    } else if (status == -2) {
      fclose (fi);
      fprintf (stderr, "ERROR: Line %zu in %s does not fit in the %d-byte input buffer.\n",
               *alllines + 1u, filename, MAXLEN);
      exit (EXIT_FAILURE);
    } else {
      fclose (fi);
      fprintf (stderr, "ERROR: Unable to read line %zu from input SubRip file %s.\n",
               *alllines + 1u, filename);
      exit (EXIT_FAILURE);
    }
  }

  if (*alllines == 0u) {
    fclose (fi);
    fprintf (stderr, "ERROR: Input SubRip file %s is empty.\n", filename);
    exit (EXIT_FAILURE);
  }

  rewind (fi);
  if (ferror (fi)) {
    fclose (fi);
    fprintf (stderr, "ERROR: Unable to rewind input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  input = allocate_strmemp (*alllines);
  for (line=0; line<*alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Second pass: load each line and again distinguish every return value.
  for (line=0; line<*alllines; line++) {
    status = readline (fi, input[line], MAXLEN);
    if (status != 0) {
      fclose (fi);
      if (status == -1) {
        fprintf (stderr, "ERROR: Unexpected end of file while reading line %zu from %s.\n", line + 1u, filename);
      } else if (status == -2) {
        fprintf (stderr, "ERROR: Line %zu in %s does not fit in the %d-byte input buffer.\n", line + 1u, filename, MAXLEN);
      } else {
        fprintf (stderr, "ERROR: Unable to read line %zu from input SubRip file %s.\n", line + 1u, filename);
      }
      exit (EXIT_FAILURE);
    }
  }

  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input SubRip file %s successfully.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Remove the UTF-8 BOM from the first logical line. It is written back only
  // if this is the text file chosen as the source of output encoding.
  if (*type == 0) {
    len = strlen (input[0]);
    if (len < bom[0].len) {
      fprintf (stderr, "ERROR: Invalid UTF-8 BOM in input file %s.\n", filename);
      exit (EXIT_FAILURE);
    }
    memmove (input[0], input[0] + bom[0].len, len - bom[0].len + 1u);
  }

  // Remove excess trailing blank lines while retaining the one that closes
  // the final subtitle.
  *nlines = *alllines;
  while ((*nlines > 1u) &&
         (input[*nlines - 1u][0] == '\n') &&
         (input[*nlines - 2u][0] == '\n')) {
    (*nlines)--;
  }

  return (input);
}

// Validate SubRip block structure and record where each subtitle's fields are
// located. Subtitle numbers are not preserved, but validating them catches a
// shifted or otherwise malformed input structure before indexing it.
SUBTITLE *
parse_subtitles (char **input, size_t nlines, const char *filename, size_t *nsubs) {

  size_t pos, count, capacity;
  SUBTITLE *subs, *grown;

  if ((input == NULL) || (filename == NULL) || (nsubs == NULL) || (nlines == 0u)) {
    fprintf (stderr, "ERROR: Invalid argument supplied to parse_subtitles().\n");
    exit (EXIT_FAILURE);
  }

  if (input[nlines - 1u][0] != '\n') {
    fprintf (stderr, "ERROR: Final subtitle in %s is not closed by a blank line.\n", filename);
    exit (EXIT_FAILURE);
  }

  capacity = 16u;
  subs = allocate_subtitlemem (capacity);
  count = 0u;
  pos = 0u;

  while (pos < nlines) {

    if (input[pos][0] == '\n') {
      fprintf (stderr, "ERROR: Unexpected blank line at line %zu in %s.\n", pos + 1u, filename);
      exit (EXIT_FAILURE);
    }

    if (!valid_subtitle_number (input[pos])) {
      fprintf (stderr, "ERROR: Invalid subtitle number at line %zu in %s: %s", pos + 1u, filename, input[pos]);
      if (strchr (input[pos], '\n') == NULL) fputc ('\n', stderr);
      exit (EXIT_FAILURE);
    }

    if (count == capacity) {
      if (capacity > (SIZE_MAX / 2u)) {
        fprintf (stderr, "ERROR: Too many subtitles in %s.\n", filename);
        exit (EXIT_FAILURE);
      }
      capacity *= 2u;
      grown = realloc (subs, capacity * sizeof (*subs));
      if (grown == NULL) {
        fprintf (stderr, "ERROR: Unable to enlarge subtitle index array.\n");
        exit (EXIT_FAILURE);
      }
      subs = grown;
    }

    subs[count].number_line = pos;
    pos++;

    if ((pos >= nlines) || (input[pos][0] == '\n')) {
      fprintf (stderr, "ERROR: Missing timestamp line for subtitle %zu in %s.\n", count + 1u, filename);
      exit (EXIT_FAILURE);
    }
    if (strstr (input[pos], " --> ") == NULL) {
      fprintf (stderr, "ERROR: Malformed timestamp line for subtitle %zu in %s: %s", count + 1u, filename, input[pos]);
      if (strchr (input[pos], '\n') == NULL) fputc ('\n', stderr);
      exit (EXIT_FAILURE);
    }

    subs[count].timestamp_line = pos;
    pos++;
    subs[count].text_start = pos;
    subs[count].ntext = 0u;

    while ((pos < nlines) && (input[pos][0] != '\n')) {
      subs[count].ntext++;
      pos++;
    }

    if (pos >= nlines) {
      fprintf (stderr, "ERROR: Subtitle %zu in %s is not closed by a blank line.\n", count + 1u, filename);
      exit (EXIT_FAILURE);
    }

    // Consume exactly one separator. If another blank line follows internally,
    // it will be diagnosed as an unexpected blank line at the next iteration.
    pos++;
    count++;
  }

  *nsubs = count;
  return (subs);
}

// Return nonzero if line contains a positive decimal subtitle number and no
// other non-whitespace characters.
int
valid_subtitle_number (const char *line) {

  const unsigned char *p;
  int have_digit;

  if (line == NULL) return (0);

  p = (const unsigned char *) line;
  while ((*p == ' ') || (*p == '\t')) p++;

  have_digit = 0;
  while (isdigit (*p)) {
    have_digit = 1;
    p++;
  }
  if (!have_digit) return (0);

  while ((*p == ' ') || (*p == '\t')) p++;
  return ((*p == '\n') || (*p == '\0'));
}

// Write one desired text line bracketed by formatting taken from the
// corresponding formatted line. Only opening tags immediately at the start and
// closing HTML-style tags immediately at the end are transferred. This follows
// the documented limitation of tag.c and deliberately ignores inline tags.
int
write_tagged_line (FILE *fo, const char *tagline, const char *textline) {

  size_t taglen, textlen, pos, end, lt, suffix;

  if ((fo == NULL) || (tagline == NULL) || (textline == NULL)) return (EXIT_FAILURE);

  taglen = strlen (tagline);
  if ((taglen > 0u) && (tagline[taglen - 1u] == '\n')) taglen--;

  textlen = strlen (textline);
  if ((textlen > 0u) && (textline[textlen - 1u] == '\n')) textlen--;

  // Copy adjacent opening HTML tags and SSA/ASS-style position/override blocks
  // from the beginning of the formatted line.
  pos = 0u;
  while (pos < taglen) {
    if ((tagline[pos] == '<') && ((pos + 1u >= taglen) || (tagline[pos + 1u] != '/'))) {
      end = pos + 1u;
      while ((end < taglen) && (tagline[end] != '>')) end++;
      if (end >= taglen) return (EXIT_FAILURE);
      if (fwrite (tagline + pos, 1u, end - pos + 1u, fo) != (end - pos + 1u)) return (EXIT_FAILURE);
      pos = end + 1u;
    } else if ((tagline[pos] == '{') && (pos + 1u < taglen) && (tagline[pos + 1u] == '\\')) {
      end = pos + 2u;
      while ((end < taglen) && (tagline[end] != '}')) end++;
      if (end >= taglen) return (EXIT_FAILURE);
      if (fwrite (tagline + pos, 1u, end - pos + 1u, fo) != (end - pos + 1u)) return (EXIT_FAILURE);
      pos = end + 1u;
    } else {
      break;
    }
  }

  if ((textlen > 0u) && (fwrite (textline, 1u, textlen, fo) != textlen)) return (EXIT_FAILURE);

  // Find the first tag in a chain of closing tags at the very end of the
  // formatted line, preserving their original order and nesting.
  suffix = taglen;
  end = taglen;
  while ((end > pos) && (tagline[end - 1u] == '>')) {
    lt = end - 1u;
    while ((lt > pos) && (tagline[lt] != '<')) lt--;
    if ((tagline[lt] != '<') || (lt + 1u >= end) || (tagline[lt + 1u] != '/')) break;
    suffix = lt;
    end = lt;
  }

  if ((suffix < taglen) &&
      (fwrite (tagline + suffix, 1u, taglen - suffix, fo) != (taglen - suffix))) {
    return (EXIT_FAILURE);
  }

  if (fputc ('\n', fo) == EOF) return (EXIT_FAILURE);
  return (EXIT_SUCCESS);
}

// Handle an output failure consistently and remove a partial output file.
void
output_error (FILE *fo) {

  if (fo != NULL) fclose (fo);
  remove ("out.srt");
  fprintf (stderr, "ERROR: Unable to write output file out.srt. Partial output was removed.\n");
  exit (EXIT_FAILURE);
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

// Detect a Byte Order Mark (BOM), if present, using the longest complete match.
// Return the corresponding BOM index, or -1 if no listed BOM is present.
int
byteordermark (const uint8_t *text, size_t nbytes, const BOM *bom) {

  int type, best;
  size_t i, bestlen;
  int found;

  if ((text == NULL) || (bom == NULL)) return (-1);

  best = -1;
  bestlen = 0u;

  for (type=0; type<MAXBOM; type++) {
    if (bom[type].len > nbytes) continue;

    found = 1;
    for (i=0u; i<bom[type].len; i++) {
      if (text[i] != bom[type].sequence[i]) {
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

// Allocate memory for an array of SUBTITLE structs.
SUBTITLE *
allocate_subtitlemem (size_t len) {
  return (allocate_mem (len, sizeof (SUBTITLE), "array of SUBTITLE structs"));
}
