/*  Copyright (C) 2024-2025 P. David Buchan (pdbuchan@gmail.com)

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

// gcc -Wall ssa2srt.c -o ssa2srt

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>  // For modf
#include <errno.h>

// Function prototypes
int readline (FILE *, char *, int);
int find_index (char *, char *, int *);
int extract_string (char *, int, char, char *);
int extract_int (char *, int, int *);
int parse_time (char *, int *, int *, int *, int *);
int parse_color (char *, FILE *);
int fix_text (char *);
char *allocate_strmem (int);
char **allocate_strmemp (int);
int *allocate_intmem (int);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters per line
#define MAXSTYLES 100 // Maximum number of styles that can be defined

int
main (int argc, char **argv) {

  int i, j, eofile, len, sub, shh, smm, sss, sms, ehh, emm, ess, ems;
  int clrstyle, boldstyle, italicstyle, underlinestyle, strikeoutstyle, alignstyle;
  int style, nstyles, iname, iclr, ibold, iital, iunderline, istrikeout, ialign, istart, iend, istyle, itext;
  int *stylebold, *styleitalic, *styleunderline, *stylestrikeout, *stylealign;
  int italflag, boldflag, underlineflag, strikeoutflag, colorflag;
  char *temp, *filename, *string, *text, **stylename, **styleclr, *format;
  FILE *fi, *fo;

  // Notes:
  //   An override tag embedded within subtitle text will override the corresponding Style attribute until the override closure tag,
  //   at which point the format reverts back to the Style value.
  // Alignments
  //   {\an#} are override markup tags with numpad alignment positions.
  //   {\a#} are override markup tags with 1,2,3 (bottom) with +4 (top) and +8 (mid)
  //   We assume SSA file Styles always use 1,2,3/+4/+8 format.
  // Italic override tags
  //   {\i1} italics on (use <i> for srt)
  //   {\i0} italics off (use <\i> for srt); {\i} is also handled, as it is sometimes encountered.
  // Bold override tags
  //   {\b1} bold on (use <b> for srt)
  //   {\b0} bold off (use </b> for srt)
  // Underline override tags
  //   {\u1} bold on (use <u> for srt)
  //   {\u0} bold off (use </u> for srt)
  // Strikeout override tags
  //   {\s1} strikeout on (use <s> for srt)
  //   {\s0} strikeout off (use </s> for srt)

  // Allocate memory for various arrays.
  filename = allocate_strmem (MAXLEN);

  // Process the command line arguments, if any.
  if (argc == 2) {
    strncpy (filename, argv[1], MAXLEN);
  } else {
    fprintf (stdout, "\nUsage: ./ssa2srt inputfilename\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    free (filename);
    return (EXIT_SUCCESS);
  }

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);
  string = allocate_strmem (MAXLEN);
  text = allocate_strmem (MAXLEN);
  format = allocate_strmem (MAXLEN);
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
  fi = fopen (filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input SubStationAlpha file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Search for V4 or V4+ Style definitions.
  do {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (readline (fi, temp, MAXLEN) == -1) {
      break;  // Reached end of file.
    }

  } while ((strncmp (temp, "[V4 Styles]", 11) != 0) && (strncmp (temp, "[V4+ Styles]", 12) != 0));

  if (strncmp (temp, "[V4 Styles]", 11) == 0)  {
    fprintf (stdout, "\nV4 Styles are defined.\n");
  } else if (strncmp (temp, "[V4+ Styles]", 12) == 0)  {
    fprintf (stdout, "\nV4+ Styles are defined.\n");
  } else {
    fprintf (stderr, "\nSSA file does not use [V4 Styles] or [V4+ Styles].\n");
    exit (EXIT_FAILURE);
  }

  // Find Format line of [V4 Styles] or [V4+ Styles] section.
  do {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (readline (fi, temp, MAXLEN) == -1) {
      fprintf (stderr, "ERROR: Could not file the V4 Styles Format line.\n");
      exit (EXIT_FAILURE);
    }

  } while (strncmp (temp, "Format:", 7) != 0);

  // Determine Style attribute indices in Style Format.
  // Other style attributes are ignored; many aren't implemented in SubRip (i.e., srt) specification.
  len = strnlen (temp, MAXLEN);

  // Find Name index in Styles Format.
  if (find_index (temp, "Name", &iname)) {
//    fprintf (stdout, "Name index in Style definitions: %i\n", iname);
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
    if (readline (fi, temp, MAXLEN) == -1) {
      break;  // Reached end of file.
    }

    if (strncmp (temp, "Style:", 6) == 0) {

      // Extract Style name. If a Style is defined, it must always have a name.
      // We assume there exists at least one Style definition.
      extract_string (temp, iname, ',', stylename[nstyles]);
//fprintf (stdout, "Stylename: %s\n", stylename[nstyles]);

      // Extract PrimaryColour setting for this Style.
      // Extract as string because format color values are expressed in decimal or hexadecimal.
      // Will be processed later by parse_color().
      if (clrstyle) {
        extract_string (temp, iclr, ',', styleclr[nstyles]);
        strcat (styleclr[nstyles], "&");
//fprintf (stdout, "  PrimaryColour: %s\n", styleclr[nstyles]);
      }

      // Extract Bold setting for this Style.
      // Bold index is ibold
      // -1 = bold on, 0 = bold off
      if (boldstyle) {
        extract_int (temp, ibold, &stylebold[nstyles]);
//fprintf (stdout, "  Bold: %i\n", stylebold[nstyles]);
      }  // End if boldstyle

      // Extract Italic setting for this Style.
      // Italics index is iital
      // -1 = italics on, 0 = italics off
      if (italicstyle) {
        extract_int (temp, iital, &styleitalic[nstyles]);
//fprintf (stdout, "  Italic: %i\n", styleitalic[nstyles]);
      }  // End if italicstyle

      // Extract Underline setting for this Style.
      // Underline index is iunderline
      // -1 = underline on, 0 = underline off
      if (underlinestyle) {
        extract_int (temp, iunderline, &styleunderline[nstyles]);
//fprintf (stdout, "  Underline: %i\n", styleunderline[nstyles]);
      }  // End if underlinestyle

      // Extract StrikeOut setting for this Style.
      // StrikeOut index is istrikeout
      // -1 = strikeout on, 0 = strikeout off
      if (strikeoutstyle) {
        extract_int (temp, istrikeout, &stylestrikeout[nstyles]);
//fprintf (stdout, "  Strikeout: %i\n", stylestrikeout[nstyles]);
      }  // End if strikeoutstyle

      // Extract Alignment setting for this Style.
      // Alignment index is ialign
      if (alignstyle) {
        extract_int (temp, ialign, &stylealign[nstyles]);
//fprintf (stdout, "  Alignment: %i\n", stylealign[nstyles]);
      }  // End if alignstyle

      nstyles++;  // Next style

    // No more Styles defined.
    } else {
      break;
    }

  }  // Next line.
fprintf (stdout, "Number of Styles defined: %i\n", nstyles);

  // Find Events in SSA file.
  do {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (readline (fi, temp, MAXLEN) == -1) {
      break;  // Reached end of file.
    }

  } while (strncmp (temp, "[Events]", 10) != 0);

  // Find Format line of Events Section.
  do {

    // Read line of text from input file.
    memset (temp, 0, MAXLEN * sizeof (char));
    if (readline (fi, temp, MAXLEN) == -1) {
      fprintf (stderr, "ERROR: Could not file the Events Format line.\n");
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

  // Open output file.
  fo = fopen ("out.srt", "r");
  if (fo != NULL) {
    fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    exit (EXIT_FAILURE);
  }
  fo = fopen ("out.srt", "w");
  if (fo == NULL) {
    fprintf (stderr, "ERROR: Unable to open output file out.srt.\n");
    exit (EXIT_FAILURE);
  }

  // Loop through Dialogue events in SSA file and save subtitles to srt file.
  sub = 1;
  eofile = 0;
  while (!eofile) {

    // Find next Dialogue line.
    do {

      // Read line of text from input file.
      memset (temp, 0, MAXLEN * sizeof (char));
      if (readline (fi, temp, MAXLEN) == -1) {

        // Reached end of file.
        eofile = 1;
        break;
      }
    } while (strncmp (temp, "Dialogue: ", 10) != 0);
    len = strnlen (temp, MAXLEN);  // Length of Dialogue line
//fprintf (stdout, "Length of Dialogue line: %i\n", len);
    if (eofile) break;

    // Subtitle number.
    fprintf (fo, "%i\n", sub);
//fprintf (stdout, "%i\n", sub);

    // Extract Start time.
    i = 0;  // Index of temp
    j = 0;  // Comma count
    while ((j != istart) && (i < len)) {
      if (temp[i] == ',') j++;
      i++;
    }
    memset (string, 0, MAXLEN * sizeof (char));
    j = 0;  // Index of string
    while (i < len) {
      if (temp[i] != ',') {
        string[j] = temp[i];
        i++;
        j++;
      } else {
        break;
      }
    }
//fprintf (stdout, "Start: %s\n", string);
    parse_time (string, &shh, &smm, &sss, &sms);

    // Extract End time.
    i = 0;  // Index of temp
    j = 0;  // Comma count
    while ((j != iend) && (i < len)) {
      if (temp[i] == ',') j++;
      i++;
    }
    memset (string, 0, MAXLEN * sizeof (char));
    j = 0;  // Index of string
    while (i < len) {
      if (temp[i] != ',') {
        string[j] = temp[i];
        i++;
        j++;
      } else {
        break;
      }
    }
//fprintf (stdout, "End: %s\n", string);
    parse_time (string, &ehh, &emm, &ess, &ems);

    // Write subtitle start and end times.
    fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", shh, smm, sss, sms, ehh, emm, ess, ems);

    // Extract Style name.
    i = 0;  // Index of temp
    j = 0;  // Comma count
    while ((j != istyle) && (i < len)) {
      if (temp[i] == ',') j++;
      i++;
    }
    memset (string, 0, MAXLEN * sizeof (char));
    j = 0;  // Index of string
    while (i < len) {
      if (temp[i] != ',') {

        // Ignore is there is a leading * (don't know why this sometimes appears).
        if (temp[i] == '*') {
          i++;

        // Copy style name.
        } else {
          string[j] = temp[i];
          i++;
          j++;
        }
      } else {
        break;
      }
    }
//fprintf (stdout, "Style name: %s\n", string);

    // Let style be index of current stylename.
    for (style=0; style<nstyles; style++) {
      if (strcmp (string, stylename[style]) == 0) break;
    }

    // Extract Text.
    extract_string (temp, itext, '\n', text);

    // Correct erroneous SubRip tags in Text.
    fix_text (text);

    len = strnlen (text, MAXLEN);  // Length of Text portion of Dialogue line.
//fprintf (stdout, "Text (ssa): %s\n", text);

    // Apply Styles

    // Apply PrimaryColour style of current Text by parsing styleclr string for current Dialogue's style.
    // These can be overridden by color tags in Text.
    parse_color (styleclr[style], fo);

    // Apply Alignment style of current Text. These can be overridden by alignment tags within Text.
    // It seems Styles use 1,2,3 +4 and +8 notation (i.e., not numpad).
    if (alignstyle) {
      switch (stylealign[style]) {
        case 1:
          fprintf (fo, "{\\an1}");  // Bottom-left
          break;
        case 2:  // This should be defaulted to by media player.
//          fprintf (fo, "{\\an2}");  // Bottom-center
          break;
        case 3:
          fprintf (fo, "{\\an3}");  // Bottom-right
          break;
        case 5:
          fprintf (fo, "{\\an7}");  // Top-left
          break;
        case 6:
          fprintf (fo, "{\\an8}");  // Top-center
          break;
        case 7:
          fprintf (fo, "{\\an9}");  // Top-right
          break;
        case 9:
          fprintf (fo, "{\\an4}");  // Middle-left
          break;
        case 10:
          fprintf (fo, "{\\an5}");  // Middle-center
          break;
        case 11:
          fprintf (fo, "{\\an6}");  // Middle-right
          break;
        default:
          fprintf (stderr, "ERROR: Unknown style alignment stylealign[istyle] = %i.\n", stylealign[style]);
          exit (EXIT_FAILURE);
      }  // End switch(stylealign[style])
    }  // End if alignstyle

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
    while (i < len) {

      // Initialize flags used for correcting SSA override format errors of current Text line.
      // Sometimes closing override tags are missing.
      boldflag = 0;
      italflag = 0;
      underlineflag = 0;
      strikeoutflag = 0;

      // Line-feed found.
      if ((text[i] == '\\') && ((text[i+1] == 'n') || (text[i+1] == 'N'))) {
          fprintf (fo, "\n");
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
            colorflag = 1;

            // Copy color definition to temporary string.
            memset (temp, 0, MAXLEN * sizeof (char));
            i++;  // Move past c
            j = 0;  // Index of temp
            do {
              temp[j] = text[i];
              i++;
              j++;
              if (i == len) break;
            } while (text[i] != '&');
            temp[j] = '&';
            i++;
            parse_color (temp, fo);  // Parse color string; save to output file.

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
            i += 5 + 6 + 1;  // Move past \1c&Hbbggrr&

          // Secondary fill color override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "\\2c&H", 5) == 0) {
            i += 4 + 6 + 1;  // Move past 2c&Hbbggrr&

          // Border color override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "3c&H", 4) == 0) {
            i += 4 + 6 + 1;  // Move past 3c&Hbbggrr&

          // Shadow color override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "4c&H", 4) == 0) {
            i += 4 + 6 + 1;  // Move past 4c&Hbbggrr&

          // Alpha override; not implemented in srt, so ignore it.
          // Note that it is written "alpha" to differentiate from alignment formats.
          } else if (strncmp (&text[i], "alpha&H", 7) == 0) {
            i += 7 + 2 + 1;  // Move past alpha&Haa&

          // Primary fill alpha override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "1a&H", 4) == 0) {
            i += 4 + 2 + 1;  // Move past 1a&HFF&

          // Secondary fill alpha override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "2a&H", 4) == 0) {
            i += 4 + 2 + 1;  // Move past 2a&HFF&

          // Border alpha override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "3a&H", 4) == 0) {
            i += 4 + 2 + 1;  // Move past 3a&HFF&

          // Shadow alpha override; not implemented in srt, so ignore it.
          } else if (strncmp (&text[i], "4a&H", 4) == 0) {
            i += 4 + 2 + 1;  // Move past 4a&HFF&

          // No more formats within current {}.
          } else if (text[i] == '}') {
            i++;
            break;

          // More formats to follow within these {}.
          } else if (text[i] == '\\') {
            i++;  /* Move past \   */
            continue;

          // Unknown format encountered within {}.
          } else {
            fprintf (stderr, "Unknown SSA format encountered in text:\n");
            fprintf (stderr, "%s\n", text);
            exit (EXIT_FAILURE);
          }

        }  // End for loop within {}

      }  // End if SSA format found.

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
    if (stylestrikeout[style] == -1) {
      fprintf (fo, "</s>");
    }
    
    // Close Underline style, if enabled.
    if (styleunderline[style] == -1) {
      fprintf (fo, "</u>");
    }

    // Close Italic style, if enabled.
    if (styleitalic[style] == -1) {
      fprintf (fo, "</i>");
    }

    // Close Bold style, if enabled.
    if (stylebold[style] == -1) {
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

  // Close input and output files.
  fclose (fi);
  fclose (fo);

  // Free allocated memory.
  free (temp);
  free (filename);
  free (string);
  free (text);
  free (format);
  for (i=0; i<MAXSTYLES; i++) {
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

// Read a single line of text from a text file.
// Returns -1 if EOF is encountered.
int
readline (FILE *fi, char *line, int limit) {

  int i, n;

  i = 0;  // i is pointer to byte in line.
  while (i < limit) {

    // Grab next byte from file.
    n = fgetc (fi);

    // End of file reached.
    // Tell calling function, by returning -1, that we're at end of file, so it won't call readline() again.
    if (n == EOF) {

      // If there's no end of line at the end of the file, ensure string termination.
      if (i > 0) {
        line[i] = 0;
        return (0);
      }
      return (-1);
    }

    // Found a carriage return. Ignore it.
    if (n == '\r') {
      continue;
    }

    // Found a newline. Change to 0 for string termination.
    // Break out of loop since this is the end of the current line.
    if (n == '\n') {
      line[i] = 0;  // Replace with 0 for string termination.
      return (0);
    }

    // Seems to be a valid character. Keep it.
    line[i] = n;
    i++;
  }

  // Advance to next line.
  n = 0;
  while ((n != '\n') && (n != EOF)) {
    n = fgetc (fi);
  }

  return (0);
}

// Find index of element within line of comma-separated text.
// Returns 0 if not found, 1 if found.
int
find_index (char *text, char *element, int *index) {

  int i, j, len, elemlen, found;

  len = strnlen (text, MAXLEN);
  elemlen = strnlen (element, MAXLEN);

  // Find index.
  found = 0;  // Default to element not found.
  *index = 0;
  i = 0;  // Index of temp
  while (i < len) {
    if (strncmp (&text[i], element, elemlen) == 0) {
      found = 1;
      break;
    }
    i++;
  }  // End while
  if (found) {
    for (j=i; j>=0; j--) {
      if (text[j] == ',') (*index)++;
    }
  }  // End if found

  return (found);
}

// Extract a string element from a comma-separated line, given index.
int
extract_string (char *text, int index, char termination, char *string) {

  int i, j, c, len;

  len = strnlen (text, MAXLEN);

  i = 0;  // Index of text

  // Skip line title ("Format:", Dialogue:", etc.)
  while (i < len) {
    if (text[i] != ':') {
      i++;
    } else {
      break;
    }
  }
  i += 2;  // Move past ": "

  // Locate element using index.
  j = 0;  // Index of string
  c = 0;  // Count of commas
  while ((i < len) && (c < index)) {
    if (text[i] == ',') c++;
    i++;
  }

  // Extract element until next comma or end of line is encountered.
  memset (string, 0, MAXLEN * sizeof (char));

  while (i < len) {
    if ((text[i] != termination) && (text[i] != '\n')) {
      string[j] = text[i];
      i++;
      j++;
    } else {
      break;
    }
  }

  return (EXIT_SUCCESS);
}

// Extract an integer element from a comma-separated line, given index.
int
extract_int (char *text, int index, int *value) {

  int i, j, c, len;
  char *string, *endptr;

  // Allocate memory for various arrays.
  string = allocate_strmem (MAXLEN);

  len = strnlen (text, MAXLEN);
  
  i = 0;  // Index of text

  // Skip line title ("Format:", Dialogue:", etc.)
  while (i < len) {
    if (text[i] != ':') {
      i++;
    } else {
      break;
    }
  }
  i += 2;  // Move past ": "
  
  // Locate element using index.
  j = 0;  // Index of string
  c = 0;  // Count of commas
  while ((i < len) && (c < index)) {
    if (text[i] == ',') c++;
    i++;
  }

  // Extract element until next comma is encountered.
  memset (string, 0, MAXLEN * sizeof (char));

  while (i < len) {
    if (text[i] != ',') {
      string[j] = text[i];
      i++;
      j++;
    } else {
      break;
    }
  }

  errno = 0;
  (*value) = (int) strtol (string, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == string)) {
    fprintf (stderr, "ERROR: Cannot make integer of string: %s\n", string);
    exit (EXIT_FAILURE);
  }

  // Free allocated memory.
  free (string);

  return (EXIT_SUCCESS);
}

// Parse string containing timestamp in SSA format.
int
parse_time (char *string, int *hh, int *mm, int *ss, int *ms) {

  int i, j, len;
  double dsec, integral, fractional;
  char *temp, *endptr;

  temp = allocate_strmem (MAXLEN);

  len = strnlen (string, MAXLEN);

  i = 0;  // Index of string

  // Extract hours.
  j = 0;  // Index of temp
  while (i < len) {
    if (string[i] != ':') {
      temp[j] = string[i];
      i++;
      j++;
    } else {
      break;
    }
  }
  errno = 0;
  *hh = (int) strtol (temp, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == string)) {
    fprintf (stderr, "ERROR: Cannot make integer of string: %s\n", temp);
    fprintf (stderr, "       %s\n", string);
    exit (EXIT_FAILURE);
  }
  i++;

  // Extract minutes.
  memset (temp, 0, MAXLEN * sizeof (char));
  j = 0;  // Index of temp
  while (i < len) {
    if (string[i] != ':') {
      temp[j] = string[i];
      i++;
      j++;
    } else {
      break;
    }
  }
  errno = 0;
  *mm = (int) strtol (temp, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == string)) {
    fprintf (stderr, "ERROR: Cannot make integer of string: %s\n", temp);
    fprintf (stderr, "       %s\n", string);
    exit (EXIT_FAILURE);
  }
  i++;

  // Extract seconds and milliseconds.
  memset (temp, 0, MAXLEN * sizeof (char));
  j = 0;  // Index of temp
  while (i < len) {
    if (string[i] != ',') {
      temp[j] = string[i];
      i++;
      j++;
    } else {
      break;
    }
  }
  errno = 0;
  dsec = strtod (temp, &endptr);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == string)) {
    fprintf (stderr, "ERROR: Cannot make double of string: %s\n", temp);
    fprintf (stderr, "       %s\n", string);
    exit (EXIT_FAILURE);
  }
  fractional = modf (dsec, &integral);
  *ms = (int) ((fractional * 1000.0) + 0.0001);
  *ss = (int) integral;

  // Free allocated memory.
  free (temp);

  return (EXIT_SUCCESS);
}

