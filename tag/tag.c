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

// tag.c - Take formatting tags from one SubRip (.srt) file and text from another SubRip file and create a new SubRip file.
//         If a Byte Order Mark (BOM) exists in the SubRip file containing the desired text, it will be included in the output file.
//         NOTE: The two .srt files must have same number of subtitles and same number of lines per subtitle.
//               Since text will likely be different between srt files, tag.c only works for opening tags that appear at the
//               beginning of a line and closing tags that appear at the end of a line.

// gcc -Wall tag.c -o tag

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
int readline (FILE*, char*, int);
int byteordermark (char *, BOM *);
void copytags (FILE *, char *, int *);
int *allocate_intmem (int);
char *allocate_strmem (int);
char **allocate_strmemp (int);
BOM *allocate_bommem (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, type, alllinestag, alllinestext, nlinestag, nlinestext, line, nsubs, sub, *ntext, tagpos, textpos;
  char *temp, *tagfilename, *textfilename, **inputtag, **inputtext;
  BOM *bom;
  FILE *fi,*fo;

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
  tagfilename = allocate_strmem (MAXLEN);
  textfilename = allocate_strmem (MAXLEN);

  // Process the command line arguments, if any.
  if (argc == 3) {
    strncpy (tagfilename, argv[1], MAXLEN);
    strncpy (textfilename, argv[2], MAXLEN);

  } else {
    fprintf (stdout, "\nUsage: ./tag taginputfilename.srt textinputfilename.srt\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    free (tagfilename);
    free (textfilename);
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

  // Open existing srt file with desired formatting tags.
  fi = fopen (tagfilename, "r");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input srt file %s with desired formatting.\n", tagfilename);
    exit (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllinestag = 0;  // Count of lines
  while (readline (fi, temp, MAXLEN) != -1) {
    alllinestag++;
  }
  fprintf (stdout, "\n%s: %i lines found including any excess trailing line-feeds.\n", tagfilename, alllinestag);
  rewind (fi);

  // Allocate memory for array to hold input file.
  inputtag = allocate_strmemp (alllinestag);
  for (line=0; line<alllinestag; line++) {
    inputtag[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0; line<alllinestag; line++) {
    if (readline (fi, inputtag[line], MAXLEN) == -1) {
      fprintf (stderr, "\nERROR: Cannot read line %i from input SubRip file %s.\n", line + 1, tagfilename);
      exit (EXIT_FAILURE);
    }
  }  // Next line

  // Close input file.
  fclose (fi);

  // Remove excess line-feeds at end of array input.
  nlinestag = alllinestag;
  for (line=alllinestag; line>1; line--) {
    if ((inputtag[line - 1][0] == '\n') && (inputtag[line - 2][0] == '\n')) {
      nlinestag--;
    } else {
      break;
    }
  }
  fprintf (stdout, "%s: %i lines found excluding trailing line-feeds.\n", tagfilename, nlinestag);

  // Open existing srt file with desired text.
  fi = fopen (textfilename, "r");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input srt file %s with desired text.\n", textfilename);
    exit (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllinestext = 0;  // Count of lines
  while (readline (fi, temp, MAXLEN) != -1) {
    alllinestext++;
  }
  fprintf (stdout, "\n%s: %i lines found including any excess trailing line-feeds.\n", textfilename, alllinestext);
  rewind (fi);

  // Allocate memory for array to hold input file.
  inputtext = allocate_strmemp (alllinestext);
  for (line=0; line<alllinestext; line++) {
    inputtext[line] = allocate_strmem (MAXLEN);
  }

  // Read input SubRip file into array input.
  for (line=0; line<alllinestext; line++) {
    if (readline (fi, inputtext[line], MAXLEN) == -1) {
      fprintf (stderr, "\nERROR: Cannot read line %i from input SubRip file %s.\n", line + 1, textfilename);
      exit (EXIT_FAILURE);
    }
  }  // Next line

  // Close input file.
  fclose (fi);

  // Remove excess line-feeds at end of array input.
  nlinestext = alllinestext;
  for (line=alllinestext; line>1; line--) {
    if ((inputtext[line - 1][0] == '\n') && (inputtext[line - 2][0] == '\n')) {
      nlinestext--;
    } else {
      break;
    }
  }
  fprintf (stdout, "%s: %i lines found excluding trailing line-feeds.\n", textfilename, nlinestext);

  // Detect any Byte Order Mark (BOM) at beginning of first line of SubRip file with desired text.
  type = byteordermark (inputtext[0], bom);
  if (type < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", textfilename);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n", textfilename, bom[type].name);
  }

  // Count number of subtitles in SubRip file with desired formatting tags; assume at least one.
  nsubs = 0;
  for (line=0; line<nlinestag; line++) {

    nsubs++;

    // Advance through to next subtitle number, if there is one.
    // End of current subtitle is demarcated by a line containing only a line-feed.
    while (inputtag[line][0] != '\n') {
      line++;
      if (line == nlinestag) break;
    }

  }  // Next sub
  fprintf (stdout, "\n%i subtitles found in %s.\n", nsubs, tagfilename);

  // Count number of subtitles in SubRip file with desired text; assume at least one.
  i = 0;
  for (line=0; line<nlinestext; line++) {

    i++;

    // Advance through to next subtitle number, if there is one.
    // End of current subtitle is demarcated by a line containing only a line-feed.
    while (inputtext[line][0] != '\n') {
      line++;
      if (line == nlinestext) break;
    }

  }  // Next sub
  fprintf (stdout, "\n%i subtitles found in %s.\n\n", i, textfilename);
  if (i != nsubs) {
    fprintf (stderr, "ERROR: Files %s and %s have different numbers of subtitles.\n", tagfilename, textfilename);
    exit (EXIT_FAILURE);
  }

  // Allocate memory for various arrays.
  ntext = allocate_intmem (nsubs);  // Number of lines of text per subtitle

  // Count number of lines of text for each subtitle by examining the formatted srt file.
  line = 0;  // Line index of formatted srt file
  for (sub=0; sub<nsubs; sub++) {

    // Skip sub number and timestamp.
    line += 2;

    // Count number of lines of text for current subtitle.
    ntext[sub] = 0;
    while (inputtag[line][0] != '\n') {
      ntext[sub]++;
      line++;
      if (line == nlinestag) break;
    }

    line++;  // Skip line-feed

  }  // Next sub

  // Open output file.
  fo = fopen ("out.srt", "r");
  if (fo != NULL) {
    fprintf (stderr, "ERROR: Output out.srt file already exists.\n");
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

  i = 0;  // Line index of SubRip files.
  // Loop through all subs.
  for (sub=0; sub<nsubs; sub++) {

    // Write sub number to output file.
    fprintf (fo, "%i\n", sub + 1);
    i++;  // Next line

    // Copy timestamp to output file.
    fprintf (fo, "%s", inputtext[i]);
    i++;  // Next line

    // Loop through all lines of text for current subtitle.
    for (line=0; line<ntext[sub]; line++) {

      // Copy opening tags at beginning of line, if present.
      tagpos = 0;  // Character index of line of formatted srt file.
      if (inputtag[i][tagpos] == '<') {
        copytags (fo, inputtag[i], &tagpos);  // Recursively copy all immediately adjacent tags.
      }

      // Copy text from text.srt file; don't include line-feed.
      for (textpos=0; textpos < strnlen (inputtext[i], MAXLEN); textpos++) {
        if (inputtext[i][textpos] == '\n') break;
        fputc (inputtext[i][textpos], fo);
      }

      // Look for closing tag or end of formatted line.
      while (tagpos < strnlen (inputtag[i], MAXLEN)) {

        // Found tag(s); copy them to output file.
        if (inputtag[i][tagpos] == '<') {
          copytags (fo, inputtag[i], &tagpos);  // Recursively copy all immediately adjacent tags.
        } else {
          tagpos++;
        }

      }  // Next char in formatted line

      // Next line
      i++;
      fprintf (fo, "\n");

    }  // Next line of text of current subtitle

    // End of sub line-feed
    fprintf (fo, "\n");
    i++;  // Next line

  }  // Next sub

  // Close output file.
  fclose (fo);

  // Free allocated memory.
  free (temp);
  free (bom);
  free (ntext);
  free (tagfilename);
  free (textfilename);
  for (line=0; line<alllinestag; line++) {
    free (inputtag[line]);
  }
  free (inputtag);
  for (line=0; line<alllinestext; line++) {
    free (inputtext[line]);
  }
  free (inputtext);

  return (EXIT_SUCCESS);
}

// Recursively copy all adjacent tags starting at position pos to output file.
void
copytags (FILE *fo, char *string, int *pos) {

  while ((*pos) < strnlen (string, MAXLEN)) {

    fputc (string[*pos], fo);
    (*pos)++;
    
    if (string[(*pos) - 1] == '>') break;
  }

  if (string[*pos] == '<') copytags (fo, string, pos);
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

    // Found a newline. Change to 0 for string termination.
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
