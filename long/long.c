// long.c - Read a SubRip (.srt) file and combine any two-line subtitle to a single line in order to have complete
//          sentences on a line. Really only designed for characters encountered in English and French.
//          If a Byte Order Mark (BOM) exists in the SubRip file containing the desired text, it will be included in the output file.

// gcc -Wall long.c -o long

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
int is_french (char *);
int readline (FILE*, char*, int);
int byteordermark (char *, BOM *);
int *allocate_intmem (int);
char *allocate_strmem (int);
char **allocate_strmemp (int);
BOM *allocate_bommem (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, j, type, alllines, nlines, line, nsubs, sub, *ntext, len, val;
  char *temp, *filename, **input;
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
  filename = allocate_strmem (MAXLEN);

  // Process the command line arguments, if any.
  if (argc == 2) {
    strncpy (filename, argv[1], MAXLEN);

  } else {
    fprintf (stdout, "\nUsage: ./long inputfilename.srt\n");
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

  // Open existing srt file.
  fi = fopen (filename, "r");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input srt file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Count lines of input SubRip file.
  alllines = 0;  // Count of lines
  while (readline (fi, temp, MAXLEN) != -1) {
    alllines++;
  }
  fprintf (stdout, "\n%s: %i lines found including any excess trailing line-feeds.\n", filename, alllines);
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
  fprintf (stdout, "%s: %i lines found excluding trailing line-feeds.\n", filename, nlines);

  // Detect any Byte Order Mark (BOM) at beginning of first line of SubRip file with desired text.
  type = byteordermark (input[0], bom);
  if (type < 0) {
    fprintf (stdout, "%s: No known Byte Order Mark (BOM) found.\n", filename);
  } else {
    fprintf (stdout, "%s: Byte Order Mark (BOM) detected for character encoding type: %s\n", filename, bom[type].name);
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
  fprintf (stdout, "\n%i subtitles found in %s.\n\n", nsubs, filename);

  // Allocate memory for various arrays.
  ntext = allocate_intmem (nsubs);  // Number of lines of text per subtitle

  // Count number of lines of text for each subtitle by examining the srt file.
  line = 0;  // Line index of formatted srt file
  for (sub = 0; sub < nsubs; sub++) {

    // Skip sub number and timestamp.
    line += 2;

    // Count number of lines of text for current subtitle.
    ntext[sub] = 0;
    while (input[line][0] != '\n') {
      ntext[sub]++;
      line++;
      if (line == nlines) break;
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

  i = 0;  // Line index of SubRip file.
  // Loop through all subs.
  for (sub = 0; sub < nsubs; sub++) {

    // Write sub number to output file.
    fprintf (fo, "%i\n", sub + 1);
    i++;  // Next line

    // Copy timestamp to output file.
    fprintf (fo, "%s", input[i]);
    i++;  // Next line

    // Loop through all lines of text for current subtitle.
    for (line = 0; line < ntext[sub]; line++) {

      len = strnlen (input[i], MAXLEN);

      // Regular un-accented characters.
      // WARNING: This does not concatenate lines when the 1st ends with "é", since it's multi-byte.
      val = input[i][len - 2];
      if (((line == 0) && (ntext[sub] > 1)) && ((val == ',') || (val == '>') ||
         ((val > 34) && (val < 44)) ||       // #, $, %, &, ', (, ), *, +
         ((val > 47) && (val < 60)) ||       // 0 - 9, :, ;
         ((val > 64) && (val < 92)) ||       // A - Z, [
         ((val > 96) && (val < 124)) ||      // a - z, {
         (val == ']'))) {

        // Replace line-feed with space if we need to concatenate next line.
        for (j = 0; j < (len - 1); j++) {
          fputc (input[i][j], fo);
        }
        fputc (' ', fo);

      // Accented characters (each is multi-byte) from French.
      } else if (((line == 0) && (ntext[sub] > 1)) && (is_french (input[i]))) {

        for (j = 0; j < (len - 1); j++) {
          fputc (input[i][j], fo);
        }
        fputc (' ', fo);

      // Bogus ellipsis case; fix it.
      } else if ((line == 0) && (strncmp (input[i], "---\n", MAXLEN) == 0)) {
        fprintf (fo, "...\n");

      // Special case: concatenate lines but leave no space.
      } else if ((line == 0) && (val == '-')) {

        for (j = 0; j < (len - 2); j++) {
          fputc (input[i][j], fo);
        }

      } else {
        fprintf (fo, "%s", input[i]);
      }

      i++;  // Next line

    } // Next line of current subtitle

    // End of sub line-feed
    fprintf (fo, "\n");
    i++;

  }  // Next sub

  // Close output file.
  fclose (fo);

  // Free allocated memory.
  free (temp);
  free (bom);
  free (ntext);
  free (filename);
  for (line = 0; line < alllines; line++) {
    free (input[line]);
  }
  free (input);

  return (EXIT_SUCCESS);
}

// Check for French character: à, é, è, ù, â, ê, î, ô, û, ë, ï, ü, ç followed by \n
// Return 0 if no match, 1 if a match.
int
is_french (char *line) {

  size_t i;
  char character[13][4] = {"à", "é", "è", "ù", "â", "ê", "î", "ô", "û", "ë", "ï", "ü", "ç"};
  char *p, temp[256];

  for (i = 0; i < 13; i++) {
    p = NULL;
    memset (temp, 0, 256 * sizeof (char));
    sprintf (temp, "%s\n", character[i]);
    p = strcasestr (line, temp);
    if (p != NULL) return (1);
  }  // Next accented char in out list

  return (0);  // No match from our list of accented chars.
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
