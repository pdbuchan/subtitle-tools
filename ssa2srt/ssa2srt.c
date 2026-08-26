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

// ssa2srt.c - Read an existing SubStationAlpha (SSA) file and convert to SubRip output file.
// Transfers styles and markups for font color, bold, italic, underline, strikeout, and alignment.
// Only recognizes V4 and V4+ Styles. Ignores those SSA style attributes and override tags not
// implemented in SubRip format.

// WARNING: SSA files do not require subtitles to be in chronological order, whereas SubRip files
//          do require they appear in chronological order in the srt file.
//          The SubRip output file is not corrected for this.

// gcc -Wall ssa2srt.c -o ssa2srt -lm

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
int extract_int (const char *, int, int *);
int parse_time (const char *, int *, int *, int *, int *);
int parse_color (const char *, FILE *);
int fix_text (char *);
static void *allocate_mem (size_t, size_t, const char *);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
int *allocate_intmem (size_t);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters per line
#define MAXSTYLES 100 // Maximum number of styles that can be defined

// Byte Order Marks are at most four bytes long in the table below.
#define BOM_BUFFER_SIZE 4

int
main (int argc, char **argv) {

  int i, j, type, eofile, len, sub, shh, smm, sss, sms, ehh, emm, ess, ems;
  int clrstyle, boldstyle, italicstyle, underlinestyle, strikeoutstyle, alignstyle;
  int style, nstyles, iname, iclr, ibold, iital, iunderline, istrikeout, ialign, istart, iend, istyle, itext;
  int lineno, v4plus, events_found;
  int *stylebold, *styleitalic, *styleunderline, *stylestrikeout, *stylealign;
  int italflag, boldflag, underlineflag, strikeoutflag, colorflag;
  size_t nread;
  char *temp, *string, *text, **stylename, **styleclr;
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

  // Notes:
  //   An override tag embedded within subtitle text will override the corresponding Style attribute until the override closure tag,
  //   at which point the format reverts back to the Style value.
  // Alignments
  //   {\an#} are override markup tags with numpad alignment positions.
  //   {\a#} are override markup tags with 1,2,3 (bottom) with +4 (top) and +8 (mid)
  //   We assume SSA file Styles always use 1,2,3/+4/+8 format.
  // Italic override tags
  //   {\i1} italics on (use <i> for srt)
  //   {\i0} italics off (use </i> for srt); {\i} is also handled, as it is sometimes encountered.
  // Bold override tags
  //   {\b1} bold on (use <b> for srt)
  //   {\b0} bold off (use </b> for srt)
  // Underline override tags
  //   {\u1} underline on (use <u> for srt)
  //   {\u0} underline off (use </u> for srt)
  // Strikeout override tags
  //   {\s1} strikeout on (use <s> for srt)
  //   {\s0} strikeout off (use </s> for srt)

  // Process the command line arguments, if any.
  if (argc == 2) {
    filename = argv[1];
  } else {
    fprintf (stdout, "\nUsage: ./ssa2srt inputfilename\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);
  string = allocate_strmem (MAXLEN);
  text = allocate_strmem (MAXLEN);
  stylename = allocate_strmemp (MAXSTYLES);
  styleclr = allocate_strmemp (MAXSTYLES);
  for (i=0; i<MAXSTYLES; i++) {
    stylename[i] = allocate_strmem (MAXLEN);
    styleclr[i] = allocate_strmem (MAXLEN);
  }
  stylebold = allocate_intmem (MAXSTYLES);
  styleitalic = allocate_intmem (MAXSTYLES);
  styleunderline = allocate_intmem (MAXSTYLES);
  stylestrikeout = allocate_intmem (MAXSTYLES);
  stylealign = allocate_intmem (MAXSTYLES);

  // Open existing SubStationAlpha file.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubStationAlpha file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Examine the beginning of the file for a BOM before parsing any SSA
  // lines. Read up to the maximum BOM length; a short file is valid input.
  nread = fread (bom_buffer, sizeof (bom_buffer[0]), BOM_BUFFER_SIZE, fi);
  if (ferror (fi)) {
    fprintf (stderr, "ERROR: Unable to examine input SubStationAlpha file %s for a Byte Order Mark.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
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
      exit (EXIT_FAILURE);
    }
  }

  // Position the stream at the first byte of SSA text. For UTF-8 input, skip
  // its BOM so it never becomes part of the first line read from the file.
  if (fseek (fi, (type == 0) ? (long) bom[type].len : 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to position input SubStationAlpha file %s after Byte Order Mark detection.\n", filename);
    fclose (fi);
    exit (EXIT_FAILURE);
  }

  lineno = 0;

  // Search for V4 or V4+ Style definitions.
  do {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (read_ssa_line (fi, temp, MAXLEN, &lineno) == -1) {
      break;  // Reached end of file.
    }

  } while ((strncmp (temp, "[V4 Styles]", 11) != 0) && (strncmp (temp, "[V4+ Styles]", 12) != 0));

  v4plus = 0;
  if (strncmp (temp, "[V4 Styles]", 11) == 0)  {
    fprintf (stdout, "\nV4 Styles are defined.\n");
  } else if (strncmp (temp, "[V4+ Styles]", 12) == 0)  {
    v4plus = 1;
    fprintf (stdout, "\nV4+ Styles are defined.\n");
  } else {
    fprintf (stderr, "\nSSA file does not use [V4 Styles] or [V4+ Styles].\n");
    exit (EXIT_FAILURE);
  }

  // Find Format line of [V4 Styles] or [V4+ Styles] section.
  do {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (read_ssa_line (fi, temp, MAXLEN, &lineno) == -1) {
      fprintf (stderr, "ERROR: Could not find the V4 Styles Format line.\n");
      exit (EXIT_FAILURE);
    }
    if ((temp[0] == '[') && (strncmp (temp, "Format:", 7) != 0)) {
      fprintf (stderr, "ERROR: V4 Styles section has no Format line.\n");
      exit (EXIT_FAILURE);
    }

  } while (strncmp (temp, "Format:", 7) != 0);

  // Determine Style attribute indices in Style Format.
  // Other style attributes are ignored; many aren't implemented in SubRip (i.e., srt) specification.

  // Find Name index in Styles Format.
  if (!find_index (temp, "Name", &iname)) {
    fprintf (stderr, "ERROR: Cannot find Name in Styles Format: %s", temp);
    exit (EXIT_FAILURE);
  }

  // Find PrimaryColour index in Styles Format.
  if ((clrstyle = find_index (temp, "PrimaryColour", &iclr))) {
//    fprintf (stdout, "PrimaryColour index in Style definitions: %i\n", iclr);
  }

  // Find Bold index in Styles Format.
  if ((boldstyle = find_index (temp, "Bold", &ibold))) {
//    fprintf (stdout, "Bold index in Style definitions: %i\n", ibold);
  }

  // Find Italic index in Styles Format.
  if ((italicstyle = find_index (temp, "Italic", &iital))) {
//    fprintf (stdout, "Italic index in Style definitions: %i\n", iital);
  }

  // Find Underline index in Styles Format.
  if ((underlinestyle = find_index (temp, "Underline", &iunderline))) {
//    fprintf (stdout, "Underline index in Style definitions: %i\n", iunderline);
  }

  // Find StrikeOut index in Styles Format.
  if ((strikeoutstyle = find_index (temp, "StrikeOut", &istrikeout))) {
//    fprintf (stdout, "StrikeOut index in Style definitions: %i\n", istrikeout);
  }

  // Find Alignment index in Styles Format.
  if ((alignstyle = find_index (temp, "Alignment", &ialign))) {
//    fprintf (stdout, "Alignment index in Style definitions: %i\n", ialign);
  }

  nstyles = 0;  // Count of number of Style definitions.

  // Loop through Styles in SSA input file.
  for (;;)  {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (read_ssa_line (fi, temp, MAXLEN, &lineno) == -1) {
      break;  // Reached end of file.
    }

    if (strncmp (temp, "Style:", 6) == 0) {

      if (nstyles >= MAXSTYLES) {
        fprintf (stderr, "ERROR: More than %d Style definitions were found.\n", MAXSTYLES);
        exit (EXIT_FAILURE);
      }

      // Extract Style name. If a Style is defined, it must always have a name.
      // We assume there exists at least one Style definition.
      if (extract_string (temp, iname, ',', stylename[nstyles]) != EXIT_SUCCESS) {
        fprintf (stderr, "ERROR: Cannot extract Name from Style definition: %s", temp);
        exit (EXIT_FAILURE);
      }
//fprintf (stdout, "Stylename: %s\n", stylename[nstyles]);

      // Extract PrimaryColour setting for this Style.
      // Extract as string because format color values are expressed in decimal or hexadecimal.
      // Will be processed later by parse_color().
      if (clrstyle) {
        if (extract_string (temp, iclr, ',', styleclr[nstyles]) != EXIT_SUCCESS) {
          fprintf (stderr, "ERROR: Cannot extract PrimaryColour from Style definition: %s", temp);
          exit (EXIT_FAILURE);
        }
//fprintf (stdout, "  PrimaryColour: %s\n", styleclr[nstyles]);
      }

      // Extract Bold setting for this Style.
      // Bold index is ibold
      // -1 = bold on, 0 = bold off
      if (boldstyle) {
        if (extract_int (temp, ibold, &stylebold[nstyles]) != EXIT_SUCCESS) {
          fprintf (stderr, "ERROR: Cannot extract Bold from Style definition: %s", temp);
          exit (EXIT_FAILURE);
        }
//fprintf (stdout, "  Bold: %i\n", stylebold[nstyles]);
      }  // End if boldstyle

      // Extract Italic setting for this Style.
      // Italics index is iital
      // -1 = italics on, 0 = italics off
      if (italicstyle) {
        if (extract_int (temp, iital, &styleitalic[nstyles]) != EXIT_SUCCESS) {
          fprintf (stderr, "ERROR: Cannot extract Italic from Style definition: %s", temp);
          exit (EXIT_FAILURE);
        }
//fprintf (stdout, "  Italic: %i\n", styleitalic[nstyles]);
      }  // End if italicstyle

      // Extract Underline setting for this Style.
      // Underline index is iunderline
      // -1 = underline on, 0 = underline off
      if (underlinestyle) {
        if (extract_int (temp, iunderline, &styleunderline[nstyles]) != EXIT_SUCCESS) {
          fprintf (stderr, "ERROR: Cannot extract Underline from Style definition: %s", temp);
          exit (EXIT_FAILURE);
        }
//fprintf (stdout, "  Underline: %i\n", styleunderline[nstyles]);
      }  // End if underlinestyle

      // Extract StrikeOut setting for this Style.
      // StrikeOut index is istrikeout
      // -1 = strikeout on, 0 = strikeout off
      if (strikeoutstyle) {
        if (extract_int (temp, istrikeout, &stylestrikeout[nstyles]) != EXIT_SUCCESS) {
          fprintf (stderr, "ERROR: Cannot extract StrikeOut from Style definition: %s", temp);
          exit (EXIT_FAILURE);
        }
//fprintf (stdout, "  Strikeout: %i\n", stylestrikeout[nstyles]);
      }  // End if strikeoutstyle

      // Extract Alignment setting for this Style.
      // Alignment index is ialign
      if (alignstyle) {
        if (extract_int (temp, ialign, &stylealign[nstyles]) != EXIT_SUCCESS) {
          fprintf (stderr, "ERROR: Cannot extract Alignment from Style definition: %s", temp);
          exit (EXIT_FAILURE);
        }
//fprintf (stdout, "  Alignment: %i\n", stylealign[nstyles]);
      }  // End if alignstyle

      nstyles++;  // Next style

    // No more Styles defined.
    } else {
      break;
    }

  }  // Next line.

  if (nstyles == 0) {
    fprintf (stderr, "ERROR: No Style definitions were found.\n");
    exit (EXIT_FAILURE);
  }
  fprintf (stdout, "Number of Styles defined: %i\n", nstyles);

  // The first non-Style line may itself be the Events section header.
  events_found = (strncmp (temp, "[Events]", 8) == 0);

  // Find Events in SSA file.
  while (!events_found) {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (read_ssa_line (fi, temp, MAXLEN, &lineno) == -1) {
      break;  // Reached end of file.
    }
    if (strncmp (temp, "[Events]", 8) == 0) events_found = 1;
  }

  if (!events_found) {
    fprintf (stderr, "ERROR: Cannot find [Events] section.\n");
    exit (EXIT_FAILURE);
  }

  // Find Format line of Events Section.
  do {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (read_ssa_line (fi, temp, MAXLEN, &lineno) == -1) {
      fprintf (stderr, "ERROR: Could not find the Events Format line.\n");
      exit (EXIT_FAILURE);
    }
    if ((temp[0] == '[') && (strncmp (temp, "Format:", 7) != 0)) {
      fprintf (stderr, "ERROR: Events section has no Format line.\n");
      exit (EXIT_FAILURE);
    }

  } while (strncmp (temp, "Format:", 7) != 0);

    // Find Start index in Events Format.
    if (!find_index (temp, "Start", &istart)) {
      fprintf (stderr, "Cannot find Start in Events Format: %s\n", temp);
      exit (EXIT_FAILURE);
    } else {
//      fprintf (stdout, "Start index in Events Format: %i\n", istart);
    }

    // Find End index in Events Format.
    if (!find_index (temp, "End", &iend)) {
      fprintf (stderr, "Cannot find End in Events Format: %s\n", temp);
      exit (EXIT_FAILURE);
    } else {
//      fprintf (stdout, "End index in Events Format: %i\n", iend);
    }

    // Find Style index in Events Format.
    if (!find_index (temp, "Style", &istyle)) {
      fprintf (stderr, "Cannot find Style in Events Format: %s\n", temp);
      exit (EXIT_FAILURE);
    } else {
//      fprintf (stdout, "Style index in Events Format: %i\n", istyle);
    }

    // Find Text index in Events Format.
    if (!find_index (temp, "Text", &itext)) {
      fprintf (stderr, "Cannot find Text in Events Format: %s\n", temp);
      exit (EXIT_FAILURE);
    } else {
//      fprintf (stdout, "Text index in Events Format: %i\n", itext);
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
    exit (EXIT_FAILURE);
  }

  // Preserve a UTF-8 BOM when one was present in the input file.
  if (type == 0) {
    if (fwrite (bom[type].sequence, sizeof (uint8_t), bom[type].len, fo) != bom[type].len) {
      fprintf (stderr, "ERROR: Unable to write UTF-8 BOM to out.srt.\n");
      fclose (fo);
      exit (EXIT_FAILURE);
    }
  }

  // Loop through Dialogue events in SSA file and save subtitles to srt file.
  sub = 1;
  eofile = 0;
  while (!eofile) {

    // Find next Dialogue line.
    do {

      // Read line of text from input file.
      memset (temp, 0, MAXLEN * sizeof (char));
      if (read_ssa_line (fi, temp, MAXLEN, &lineno) == -1) {

        // Reached end of file.
        eofile = 1;
        break;
      }
    } while (strncmp (temp, "Dialogue:", 9) != 0);
    len = (int) strlen (temp);  // Length of Dialogue line
//fprintf (stdout, "Length of Dialogue line: %i\n", len);
    if (eofile) break;

    // Subtitle number.
    fprintf (fo, "%i\n", sub);
//fprintf (stdout, "%i\n", sub);

    // Extract Start time.
    if (extract_string (temp, istart, ',', string) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Cannot extract Start from Dialogue line: %s", temp);
      exit (EXIT_FAILURE);
    }
//fprintf (stdout, "Start: %s\n", string);
    if (parse_time (string, &shh, &smm, &sss, &sms) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Invalid SSA start timestamp '%s'.\n", string);
      exit (EXIT_FAILURE);
    }

    // Extract End time.
    if (extract_string (temp, iend, ',', string) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Cannot extract End from Dialogue line: %s", temp);
      exit (EXIT_FAILURE);
    }
//fprintf (stdout, "End: %s\n", string);
    if (parse_time (string, &ehh, &emm, &ess, &ems) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Invalid SSA end timestamp '%s'.\n", string);
      exit (EXIT_FAILURE);
    }

    // Write subtitle start and end times.
    fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", shh, smm, sss, sms, ehh, emm, ess, ems);

    // Extract Style name.
    if (extract_string (temp, istyle, ',', string) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Cannot extract Style from Dialogue line: %s", temp);
      exit (EXIT_FAILURE);
    }

    // A leading '*' is sometimes encountered before a Style name.
    if (string[0] == '*') {
      memmove (string, string + 1, strlen (string));
    }
//fprintf (stdout, "Style name: %s\n", string);

    // Let style be index of current stylename.
    for (style=0; style<nstyles; style++) {
      if (strcmp (string, stylename[style]) == 0) break;
    }
    if (style == nstyles) {
      fprintf (stderr, "ERROR: Dialogue refers to undefined Style '%s'.\n", string);
      exit (EXIT_FAILURE);
    }

    // Extract Text.
    if (extract_string (temp, itext, '\n', text) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: Cannot extract Text from Dialogue line: %s", temp);
      exit (EXIT_FAILURE);
    }

    // Correct erroneous SubRip tags in Text.
    fix_text (text);

    len = (int) strlen (text);  // Length of Text portion of Dialogue line.
//fprintf (stdout, "Text (ssa): %s\n", text);

    // Apply Styles

    // Apply PrimaryColour style of current Text by parsing styleclr string for current Dialogue's style.
    // These can be overridden by color tags in Text.
    if (clrstyle) {
      if (parse_color (styleclr[style], fo) != EXIT_SUCCESS) {
        fprintf (stderr, "ERROR: Invalid PrimaryColour '%s' for Style '%s'.\n", styleclr[style], stylename[style]);
        exit (EXIT_FAILURE);
      }
    }

    // Apply Alignment style of current Text. These can be overridden by alignment tags within Text.
    // It seems Styles use 1,2,3 +4 and +8 notation (i.e., not numpad).
    if (alignstyle) {

      // V4+ (ASS) uses numpad alignment values 1 through 9 directly.
      if (v4plus) {
        if ((stylealign[style] < 1) || (stylealign[style] > 9)) {
          fprintf (stderr, "ERROR: Unknown V4+ Style alignment value %i.\n", stylealign[style]);
          exit (EXIT_FAILURE);
        }
        if (stylealign[style] != 2) {
          fprintf (fo, "{\\an%i}", stylealign[style]);
        }

      // V4 SSA uses the older 1/2/3 plus 4 (top) and 8 (middle) scheme.
      } else {
        switch (stylealign[style]) {
          case 1:  fprintf (fo, "{\\an1}"); break;
          case 2:  break;  // Bottom-center is the normal SubRip default.
          case 3:  fprintf (fo, "{\\an3}"); break;
          case 5:  fprintf (fo, "{\\an7}"); break;
          case 6:  fprintf (fo, "{\\an8}"); break;
          case 7:  fprintf (fo, "{\\an9}"); break;
          case 9:  fprintf (fo, "{\\an4}"); break;
          case 10: fprintf (fo, "{\\an5}"); break;
          case 11: fprintf (fo, "{\\an6}"); break;
          default:
            fprintf (stderr, "ERROR: Unknown V4 Style alignment value %i.\n", stylealign[style]);
            exit (EXIT_FAILURE);
        }
      }
    }

    // Apply Bold style of current Text. This may be overridden by bold tags within Text.
    if ((boldstyle) && (stylebold[style] == -1)) {
      fprintf (fo, "<b>");
    }

    // Apply Italic style of current Text. This may be overridden by italics tags within Text.
    if ((italicstyle) && (styleitalic[style] == -1)) {
      fprintf (fo, "<i>");
    }

    // Apply Underline style of current Text. This may be overridden by underline tags within Text.
    if ((underlinestyle) && (styleunderline[style] == -1)) {
      fprintf (fo, "<u>");
    }

    // Apply StrikeOut style of current Text. This may be overridden by strikeout tags within Text.
    if ((strikeoutstyle) && (stylestrikeout[style] == -1)) {
      fprintf (fo, "<s>");
    }

    // Apply any override formatting embedded in Text.

    // Loop through chars of current Text.
    i = 0;  // Index of current Dialogue's Text, which is array text.
    colorflag = 0;  // Default to no color markup; this flag is needed in order to add </font> if {\c} missing from Text.
    boldflag = 0;
    italflag = 0;
    underlineflag = 0;
    strikeoutflag = 0;
    while (i < len) {

      // Line-feed found.
      if ((text[i] == '\\') && ((i + 1) < len) && ((text[i+1] == 'n') || (text[i+1] == 'N'))) {
        fprintf (fo, "\n");
        i += 2;

      // SSA hard space. SubRip has no direct equivalent, so use an ordinary space.
      } else if ((text[i] == '\\') && ((i + 1) < len) && (text[i+1] == 'h')) {
        fputc (' ', fo);
        i += 2;

      /* Process any override formats if { found.
         Override formats are formats embedded within Text which override Style formatting.
         All override formats begin with {\
         There could be multiple formats within {}, each preceded by a \
      */
      } else if (strncmp (&text[i], "{\\", 2) == 0) {

        i += 2;  /* Move past {\  */

        // Process all formats within current {}.
        for (;;) {

          // Italics on; overrides Italic style, if any.
          if (strncmp (&text[i], "i1", 2) == 0) {
            italflag = 1;
            fprintf (fo, "<i>");
            i += 2;  // Move past i1

          // Italics off; overrides Italic style, if any.
          } else if (strncmp (&text[i], "i0", 2) == 0) {
            italflag = 0;
            fprintf (fo, "</i>");
            i += 2;  // Move past i0

          // Italics off (shortform); overrides Italic style, if any.
          } else if (strncmp (&text[i], "i}", 2) == 0) {
            italflag = 0;
            fprintf (fo, "</i>");
            i += 2;  // Move past i}
            break;  // Leave {} loop.

          // Bold on; overrides Bold style, if any.
          } else if (strncmp (&text[i], "b1", 2) == 0) {
            boldflag = 1;
            fprintf (fo, "<b>");
            i += 2;  // Move past b1

          // Bold off; overrides Bold style, if any.
          } else if (strncmp (&text[i], "b0", 2) == 0) {
            boldflag = 0;
            fprintf (fo, "</b>");
            i += 2;  // Move past b0

          // Underline on; overrides Underline style, if any.
          } else if (strncmp (&text[i], "u1", 2) == 0) {
            underlineflag = 1;
            fprintf (fo, "<u>");
            i += 2;  // Move past u1

          // Underline off; overrides Underline style, if any.
          } else if (strncmp (&text[i], "u0", 2) == 0) {
            underlineflag = 0;
            fprintf (fo, "</u>");
            i += 2;  // Move past u0

          // StrikeOut on; overrides StrikeOut style, if any.
          } else if (strncmp (&text[i], "s1", 2) == 0) {
            strikeoutflag = 1;
            fprintf (fo, "<s>");
            i += 2;  // Move past s1

          // StrikeOut off; overrides StrikeOut style, if any.
          } else if (strncmp (&text[i], "s0", 2) == 0) {
            strikeoutflag = 0;
            fprintf (fo, "</s>");
            i += 2;  // Move past s0

          // Primary font color on; overrides PrimaryColour style, if any.
          } else if (strncmp (&text[i], "c&H", 3) == 0) {

            // A new color override replaces an earlier color override.
            if (colorflag) fprintf (fo, "</font>");
            colorflag = 1;

            // Copy color definition to temporary string.
            memset (temp, 0, MAXLEN * sizeof (char));
            i++;  // Move past c; temp begins with &H...
            j = 0;
            while ((i < len) && (text[i] != '&')) {
              if (j >= (MAXLEN - 2)) {
                fprintf (stderr, "ERROR: Color override is too long.\n");
                exit (EXIT_FAILURE);
              }
              temp[j++] = text[i++];
            }
            if (i >= len) {
              fprintf (stderr, "ERROR: Unterminated color override in text: %s\n", text);
              exit (EXIT_FAILURE);
            }
            temp[j++] = '&';
            temp[j] = '\0';
            i++;  // Move past terminating &
            if (parse_color (temp, fo) != EXIT_SUCCESS) {
              fprintf (stderr, "ERROR: Invalid color override '%s'.\n", temp);
              exit (EXIT_FAILURE);
            }

          // Font color off; overrides PrimaryColour style, if any.
          // Exit {} loop.
          } else if (strncmp (&text[i], "c}", 2) == 0) {
            colorflag = 0;
            fprintf (fo, "</font>");
            i += 2;  // Move past c}
            break;  // Leave {} loop.

          // Alignment; overrides Alignment style, if any.
          } else if (text[i] == 'a') {

            fprintf (fo, "{\\a");
            i++;  // Move past a

            // Copy alignment, since srt format is same as ssa format, until } or another \ is encountered.
            // Could be {\a or {\an type of alignment format.
            while (i < len) {
              if ((text[i] != '}') && (text[i] != '\\')) {
                fprintf (fo, "%c", text[i]);
                i++;
              } else {
                break;
              }
            }

            // No more formats within these {}.
            if (text[i] == '}') {
              fprintf (fo, "}");
              i++;  // Move past }
              break;  // Leave {} loop.

            // More formats are within these {}.
            } else if (text[i] == '\\') {
              fprintf (fo, "}");  // Close alignment format.
              i++;  /* Move past \   */
              continue;
            }

          // pos function override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "pos", 3) == 0) {
            i += 3;  // Move past pos
            // Move past location specification (#,#)
            for (;;) {
              if (i >= len) break;
              if (text[i] != ')') {
                i++;
              } else {
                i++;  // Move past )
                break;
              }
            }

          // Primary fill color override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "1c&H", 4) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // Secondary fill color override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "2c&H", 4) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // Border color override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "3c&H", 4) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // Shadow color override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "4c&H", 4) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // Alpha override; not implemented in srt, so ignore it.
          // Note that it is written "alpha" to differentiate from alignment formats.
          } else if (strncmp (&text[i], "alpha&H", 7) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // Primary fill alpha override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "1a&H", 4) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // Secondary fill alpha override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "2a&H", 4) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // Border alpha override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "3a&H", 4) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // Shadow alpha override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "4a&H", 4) == 0) {
            while ((i < len) && (text[i] != '&')) i++;
            if (i < len) i++;

          // No more formats within current {}.
          } else if (text[i] == '}') {
            i++;
            break;

          // More formats to follow within these {}.
          } else if (text[i] == '\\') {
            i++;  /* Move past \   */
            continue;

          // Unknown/unimplemented override format. Ignore it up to the next
          // format separator or the end of the override block.
          } else {
            while ((i < len) && (text[i] != '\\') && (text[i] != '}')) i++;
          }

        }  // End for loop within {}

      } else if ((text[i] == '{') || (text[i] == '\\')) {

        // A literal brace/backslash that is not a recognized SSA escape or
        // override must still be consumed; otherwise the loop would stall.
        fputc (text[i], fo);
        i++;
      }

      // Regular text is found.
      while (i < len) {
        if ((text[i] != '{') && (text[i] != '\\')) {
          fprintf (fo, "%c", text[i]);
          i++;
        } else {
          break;
        }
      }

    }  // End for loop through chars of Text

    // Fix any SSA input file formatting errors in current Text.

    // Strikeout: Add closing </s> if {\s0} is missing.
    if (strikeoutflag == 1) {
      fprintf (fo, "</s>");
    }

    // Underline: Add closing </u> if {\u0} is missing.
    if (underlineflag == 1) {
      fprintf (fo, "</u>");
    }

    // Italics: Add closing </i> if {\i0} or {\i} is missing.
    if (italflag == 1) {
      fprintf (fo, "</i>");
    }

    // Bold: Add closing </b> if {\b0} is missing.
    if (boldflag == 1) {
      fprintf (fo, "</b>");
    }

   // Font color: Add closing </font> if {\c} is missing.
    if (colorflag == 1) {
      fprintf (fo, "</font>");
    }

    // Close Style formats, if any.
    // Align Style is in SSA format in srt file and does not need closure.

    // Close StrikeOut style, if enabled.
    if (strikeoutstyle && (stylestrikeout[style] == -1)) {
      fprintf (fo, "</s>");
    }
    
    // Close Underline style, if enabled.
    if (underlinestyle && (styleunderline[style] == -1)) {
      fprintf (fo, "</u>");
    }

    // Close Italic style, if enabled.
    if (italicstyle && (styleitalic[style] == -1)) {
      fprintf (fo, "</i>");
    }

    // Close Bold style, if enabled.
    if (boldstyle && (stylebold[style] == -1)) {
      fprintf (fo, "</b>");
    }

    // Close PrimaryColour style, if enabled.
    if (clrstyle) {
      fprintf (fo, "</font>");
    }

    sub++;  // Increment subtitle number.
    fprintf (fo, "\n\n");

  }  // End while !eofile

  fprintf (stdout, "%i subtitles found in SSA input file.\n\n", sub - 1);

  // Check output and close files.
  if (ferror (fo)) {
    fprintf (stderr, "ERROR: Failed while writing output file out.srt.\n");
    exit (EXIT_FAILURE);
  }
  if (fclose (fi) != 0) {
    fprintf (stderr, "ERROR: Unable to close input file %s.\n", filename);
    exit (EXIT_FAILURE);
  }
  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.srt.\n");
    exit (EXIT_FAILURE);
  }

  // Free allocated memory.
  free (temp);
  free (string);
  free (text);
  for (i = 0; i < MAXSTYLES; i++) {
    free (stylename[i]);
    free (styleclr[i]);
  }
  free (stylename);
  free (styleclr);
  free (stylebold);
  free (styleitalic);
  free (styleunderline);
  free (stylestrikeout);
  free (stylealign);

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

// Read one SSA line and convert readline() errors into useful diagnostics.
// EOF is returned to the caller because it is expected in several search loops.
int
read_ssa_line (FILE *fi, char *line, int limit, int *lineno) {

  int status;

  if (lineno == NULL) {
    fprintf (stderr, "ERROR: Invalid line-number pointer in read_ssa_line().\n");
    exit (EXIT_FAILURE);
  }

  status = readline (fi, line, limit);
  if (status == 0) {
    (*lineno)++;
    return (0);
  }
  if (status == -1) {
    return (-1);
  }
  if (status == -2) {
    fprintf (stderr, "ERROR: Line %i of the SSA file does not fit in the %d-byte input buffer.\n", *lineno + 1, limit);
    exit (EXIT_FAILURE);
  }

  fprintf (stderr, "ERROR: Unable to read line %i of the SSA file.\n", *lineno + 1);
  exit (EXIT_FAILURE);
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

  // Skip the conventional single space after "Format:", "Style:", etc.
  if (*p == ' ') p++;

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

// Extract an integer element from a comma-separated line, given index.
int
extract_int (const char *text, int index, int *value) {

  char string[MAXLEN];
  char *endptr;
  long parsed;

  if ((text == NULL) || (value == NULL)) return (EXIT_FAILURE);
  if (extract_string (text, index, ',', string) != EXIT_SUCCESS) return (EXIT_FAILURE);

  errno = 0;
  parsed = strtol (string, &endptr, 10);
  if ((errno == ERANGE) || (endptr == string)) return (EXIT_FAILURE);

  while (isspace ((unsigned char) *endptr)) endptr++;
  if ((*endptr != '\0') || (parsed < INT_MIN) || (parsed > INT_MAX)) {
    return (EXIT_FAILURE);
  }

  *value = (int) parsed;
  return (EXIT_SUCCESS);
}

// Parse a timestamp in SSA/ASS form h:mm:ss.cc.
// The seconds parser also accepts one or three fractional digits and converts
// the result to milliseconds for SubRip output.
int
parse_time (const char *string, int *hh, int *mm, int *ss, int *ms) {

  const char *p;
  char *endptr;
  long hours, minutes;
  double seconds;
  long total_in_minute;

  if ((string == NULL) || (hh == NULL) || (mm == NULL) ||
      (ss == NULL) || (ms == NULL)) {
    return (EXIT_FAILURE);
  }

  p = string;

  errno = 0;
  hours = strtol (p, &endptr, 10);
  if ((errno == ERANGE) || (endptr == p) || (*endptr != ':') ||
      (hours < 0) || (hours > INT_MAX)) {
    return (EXIT_FAILURE);
  }
  p = endptr + 1;

  errno = 0;
  minutes = strtol (p, &endptr, 10);
  if ((errno == ERANGE) || (endptr == p) || (*endptr != ':') ||
      (minutes < 0) || (minutes > 59)) {
    return (EXIT_FAILURE);
  }
  p = endptr + 1;

  errno = 0;
  seconds = strtod (p, &endptr);
  if ((errno == ERANGE) || (endptr == p) || !isfinite (seconds) ||
      (seconds < 0.0) || (seconds >= 60.0)) {
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
// Note that < or > could appear in a SubRip (i.e., srt) file as intended displayable subtitle text
// so we check if a < is eventually followed by a >. We do not compare against all possible tag names.
int
fix_text (char *text) {

  int i, j, k, len;
  char *temp;

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);

  len = (int) strlen (text);

  // Whitespace is not allowed between < and tag name, and </ and tag name.
  i = 0;  // Index of text
  j = 0;  // Index of temp
  while (i < len) {

    // Found a possible opening of SubRip format tag.
    if (text[i] == '<') {
      temp[j] = text[i];
      i++;  // Move past <
      j++;

      // Check if a > later follows.
      k = i;
      while (k < len) {

        // Found a >, so we probably do have a markup tag.
        if (text[k] == '>') {

          // Skip any whitespace before tag name or /
          while (i < len) {
            if (text[i] == ' ') {
              i++;
            } else {
              break;
            }
          }

          // Skip any whitespace after / but before tag name.
          if (text[i] == '/') {
            temp[j] = text[i];
            i++;  // Move past /
            j++;
            while (i < len) {
              if (text[i] == ' ') {
                i++;
              } else {
                break;
              }
            }
          }

          break;
        }  // End if found >

        k++;
      }  // End while looking for >
    }  // End if found <

    temp[j] = text[i];
    i++;
    j++;
  }

  // Replace text with cleaned-up version in temp.
  memset (text, 0, MAXLEN * sizeof (char));
  memcpy (text, temp, MAXLEN * sizeof (char));

  // Free allocated memory.
  free (temp);

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