// Parse string containing color definition.
// Color definition is extracted as string because format color values are expressed in decimal or hexadecimal.
//   Hexadecimal:
//     BGR format as &HBBGGRR& or ABGR (with alpha channel) as &HAABBGGRR&. 
//   Decimal:
//     - Convert to Hexadecimal: e.g., The decimal value 65535 converts to hex as 0000FFFF.
//     - Extract Color Components:
//     - Alpha (A): The first two characters (00), indicating fully transparent.
//     - Blue (B): The next two characters (00), indicating no blue.
//     - Green (G): The next two characters (FF), indicating full green.
//     - Red (R): The last two characters (FF), indicating full red.
//     - Combining these components, the color represented by 65535 is a fully opaque yellow (since red + green = yellow).
int
parse_color (char *string, FILE *fo) {

  int i, c, dec, len;
  char *hexstring, *endptr;

  // Allocate memory for various arrays.
  hexstring = allocate_strmem (MAXLEN);

  len = strnlen (string, MAXLEN);

  fprintf (fo, "<font color=\"#");

  // Hexadecimal format.
  if (strncmp (string, "&H", 2) == 0) {

    i = 2;  // Index of string; skip &H
    c = 0;
    while (i < len) {
      if (string[i] != '&') {
        c++;  // Count digits
        i++;
      } else {
        break;
      }
    }

    // Hexadecimal BGR format.
    i = 0;  // Index of string;
    if (c == 6) {
      i += 2;  // Move past &H

    // Hexadecimal ABGR format.
    // Ignore alpha since it isn't implemented in SubRip format.
    } else if (c == 8) { 
      i += 4;  // Move past &HAA

    } else {  
      fprintf (stderr, "ERROR: Unknown color definition: %s\n", string);
      exit (EXIT_FAILURE);
            
    }  // End if BGR or ABGR

    // Extract as RGB for SubRip.
    fprintf (fo, "%c", string[i+4]);  // R left-digit
    fprintf (fo, "%c", string[i+5]);  // R right-digit
    fprintf (fo, "%c", string[i+2]);  // G left-digit
    fprintf (fo, "%c", string[i+3]);  // G right-digit
    fprintf (fo, "%c", string[i]);    // B left-digit
    fprintf (fo, "%c", string[i+1]);  // B right-digit
    fprintf (fo, "\">");

  // Decimal format.
  } else {
    i = 0;  // Index of string
    errno = 0;
    dec = (int) strtol (string, &endptr, 10);
    if ((errno == ERANGE) || (errno == EINVAL) || (endptr == string)) {
      fprintf (stderr, "ERROR: Cannot make integer of string: %s\n", string);
      exit (EXIT_FAILURE);
    }
    sprintf (hexstring, "%08X", dec);  // As hexadecimal ABGR format.

    // Extract as RGB for SubRip.
    i += 2;  // Ignore alpha since it isn't implemented in SubRip format.
    fprintf (fo, "%c", hexstring[i+4]);  // R left-digit
    fprintf (fo, "%c", hexstring[i+5]);  // R right-digit
    fprintf (fo, "%c", hexstring[i+2]);  // G left-digit
    fprintf (fo, "%c", hexstring[i+3]);  // G right-digit
    fprintf (fo, "%c", hexstring[i]);    // B left-digit
    fprintf (fo, "%c", hexstring[i+1]);  // B right-digit
    fprintf (fo, "\">");

  }  // End if hexadecimal or decimal color definition

  // Free allocated memory.
  free (hexstring);

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

  len = strnlen (text, MAXLEN);

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

  // Free allocae memory.
  free (temp);

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

  tmp = (char *) malloc (len * sizeof (char));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (char));
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

  tmp = (char **) malloc (len * sizeof (char *));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (char *));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmemp().\n");
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

  tmp = (int *) malloc (len * sizeof (int));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (int));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_intmem().\n");
    exit (EXIT_FAILURE);
  }
}
