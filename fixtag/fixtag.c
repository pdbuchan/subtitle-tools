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

// fixtag.c - Read an existing SubRip (srt) file and look for and fix some common markup tag errors.
//            Tags included: italics, bold, underline, strikethrough, font color, font size, position

//            Note: Media players read font tags as nested. For example:

//            1
//            00:00:01,000 --> 00:00:49,000
//            <font color="yellow"><font color="blue"><font color="red">THIS IS RED </font>THIS IS BLUE </font>THIS IS YELLOW</font>

// gcc -Wall fixtag.c -o fixtag

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h> 
#include <inttypes.h>  // uint8_t
#include <string.h>

// Definition of structs
typedef struct {
  int len;
  char *name;
  uint8_t *sequence;
} BOM;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (char *, BOM *);
int searchandreplace (char *, const char *, const char *);
int counttags (char *, const char *);
char *allocate_strmem (int);
char **allocate_strmemp (int);
int *allocate_intmem (int);
BOM *allocate_bommem (int);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters per line
#define MAXLINES 10  // Maximum number of lines of text per subtitle
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, type, alllines, index, nlines, line, nsubs, sub;
  int nital, nbold, nunderline, nstrikeout, ncolor, nsize, nfontclose, closeoption;
  char *temp, *filename, **input, **text, *alltext;
  BOM *bom;
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
  closeoption = 0;  // Default to not adding any missing closing tags.
  if (argc == 2) {
    strncpy (filename, argv[1], MAXLEN);

  } else if ((argc == 3) && (strncmp (argv[2], "close", 8) == 0)) {
    strncpy (filename, argv[1], MAXLEN);
    closeoption = 1;

  } else {
    fprintf (stdout, "\nUsage: ./fixtag inputfilename.srt [close]\n\n");
    fprintf (stdout, "       close option: Append any missing markup closure tags to last line of subtitle text.\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    free (filename);
    return (EXIT_SUCCESS);
  }

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);
  bom = allocate_bommem (MAXBOM);
  text = allocate_strmemp (MAXLINES);
  for (i=0; i<MAXLINES; i++) {
    text[i] = allocate_strmem (MAXLEN);
  }
  alltext = allocate_strmem (MAXLEN * MAXLINES);
  
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
    fprintf (stderr, "\nERROR: Unable to open input SubRip file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllines = 0;  // Count of lines
  while (readline (fi, temp, MAXLEN) != -1) {
    alllines++;
  }
  fprintf (stdout, "\n%i lines found including any excess trailing line-feeds.\n", alllines);
  rewind (fi);

  // Allocate memory for array to hold input file.
  input = allocate_strmemp (alllines + 1);  // Add 1 in case we need to replace a missing line-feed at end.
  for (line=0; line<(alllines + 1); line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0; line<alllines; line++) {
    if (readline (fi, input[line], MAXLEN) == -1) {
      fprintf (stderr, "\nERROR: Cannot read line %i from input SubRip file %s.\n", line + 1, filename);
      exit (EXIT_FAILURE);
    }
  }  // Next line

  // Close input file.
  fclose (fi);

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
  // Add one if missing.
  if (input[nlines - 1][0] != '\n') {
    nlines++;
    input[nlines - 1][0] = '\n';
    fprintf (stdout, "WARNING: Final closing line-feed for last subtitle was missing but was corrected.\n");
  } else {
    fprintf (stdout, "%i lines found excluding excess trailing line-feeds.\n", nlines);
  }

  // Detect any Byte Order Mark (BOM) at beginning of first line.
  type = byteordermark (input[0], bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);
  }

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
  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Replace all line-feeds with string termination.
  for (line=0; line<nlines; line++) {
    input[line][strnlen (input[line], MAXLEN) - 1] = 0;
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

  // Write Byte Order Mark (BOM) to output file if detected in input file.
  if (type != -1) {
    fwrite (bom[type].sequence, bom[type].len * sizeof (uint8_t), 1, fo);
  }

  // Loop through all subtitles.
  index = 0;  // Line index of input file.
  for (sub=0; sub<nsubs; sub++) {

    // Write subtitle number to output file.
    fprintf (fo, "%i\n", sub + 1);
    index++;

    // Write line containing start and end times to output file.
    fprintf (fo, "%s\n", input[index]);
    index++;

    // Clear all text from text array.
    for (i=0; i<MAXLINES; i++) {
      memset (text[i], 0, MAXLEN * sizeof (char));
    }

    // Extract text lines for current subtitle.
    // End of current subtitle is demarcated by a line containing only a line-feed.
    nlines = 0;  // Count of number of lines of text.
    while (input[index][0] != 0) {
      sprintf (text[nlines], "%s", input[index]);
      nlines++;
      index++;
    }

    // Move to next subtitle.
    index++;

    // For current sub, initialize counts of missing tags for each category.
    nital = 0;
    nbold = 0;
    nunderline = 0;
    nstrikeout = 0;
    ncolor = 0;
    nsize = 0;
    nfontclose = 0;

    // Clear buffer used to contain all lines of text for this subtitle.
    memset (alltext, 0, MAXLEN * MAXLINES * sizeof (char));

    // Loop through all lines of text of current subtitle.
    // Search-and-replace malformed markup tags.
    for (line=0; line<nlines; line++) {

      // Italics - malformed
      searchandreplace (text[line], "< i>", "<i>");
      searchandreplace (text[line], "<i >", "<i>");
      searchandreplace (text[line], "< i >", "<i>");
      searchandreplace (text[line], "<i>", "<i>");  // Fix <I>

      searchandreplace (text[line], "</ i>", "</i>");
      searchandreplace (text[line], "</i >", "</i>");
      searchandreplace (text[line], "< /i>", "</i>");
      searchandreplace (text[line], "< / i>", "</i>");
      searchandreplace (text[line], "< / i >", "</i>");
      searchandreplace (text[line], "</i>", "</i>");  // Fix </I>

      // Bold - malformed
      searchandreplace (text[line], "< b>", "<b>");
      searchandreplace (text[line], "<b >", "<b>");
      searchandreplace (text[line], "< b >", "<b>");
      searchandreplace (text[line], "<b>", "<b>");  // Fix <B>

      searchandreplace (text[line], "</ b>", "</b>");
      searchandreplace (text[line], "</b >", "</b>");
      searchandreplace (text[line], "< /b>", "</b>");
      searchandreplace (text[line], "< / b>", "</b>");
      searchandreplace (text[line], "< / b >", "</b>");
      searchandreplace (text[line], "</b>", "</b>");  // Fix </B>

      // Underline - malformed
      searchandreplace (text[line], "< u>", "<u>");
      searchandreplace (text[line], "<u >", "<u>");
      searchandreplace (text[line], "< u >", "<u>");
      searchandreplace (text[line], "<u>", "<u>");  // Fix <U>

      searchandreplace (text[line], "</ u>", "</u>");
      searchandreplace (text[line], "</u >", "</u>");
      searchandreplace (text[line], "< /u>", "</u>");
      searchandreplace (text[line], "< / u>", "</u>");
      searchandreplace (text[line], "< / u >", "</u>");
      searchandreplace (text[line], "</u>", "</u>");  // Fix </U>

      // Strikeout - malformed
      searchandreplace (text[line], "< s>", "<s>");
      searchandreplace (text[line], "<s >", "<s>");
      searchandreplace (text[line], "< s >", "<s>");
      searchandreplace (text[line], "<s>", "<s>");  // Fix <S>

      searchandreplace (text[line], "</ s>", "</s>");
      searchandreplace (text[line], "</s >", "</s>");
      searchandreplace (text[line], "< /s>", "</s>");
      searchandreplace (text[line], "< / s>", "</s>");
      searchandreplace (text[line], "< / s >", "</s>");
      searchandreplace (text[line], "</s>", "</s>");  // Fix </S>

      // Font color - malformed
      searchandreplace (text[line], "< font color=", "<font color=");
      searchandreplace (text[line], "<font color =", "<font color=");
      searchandreplace (text[line], "<font color= ", "<font color=");
      searchandreplace (text[line], "<font color = ", "<font color=");
      searchandreplace (text[line], "< font color = ", "<font color=");
      searchandreplace (text[line], "<fontcolor=", "<font color=");
      searchandreplace (text[line], "<font color=", "<font color=");  // Fix uppercase mistakes.

      // Font size - malformed
      searchandreplace (text[line], "< font size=", "<font size=");
      searchandreplace (text[line], "<font size =", "<font size=");
      searchandreplace (text[line], "<font size= ", "<font size=");
      searchandreplace (text[line], "<font size = ", "<font size=");
      searchandreplace (text[line], "< font size = ", "<font size=");
      searchandreplace (text[line], "<fontsize=", "<font size=");
      searchandreplace (text[line], "<font size=", "<font size=");  // Fix uppercase mistakes.

      // Closing font - malformed
      searchandreplace (text[line], "</ font>", "</font>");
      searchandreplace (text[line], "</font >", "</font>");
      searchandreplace (text[line], "< /font>", "</font>");
      searchandreplace (text[line], "< /font >", "</font>");
      searchandreplace (text[line], "</font>", "</font>");  // Fix uppercase mistakes.

      // Position - malformed
      searchandreplace (text[line], "{ \\an", "{\\an");
      searchandreplace (text[line], "{\\ an", "{\\an");
      searchandreplace (text[line], "{\\an ", "{\\an");
      searchandreplace (text[line], "{ \\ an", "{\\an");
      searchandreplace (text[line], "{ \\ an ", "{\\an");
      searchandreplace (text[line], "{\\an", "{\\an");  // Fix uppercase mistakes.

      searchandreplace (text[line], "1 }", "1}");  // Bottom-left
      searchandreplace (text[line], "2 }", "2}");  // Bottom-center
      searchandreplace (text[line], "3 }", "3}");  // Bottom-right
      searchandreplace (text[line], "4 }", "4}");  // Middle-left
      searchandreplace (text[line], "5 }", "5}");  // Middle-center
      searchandreplace (text[line], "6 }", "6}");  // Middle-right
      searchandreplace (text[line], "7 }", "7}");  // Top-left
      searchandreplace (text[line], "8 }", "8}");  // Top-center
      searchandreplace (text[line], "9 }", "9}");  // Top-right

      // Italics - count missing closure tag(s)
      nital += (counttags (text[line], "<i>") - counttags (text[line], "</i>"));

      // Bold - count missing closure tag(s)
      nbold += (counttags (text[line], "<b>") - counttags (text[line], "</b>"));

      // Underline - count missing closure tag(s)
      nunderline += (counttags (text[line], "<u>") - counttags (text[line], "</u>"));

      // Strikeout - count missing closure tag(s)
      nstrikeout += (counttags (text[line], "<s>") - counttags (text[line], "</s>"));

      // Font color - count opening font color tag(s)
      ncolor += counttags (text[line], "<font color=");

      // Font size - count opening font size tag(s)
      nsize += counttags (text[line], "<font size=");

      // Font closure - count font closure tag(s)
      nfontclose += counttags (text[line], "</font>");

      // Add lines to buffer alltext.
      strncat (alltext, text[line], MAXLEN);
      strcat (alltext, "\n");

    }  // Next line of current subtitle

    // if requested, append missing closing tag(s) to last text line of current subtitle.
    if (closeoption) {
      alltext[strnlen (alltext, MAXLEN * MAXLINES) - 1] = 0;  // Remove final linefeed.
      for (i=0; i<nital; i++) strcat (alltext, "</i>");
      for (i=0; i<nbold; i++) strcat (alltext, "</b>");
      for (i=0; i<nunderline; i++) strcat (alltext, "</u>");
      for (i=0; i<nstrikeout; i++) strcat (alltext, "</s>");
      for (i=0; i<(ncolor + nsize - nfontclose); i++) strcat (alltext, "</font>");
      strcat (alltext, "\n");  // Add back final linefeed.
    }

    // Write corrected text lines to output file.
    fprintf (fo, "%s", alltext);
    fprintf (fo, "\n");

  }  // Next subtitle

  // Close output file.
  fclose (fo);

  // Free allocated memory.
  free (temp);
  free (bom);
  free (filename);
  for (line=0; line<(alllines + 1); line++) {
    free (input[line]);
  }
  free (input);
  for (i=0; i<MAXLINES; i++) {
    free (text[i]);
  }
  free (text);
  free (alltext);

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

    // Seems to be a valid character. Keep it.
    line[i] = n;
    i++;

    // Found a newline.
    // Break out of loop since this is the end of the current line.
    if (n == '\n') { 
      return (0);
    }

  }

  // Advance to next line.
  n = 0;
  while ((n != '\n') && (n != EOF)) {
    n = fgetc (fi);
  }

  return (0);
}

