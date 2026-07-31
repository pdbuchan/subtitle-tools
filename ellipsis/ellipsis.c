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
//              " ...\n"...\n"
//              "...\n ...\n"
//              or bogus ellipsis marks:
//              "---\n"
//              "---\n---\n" 
//              " ---\n"---\n"
//              "---\n ---\n"
//              If a Byte Order Mark (BOM) exists in the SubRip file containing the desired text, it will be included in the output file.
//              Write a new SubRip file.

// gcc -Wall ellipsis.c -o ellipsis

// Run without command line arguments to see usage notes.
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>  // uint8_t
#include <string.h>
#include <errno.h>

// Definition of structs
typedef struct {
  int len;
  char *name;
  uint8_t *sequence;
} BOM;

// Function prototypes
int readline (FILE*, char*, int);
int byteordermark (char *, BOM *);
char *allocate_strmem (int);
char **allocate_strmemp (int);
int *allocate_intmem (int);
BOM *allocate_bommem (int);

// Set some symbolic constants.
#define MAXLEN 1024  // Maximum number of characters in a string
#define MAXLINES 10 // Maximum number of lines per subtitle
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, c, type, alllines, nlines, line, nsubs, sub, *sublines, *skiplist, nskip;
  char *temp, *filename, **input, **time, **text;
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
  if (argc == 2) {
    strncpy (filename, argv[1], MAXLEN);

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
  alllines = 0;  // Count of lines
  while (readline (fi, temp, MAXLEN) != -1) {
    alllines++;
  }
  fprintf (stdout, "\n%i lines found including any excess trailing line-feeds.\n", alllines);
  rewind (fi);

  // Allocate memory for array to hold input file.
  input = allocate_strmemp (alllines);
  for (line = 0; line < alllines; line++) {
    input[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line = 0; line < alllines; line++) {
    if (readline (fi, input[line], MAXLEN) == -1) {
      fprintf (stderr, "\nERROR: Cannot read line %i from input SubRip file %s.\n", line + 1, filename);
      exit (EXIT_FAILURE);
    }
  }  // Next line

  // Close input file.
  fclose (fi);

  // Remove excess line-feeds at end of array input.
  nlines = alllines;
  for (line = alllines; line > 1; line--) {
    if ((input[line - 1][0] == '\n') && (input[line - 2][0] == '\n')) {
      nlines--;
    } else {
      break;
    }
  }
  fprintf (stdout, "%i lines found excluding trailing line-feeds.\n", nlines);

  // Detect any Byte Order Mark (BOM) at beginning of first line.
  type = byteordermark (input[0], bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nByte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);
  }

  // Count number of subtitles in SubRip file; assume at least one.
  nsubs = 0;
  for (line = 0; line < nlines; line++) {

    nsubs++;

    // Advance through to next subtitle number, if there is one.
    // End of current subtitle is demarcated by a line containing only a line-feed.
    while (input[line][0] != '\n') {
      line++;
      if (line == nlines) break;
    }

  }  // Next sub
  fprintf (stdout, "\n%i subtitles found.\n\n", nsubs);

  // Allocate memory for various arrays.
  text = allocate_strmemp (nsubs);
  time = allocate_strmemp (nsubs);
  for (i = 0; i < nsubs; i++) {
    text[i] = allocate_strmem (MAXLEN);
    time[i] = allocate_strmem (MAXLEN);
  }
  sublines = allocate_intmem (nsubs);
  skiplist = allocate_intmem (nsubs);

  // Loop through all subtitles.
  line = 0;  // Line index of input file
  for (sub = 0; sub < nsubs; sub++) {

    // Skip line with sub number.
    line++;

    // Extract start and end times.
    strncpy (time[sub], input[line], MAXLEN);
    line++;

    // Advance through to next subtitle number, if there is one.
    // End of current subtitle is demarcated by a line containing only a line-feed.
    sublines[sub] = 0;
    while (input[line][0] != '\n') {
      strncat (text[sub], input[line], MAXLEN);
      line++;
      sublines[sub]++;  // Increment count of lines for this sub.
      if (line == nlines) break;
    }

    // End of sub line-feed.
    line++;
  }

  // Loop through all subs making a list of subtitles containing only "...".
  nskip = 0;  // Index of skiplist array.
  for (sub = 0; sub < nsubs; sub++) {

    // If subtitle is a single line with "...", add sub to skiplist.
    if (((sublines[sub] == 1) && (strncmp (text[sub], "...\n", MAXLEN) == 0)) ||
        ((sublines[sub] == 2) && (strncmp (text[sub], "...\n...\n", MAXLEN) == 0)) ||
        ((sublines[sub] == 2) && (strncmp (text[sub], "...\n ...\n", MAXLEN) == 0)) ||
        ((sublines[sub] == 2) && (strncmp (text[sub], " ...\n...\n", MAXLEN) == 0)) ||
        // Bogus ellipsis marks
        ((sublines[sub] == 1) && (strncmp (text[sub], "---\n", MAXLEN) == 0)) ||
        ((sublines[sub] == 2) && (strncmp (text[sub], "---\n---\n", MAXLEN) == 0)) ||
        ((sublines[sub] == 2) && (strncmp (text[sub], "---\n ---\n", MAXLEN) == 0)) ||
        ((sublines[sub] == 2) && (strncmp (text[sub], " ---\n---\n", MAXLEN) == 0))) {
      skiplist[nskip] = sub;
      nskip++;
    }

  }  // Next subtitle

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

  // Write subtitles to output file with any duplicates combined.
  sub = 0;  // Original subtitle number
  c = 0;  // New subtitle number
  i = 0;  // Index of skiplist
  do {

    // Don't copy sub if it's in our skiplist.
    if (nskip > 0) {
      if (skiplist[i] == sub) {
        i++;
        sub++;
        continue;
      }
    }

    // Write subtitle number.
    fprintf (fo, "%i\n", c + 1);

    // Write start timestamp.
    fprintf (fo, "%s", time[sub]);

    // Write subtitle text.
    fprintf (fo, "%s\n", text[sub]);

    // Next subtitle
    sub++;
    c++;

  } while (sub < nsubs);

  // Close output file.
  fclose (fo);

  fprintf (stdout, "%i subtitles written.\n\n", c);

  // Free allocated memory.
  free (temp);
  free (bom);
  free (filename);
  for (line = 0; line < alllines; line++) {
    free (input[line]);
  }
  free (input);
  for (i=0; i<nsubs; i++) {
    free (time[i]);
    free (text[i]);
  }
  free (time);
  free (text);
  free (sublines);
  free (skiplist);

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
