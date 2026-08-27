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

// ssa2srt-nostyles.c - Read an existing SubStationAlpha (SSA) file and convert to SubRip output file.
// Does not transfer styles, only markups for font color, bold, italic, underline, strikeout, and alignment.

// WARNING: SSA files do not require subtitles to be in chronological order, whereas SubRip files
//          do require they appear in chronological order in the srt file.
//          The SubRip output file is not corrected for this.

// gcc -Wall ssa2srt-nostyles.c -o ssa2srt-nostyles -lm

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

// Definition of structs
typedef struct {
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes
int readline (FILE *, char *, int);
int read_ssa_line (FILE *, char *, int, int *);
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);
int find_index (const char *, const char *, int *);
int extract_string (const char *, int, char, char *);
int parse_time (const char *, int *, int *, int *, int *);
int parse_color (const char *, FILE *);
int fix_text (char *);
int write_text (const char *, char *, FILE *);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters per physical line

// Byte Order Marks are at most four bytes long in the table below.
#define BOM_BUFFER_SIZE 4

int
main (int argc, char **argv) {

  int type, eofile, sub, shh, smm, sss, sms, ehh, emm, ess, ems;
  int istart, iend, itext, lineno, events_found, status;
  size_t nread;
  char temp[MAXLEN], string[MAXLEN], text[MAXLEN];
  const char *filename;
  uint8_t bom_buffer[BOM_BUFFER_SIZE] = {0};
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

  // Process the command line arguments, if any.
  if (argc == 2) {
    filename = argv[1];
  } else {
    fprintf (stdout, "\nUsage: ./ssa2srt-nostyles inputfilename\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  // Open existing SubStationAlpha file.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubStationAlpha file %s.\n", filename);
    return (EXIT_FAILURE);
  }

  // Examine the beginning of the file for a BOM before parsing any SSA
  // lines. Read up to the maximum BOM length; a short file is valid input.
  nread = fread (bom_buffer, sizeof (bom_buffer[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to examine input SubStationAlpha file %s for a Byte Order Mark.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  type = byteordermark (bom_buffer, nread, bom, nbom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);

    // This program parses SSA syntax as single-byte/UTF-8 text. UTF-8 is
    // compatible with that processing; other BOM-marked encodings are not.
    if (type != 0) {
      fprintf (stderr, "ERROR: Character encoding %s is not supported by this byte-oriented SSA parser.\n", bom[type].name);
      fprintf (stderr, "       Convert the SSA file to UTF-8 before converting it to SubRip.\n");
      fclose (fi);
      return (EXIT_FAILURE);
    }
  }

  // Position the stream at the first byte of SSA text. For UTF-8 input, skip
  // its BOM so it never becomes part of the first line read from the file.
  if (fseek (fi, (type == 0) ? (long) bom[type].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to position input SubStationAlpha file %s after Byte Order Mark detection.\n", filename);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  lineno = 0;
  events_found = 0;

  // Find [Events] section.
  while (!events_found) {
    memset (temp, 0, sizeof (temp));
    status = read_ssa_line (fi, temp, MAXLEN, &lineno);
    if (status == -1) break;
    if (status != 0) {
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (strncmp (temp, "[Events]", 8) == 0) events_found = 1;
  }

  if (!events_found) {
    fprintf (stderr, "ERROR: Cannot find [Events] section.\n");
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Find Format line of Events section.
  do {
    memset (temp, 0, sizeof (temp));
    status = read_ssa_line (fi, temp, MAXLEN, &lineno);
    if (status == -1) {
      fprintf (stderr, "ERROR: Could not find the Events Format line.\n");
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if (status != 0) {
      fclose (fi);
      return (EXIT_FAILURE);
    }
    if ((temp[0] == '[') && (strncmp (temp, "Format:", 7) != 0)) {
      fprintf (stderr, "ERROR: Events section has no Format line.\n");
      fclose (fi);
      return (EXIT_FAILURE);
    }
  } while (strncmp (temp, "Format:", 7) != 0);

  // Find the fields required to convert Dialogue events.
  if (!find_index (temp, "Start", &istart)) {
    fprintf (stderr, "ERROR: Cannot find Start in Events Format: %s", temp);
    fclose (fi);
    return (EXIT_FAILURE);
  }
  if (!find_index (temp, "End", &iend)) {
    fprintf (stderr, "ERROR: Cannot find End in Events Format: %s", temp);
    fclose (fi);
    return (EXIT_FAILURE);
  }
  if (!find_index (temp, "Text", &itext)) {
    fprintf (stderr, "ERROR: Cannot find Text in Events Format: %s", temp);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Open output file. The "x" mode prevents accidental replacement.
  errno = 0;
  fo = fopen ("out.srt", "wx");
  if (fo == NULL) {
    if (errno == EEXIST) {
      fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Unable to open output file out.srt.\n");
    }
    fclose (fi);
    return (EXIT_FAILURE);
  }

  // Preserve a UTF-8 BOM when one was present in the input file.
  if (type == 0) {
    if (fwrite (bom[type].sequence, sizeof (uint8_t), bom[type].len, fo) != bom[type].len) {
      fprintf (stderr, "ERROR: Unable to write UTF-8 BOM to out.srt.\n");
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }
  }

  // Loop through Dialogue events in the SSA file and save subtitles to SRT.
  sub = 1;
  eofile = 0;
  while (!eofile) {

    // Find next Dialogue line.
    do {
      memset (temp, 0, sizeof (temp));
      status = read_ssa_line (fi, temp, MAXLEN, &lineno);
      if (status == -1) {
        eofile = 1;
        break;
      }
      if (status != 0) {
        fclose (fi);
        fclose (fo);
        remove ("out.srt");
        return (EXIT_FAILURE);
      }
    } while (strncmp (temp, "Dialogue:", 9) != 0);

    if (eofile) break;

    // Extract and convert Start time.
    if (extract_string (temp, istart, ',', string) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Cannot extract Start from Dialogue line %i: %s", lineno, temp);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }
    if (parse_time (string, &shh, &smm, &sss, &sms) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Invalid SSA start timestamp '%s' on line %i.\n", string, lineno);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }

    // Extract and convert End time.
    if (extract_string (temp, iend, ',', string) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Cannot extract End from Dialogue line %i: %s", lineno, temp);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }
    if (parse_time (string, &ehh, &emm, &ess, &ems) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Invalid SSA end timestamp '%s' on line %i.\n", string, lineno);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }

    // Extract Text. Because Text may contain commas, extract_string() returns
    // the remainder of the physical line for this field.
    if (extract_string (temp, itext, '\n', text) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Cannot extract Text from Dialogue line %i: %s", lineno, temp);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }

    // Correct whitespace mistakes in any existing SubRip-style tags.
    if (fix_text (text) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Unable to correct markup on Dialogue line %i.\n", lineno);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }

    if (fprintf (fo, "%i\n", sub) < 0 ||
        fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", shh, smm, sss, sms, ehh, emm, ess, ems) < 0) {
      fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }

    if (write_text (text, temp, fo) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Unable to convert text for subtitle %i.\n", sub);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }

    if (fprintf (fo, "\n\n") < 0) {
      fprintf (stderr, "ERROR: Unable to write subtitle %i to out.srt.\n", sub);
      fclose (fi);
      fclose (fo);
      remove ("out.srt");
      return (EXIT_FAILURE);
    }

    sub++;
  }

  fprintf (stdout, "\n%i subtitles found in SSA input file.\n\n", sub - 1);

  // Check output and close files.
  if (ferror (fo)) {
    fprintf (stderr, "ERROR: Failed while writing output file out.srt.\n");
    fclose (fi);
    fclose (fo);
    remove ("out.srt");
    return (EXIT_FAILURE);
  }
  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
    fclose (fo);
    remove ("out.srt");
    return (EXIT_FAILURE);
  }
  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt.\n");
    remove ("out.srt");
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
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

  for (type = 0u; type < nbom; type++) {

    // The file must contain the complete signature.
    if (bom[type].len > nbytes) {
      continue;
    }

    if ((bom[type].len > best_len) && (memcmp (text, bom[type].sequence, bom[type].len) == 0)) {
      best = (int) type;
      best_len = bom[type].len;
    }
  }

  return (best);
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

    // Found a line-feed. Retain it.
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

// Read one SSA line and add a line number to readline() diagnostics.
// The readline() status is returned unchanged so callers can clean up any
// partially created output before failing.
int
read_ssa_line (FILE *fi, char *line, int limit, int *lineno) {

  int status;

  if (lineno == NULL) return (-3);

  status = readline (fi, line, limit);
  if (status == 0) {
    (*lineno)++;
  } else if (status == -2) {
    fprintf (stderr, "ERROR: Line %i of the SSA file does not fit in the %d-byte input buffer.\n", *lineno + 1, limit);
  } else if (status == -3) {
    fprintf (stderr, "ERROR: Unable to read line %i of the SSA file.\n", *lineno + 1);
  }

  return (status);
}

// Find the zero-based index of an exact element in a comma-separated Format line.
// Returns 0 if not found, 1 if found.
int
find_index (const char *text, const char *element, int *index) {

  const char *p, *start, *end;
  size_t elemlen, tokenlen;
  int current;

  if ((text == NULL) || (element == NULL) || (index == NULL)) return (0);

  p = strchr (text, ':');
  if (p == NULL) return (0);
  p++;

  elemlen = strlen (element);
  current = 0;

  while ((*p != '\0') && (*p != '\n')) {

    while ((*p == ' ') || (*p == '\t')) p++;
    start = p;

    while ((*p != '\0') && (*p != '\n') && (*p != ',')) p++;
    end = p;
    while ((end > start) && ((end[-1] == ' ') || (end[-1] == '\t'))) end--;

    tokenlen = (size_t) (end - start);
    if ((tokenlen == elemlen) && (strncmp (start, element, elemlen) == 0)) {
      *index = current;
      return (1);
    }

    if (*p == ',') {
      p++;
      current++;
    }
  }

  return (0);
}

// Extract a string element from a comma-separated line, given its zero-based index.
// If termination is '\n', the remainder of the physical line is returned; this is
// used for the Text field because subtitle text may itself contain commas.
int
extract_string (const char *text, int index, char termination, char *string) {

  const char *p, *start, *end;
  size_t len;
  int current;

  if ((text == NULL) || (string == NULL) || (index < 0)) return (EXIT_FAILURE);

  p = strchr (text, ':');
  if (p == NULL) return (EXIT_FAILURE);
  p++;
  while ((*p == ' ') || (*p == '\t')) p++;

  current = 0;
  while (current < index) {
    p = strchr (p, ',');
    if (p == NULL) return (EXIT_FAILURE);
    p++;
    current++;
  }

  start = p;
  end = start;

  if (termination == '\n') {
    while ((*end != '\0') && (*end != '\n')) end++;
  } else {
    while ((*end != '\0') && (*end != '\n') && (*end != termination)) end++;
  }

  len = (size_t) (end - start);
  if (len >= MAXLEN) return (EXIT_FAILURE);

  memcpy (string, start, len);
  string[len] = '\0';

  return (EXIT_SUCCESS);
}

// Parse a timestamp in SSA/ASS form h:mm:ss.cc.
// Other fractional-second precisions are accepted and rounded to milliseconds.
int
parse_time (const char *string, int *hh, int *mm, int *ss, int *ms) {

  const char *p;
  char *endptr;
  long hours, minutes;
  double seconds;
  long total_in_minute;

  if ((string == NULL) || (hh == NULL) || (mm == NULL) || (ss == NULL) || (ms == NULL)) {
    return (EXIT_FAILURE);
  }

  p = string;
  while (isspace ((unsigned char) *p)) p++;

  errno = 0;
  hours = strtol (p, &endptr, 10);
  if ((errno == ERANGE) || (endptr == p) || (*endptr != ':') || (hours < 0) || (hours > INT_MAX)) {
    return (EXIT_FAILURE);
  }
  p = endptr + 1;

  errno = 0;
  minutes = strtol (p, &endptr, 10);
  if ((errno == ERANGE) || (endptr == p) || (*endptr != ':') || (minutes < 0) || (minutes > 59)) {
    return (EXIT_FAILURE);
  }
  p = endptr + 1;

  errno = 0;
  seconds = strtod (p, &endptr);
  if ((errno == ERANGE) || (endptr == p) || !isfinite (seconds) || (seconds < 0.0) || (seconds >= 60.0)) {
    return (EXIT_FAILURE);
  }

  while (isspace ((unsigned char) *endptr)) endptr++;
  if (*endptr != '\0') return (EXIT_FAILURE);

  // Round to the nearest millisecond, then normalize a possible carry.
  total_in_minute = lround (seconds * 1000.0);
  if (total_in_minute >= 60000L) {
    total_in_minute -= 60000L;
    minutes++;
    if (minutes == 60) {
      minutes = 0;
      hours++;
      if (hours > INT_MAX) return (EXIT_FAILURE);
    }
  }

  *hh = (int) hours;
  *mm = (int) minutes;
  *ss = (int) (total_in_minute / 1000L);
  *ms = (int) (total_in_minute % 1000L);

  return (EXIT_SUCCESS);
}

// Parse an SSA/ASS color definition and write the equivalent SubRip font tag.
// Hexadecimal colors are BGR (&HBBGGRR) or ABGR (&HAABBGGRR). Decimal
// values are interpreted as the same 32-bit ABGR representation; negative
// decimal values are accepted using their two's-complement 32-bit form.
int
parse_color (const char *string, FILE *fo) {

  char digits[9];
  const char *p, *end;
  char *endptr;
  size_t ndigits;
  unsigned long hexvalue;
  long long decimal;
  uint32_t value;
  unsigned int r, g, b;

  if ((string == NULL) || (fo == NULL)) return (EXIT_FAILURE);

  while (isspace ((unsigned char) *string)) string++;
  if (*string == '\0') return (EXIT_FAILURE);

  if ((string[0] == '&') && ((string[1] == 'H') || (string[1] == 'h'))) {

    p = string + 2;
    end = p;
    while (isxdigit ((unsigned char) *end)) end++;
    ndigits = (size_t) (end - p);

    if ((ndigits != 6U) && (ndigits != 8U)) return (EXIT_FAILURE);

    while (isspace ((unsigned char) *end)) end++;
    if (*end == '&') {
      end++;
      while (isspace ((unsigned char) *end)) end++;
    }
    if (*end != '\0') return (EXIT_FAILURE);

    memcpy (digits, p, ndigits);
    digits[ndigits] = '\0';

    errno = 0;
    hexvalue = strtoul (digits, &endptr, 16);
    if ((errno == ERANGE) || (*endptr != '\0') || (hexvalue > UINT32_MAX)) {
      return (EXIT_FAILURE);
    }
    value = (uint32_t) hexvalue;

  } else {

    errno = 0;
    decimal = strtoll (string, &endptr, 10);
    if ((errno == ERANGE) || (endptr == string)) return (EXIT_FAILURE);

    while (isspace ((unsigned char) *endptr)) endptr++;
    if (*endptr != '\0') return (EXIT_FAILURE);

    if ((decimal < (long long) INT32_MIN) || (decimal > (long long) UINT32_MAX)) {
      return (EXIT_FAILURE);
    }

    if (decimal < 0) {
      value = (uint32_t) (int32_t) decimal;
    } else {
      value = (uint32_t) decimal;
    }
  }

  // SSA/ASS stores BGR in the low 24 bits.
  r = (unsigned int) (value & UINT32_C (0xff));
  g = (unsigned int) ((value >> 8) & UINT32_C (0xff));
  b = (unsigned int) ((value >> 16) & UINT32_C (0xff));

  if (fprintf (fo, "<font color=\"#%02X%02X%02X\">", r, g, b) < 0) {
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Fix erroneous SubRip tags in a line of text.
// Whitespace immediately after '<' or after '</' is removed only if a later
// '>' exists, so a literal '<' in subtitle text is left alone.
int
fix_text (char *text) {

  char temp[MAXLEN];
  size_t i, j, k, len;

  if (text == NULL) return (EXIT_FAILURE);

  len = strlen (text);
  i = 0U;
  j = 0U;

  while (i < len) {

    if (text[i] == '<') {
      k = i + 1U;
      while ((k < len) && (text[k] != '>')) k++;

      if (k < len) {
        if (j >= (MAXLEN - 1U)) return (EXIT_FAILURE);
        temp[j++] = '<';
        i++;

        while ((i < len) && (text[i] == ' ')) i++;
        if ((i < len) && (text[i] == '/')) {
          if (j >= (MAXLEN - 1U)) return (EXIT_FAILURE);
          temp[j++] = '/';
          i++;
          while ((i < len) && (text[i] == ' ')) i++;
        }
        continue;
      }
    }

    if (j >= (MAXLEN - 1U)) return (EXIT_FAILURE);
    temp[j++] = text[i++];
  }

  temp[j] = '\0';
  memcpy (text, temp, j + 1U);
  return (EXIT_SUCCESS);
}

// Convert SSA/ASS override markup embedded in one Dialogue Text field.
int
write_text (const char *text, char *temp, FILE *fo) {

  int i, j, len;
  int italflag, boldflag, underlineflag, strikeoutflag, colorflag;

  if ((text == NULL) || (temp == NULL) || (fo == NULL)) return (EXIT_FAILURE);

  len = (int) strlen (text);
  i = 0;
  colorflag = 0;
  boldflag = 0;
  italflag = 0;
  underlineflag = 0;
  strikeoutflag = 0;

  while (i < len) {

    // SSA/ASS explicit line break.
    if ((text[i] == '\\') && ((i + 1) < len) &&
        ((text[i+1] == 'n') || (text[i+1] == 'N'))) {
      if (fputc ('\n', fo) == EOF) return (EXIT_FAILURE);
      i += 2;

    // SSA/ASS hard space. SubRip has no direct equivalent.
    } else if ((text[i] == '\\') && ((i + 1) < len) && (text[i+1] == 'h')) {
      if (fputc (' ', fo) == EOF) return (EXIT_FAILURE);
      i += 2;

    // Override block.
    } else if (((i + 1) < len) && (text[i] == '{') && (text[i+1] == '\\')) {

      i += 2;  // Move past opening override marker.

      for (;;) {

        if (i >= len) {
          // Unterminated override block: all safely parseable content has
          // already been consumed, so stop at end of Dialogue Text.
          break;
        }

        if (strncmp (&text[i], "i1", 2) == 0) {
          italflag = 1;
          if (fputs ("<i>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;

        } else if (strncmp (&text[i], "i0", 2) == 0) {
          italflag = 0;
          if (fputs ("</i>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;

        } else if (strncmp (&text[i], "i}", 2) == 0) {
          italflag = 0;
          if (fputs ("</i>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;
          break;

        } else if (strncmp (&text[i], "b1", 2) == 0) {
          boldflag = 1;
          if (fputs ("<b>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;

        } else if (strncmp (&text[i], "b0", 2) == 0) {
          boldflag = 0;
          if (fputs ("</b>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;

        } else if (strncmp (&text[i], "u1", 2) == 0) {
          underlineflag = 1;
          if (fputs ("<u>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;

        } else if (strncmp (&text[i], "u0", 2) == 0) {
          underlineflag = 0;
          if (fputs ("</u>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;

        } else if (strncmp (&text[i], "s1", 2) == 0) {
          strikeoutflag = 1;
          if (fputs ("<s>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;

        } else if (strncmp (&text[i], "s0", 2) == 0) {
          strikeoutflag = 0;
          if (fputs ("</s>", fo) == EOF) return (EXIT_FAILURE);
          i += 2;

        // Primary font color override.
        } else if (strncmp (&text[i], "c&H", 3) == 0) {

          // A new color override replaces an earlier color override.
          if (colorflag && (fputs ("</font>", fo) == EOF)) return (EXIT_FAILURE);
          colorflag = 1;

          memset (temp, 0, MAXLEN * sizeof (char));
          i++;  // Move past c to the opening '&' of &H...
          j = 0;
          if ((i >= len) || (text[i] != '&')) return (EXIT_FAILURE);
          temp[j++] = text[i++];
          while ((i < len) && (text[i] != '&')) {
            if (j >= (MAXLEN - 2)) return (EXIT_FAILURE);
            temp[j++] = text[i++];
          }
          if (i >= len) return (EXIT_FAILURE);
          temp[j++] = text[i++];  // Copy terminating '&'.
          temp[j] = '\0';

          if (parse_color (temp, fo) != EXIT_SUCCESS) return (EXIT_FAILURE);

        // Primary font color off.
        } else if (strncmp (&text[i], "c}", 2) == 0) {
          if (colorflag && (fputs ("</font>", fo) == EOF)) return (EXIT_FAILURE);
          colorflag = 0;
          i += 2;
          break;

        // Alignment override. Preserve either \a or \an notation.
        } else if ((text[i] == 'a') && ((i + 1) < len) &&
                   ((text[i+1] == 'n') || isdigit ((unsigned char) text[i+1]))) {

          if (fputs ("{\\a", fo) == EOF) return (EXIT_FAILURE);
          i++;

          while ((i < len) && (text[i] != '}') && (text[i] != '\\')) {
            if (fputc (text[i], fo) == EOF) return (EXIT_FAILURE);
            i++;
          }

          if ((i < len) && (text[i] == '}')) {
            if (fputc ('}', fo) == EOF) return (EXIT_FAILURE);
            i++;
            break;
          }
          if ((i < len) && (text[i] == '\\')) {
            if (fputc ('}', fo) == EOF) return (EXIT_FAILURE);
            i++;
            continue;
          }
          break;

        // Position is not representable in SubRip; ignore it.
        } else if (strncmp (&text[i], "pos", 3) == 0) {
          i += 3;
          while ((i < len) && (text[i] != ')') && (text[i] != '\\') && (text[i] != '}')) i++;
          if ((i < len) && (text[i] == ')')) i++;

        // Other color and alpha overrides are not implemented in SubRip.
        } else if ((strncmp (&text[i], "1c&H", 4) == 0) ||
                   (strncmp (&text[i], "2c&H", 4) == 0) ||
                   (strncmp (&text[i], "3c&H", 4) == 0) ||
                   (strncmp (&text[i], "4c&H", 4) == 0) ||
                   (strncmp (&text[i], "alpha&H", 7) == 0) ||
                   (strncmp (&text[i], "1a&H", 4) == 0) ||
                   (strncmp (&text[i], "2a&H", 4) == 0) ||
                   (strncmp (&text[i], "3a&H", 4) == 0) ||
                   (strncmp (&text[i], "4a&H", 4) == 0)) {
          // Skip the complete &H...& value, not merely the opening ampersand.
          while ((i < len) && (text[i] != '&') && (text[i] != '\\') && (text[i] != '}')) i++;
          if ((i < len) && (text[i] == '&')) {
            i++;
            while ((i < len) && (text[i] != '&') && (text[i] != '\\') && (text[i] != '}')) i++;
            if ((i < len) && (text[i] == '&')) i++;
          }

        } else if (text[i] == '}') {
          i++;
          break;

        } else if (text[i] == '\\') {
          i++;
          continue;

        // Unknown/unimplemented override. Ignore it up to the next separator
        // or the end of the block rather than rejecting the whole subtitle.
        } else {
          while ((i < len) && (text[i] != '\\') && (text[i] != '}')) i++;
        }
      }

    } else if ((text[i] == '{') || (text[i] == '\\')) {

      // A literal brace/backslash that is not a recognized SSA escape or
      // override must still be consumed; otherwise this loop could stall.
      if (fputc (text[i], fo) == EOF) return (EXIT_FAILURE);
      i++;

    } else {

      // Ordinary subtitle text.
      while ((i < len) && (text[i] != '{') && (text[i] != '\\')) {
        if (fputc (text[i], fo) == EOF) return (EXIT_FAILURE);
        i++;
      }
    }
  }

  // Repair missing closing override tags at the end of this subtitle.
  if (strikeoutflag && (fputs ("</s>", fo) == EOF)) return (EXIT_FAILURE);
  if (underlineflag && (fputs ("</u>", fo) == EOF)) return (EXIT_FAILURE);
  if (italflag && (fputs ("</i>", fo) == EOF)) return (EXIT_FAILURE);
  if (boldflag && (fputs ("</b>", fo) == EOF)) return (EXIT_FAILURE);
  if (colorflag && (fputs ("</font>", fo) == EOF)) return (EXIT_FAILURE);

  return (EXIT_SUCCESS);
}