// Detect Byte Order Mark (BOM), if it exists, at beginning of line.
// Return index of bom array corresponding to type of BOM detected,
// or return -1 if none (or unlisted type) detected.
int
byteordermark (char *text, BOM *bom) {

  int type, i, found;

  // Loop through all types of Byte Order Marks.
  for (type=0; type<MAXBOM; type++) {

    found = 1;  // Default to current type detected.
    for (i=0; i<bom[type].len; i++) {
      if ((uint8_t) text[i] != bom[type].sequence[i]) found = 0;
    }

    // We found a match.
    if (found) return (type);
  }

  // Failed to find a match.
  return (-1);
}

// Search a string for a subtring and replace with a new substring.
// Ignores case of string and old substring.
int
searchandreplace (char *string, const char *oldsub, const char *newsub) {

  int oldlen, newlen, lenbeforematch, iresult;
  char *currentpos, *matchpos, *result;

  // Allocate memory for various arrays.
  result = allocate_strmem (MAXLEN);

  // Edge case check: If the oldsub is empty, we don't do anything.
  if (oldsub == NULL || *oldsub == '\0' || string == NULL) {
    return (EXIT_SUCCESS);
  }

  // Calculate lengths of the old and new substrings.
  oldlen = strnlen (oldsub, MAXLEN);
  newlen = strnlen (newsub, MAXLEN);

  currentpos = string;
  iresult = 0;  // Index within result, the newly constructed string.

  while ((matchpos = strcasestr (currentpos, oldsub)) != NULL) {

    // Copy the part of the original string before the match.
    lenbeforematch = matchpos - currentpos;
    strncpy (result + iresult, currentpos, lenbeforematch);  // Won't be > MAXLEN
    iresult += lenbeforematch;

    // Copy the new substring into the result.
    strncpy (result + iresult, newsub, newlen);
    iresult += newlen;

    // Move the current position pointer past the old substring.
    currentpos = matchpos + oldlen;
  }

  // Copy the remaining part of the string after the last match.
  strncpy (result + iresult, currentpos, MAXLEN - iresult);

  // Copy the result back into the original string.
  memset (string, 0, MAXLEN * sizeof (char));
  strncpy (string, result, MAXLEN);

  // Free allocated memory.
  free (result);

  return (EXIT_SUCCESS);
}

// Count number of occurrences of tag in line of subtitle text.
int
counttags (char *string, const char *tag) {

  int stringlen, position, count;
  char *p;

  stringlen = strnlen (string, MAXLEN * MAXLINES);

  // Count occurrences of tag.
  count = 0;
  position = 0;
  while (((p = strcasestr (string + position, tag)) != NULL) && (position < stringlen)) {
    position = p - string + strnlen (tag, MAXLEN);
    count++;
  }

  return (count);
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

// Allocate memory for an array of BOM (Byte Order Mark) structs.
BOM *
allocate_bommem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_bommem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (BOM *) malloc (len * sizeof (BOM));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (BOM));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_bommem().\n");
    exit (EXIT_FAILURE);
  }
}
