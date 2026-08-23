/*  Copyright (C) 2025 P. David Buchan (pdbuchan@gmail.com)

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

// enc.c - Detect character encoding of a text file, convert to UTF-8, and add a Byte Order Mark (BOM) if requested.

// gcc -Wall enc.c -o enc

// Run without command line arguments to see usage notes.
// Output: out.txt

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>  // uint8_t
#include <string.h>
#include <string.h>
#include <errno.h>
#include "compact_enc_det.h"

// Definition of structs
typedef struct {
  int len;
  char *name;
  uint8_t *sequence;
} BOM;

// Function prototypes
int inputtext (char *);
int byteordermark (uint8_t *, BOM *);
const char *enc_name (int);
char *allocate_strmem (int);
uint8_t *allocate_ustrmem (int);
BOM *allocate_bommem (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXLINES 10  // Maximum number of lines of text per subtitle
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, type, choice, nbytes, bytes_consumed;
  char *filename, *temp, *temp2, *text;
  const char *encname;
  bool is_reliable;
  BOM *bom;
  Encoding encoding;
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
  temp = allocate_strmem (MAXLEN);
  temp2 = allocate_strmem (MAXLEN);
  bom = allocate_bommem (MAXBOM);
  filename = allocate_strmem (MAXLEN);

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

  // Process the command line arguments, if any.
  if (argc == 2) {
    strncpy (filename, argv[1], MAXLEN);
  
  } else {
    fprintf (stdout, "\nUsage: ./enc inputfilename\n");
    fprintf (stdout, "       Output will be out.txt.\n\n");
    return (EXIT_SUCCESS);
  }

  fprintf (stdout, "\nInput file: %s\n\n", filename);

  // Open input file.
  fi = fopen (filename, "rb");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open input file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  // Count bytes in file.
  nbytes = 0;
  while (fgetc (fi) != EOF) {
    nbytes++;
  }
  rewind (fi);
  fprintf (stdout, "%i bytes in input file.\n", nbytes);

  // Stop if less than 4 bytes in file.
  if (nbytes < 4) {
    fprintf (stderr, "ERROR: There are less than 4 bytes in input file.\n");
    fprintf (stderr, "       No action taken.\n\n");
    fclose (fi);
    free (temp);
    free (temp2);
    free (bom);
    free (filename);
    return (EXIT_FAILURE);
  }

  // Allocate memory for various arrays.
  text = allocate_strmem (nbytes);

  // Read file contents.
  for (i=0; i<nbytes; i++) {
    text[i] = fgetc (fi);
  }
  if (i != nbytes) {
    fprintf (stderr, "ERROR: Unable to read input file %s\n", filename);
    fprintf (stderr, "       %i bytes read.\n", i);
    exit (EXIT_FAILURE);
  }

  // Close input file.
  fclose (fi);

  // Detect any Byte Order Mark (BOM) at beginning of first line.
  type = byteordermark ((uint8_t *) text, bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known existing Byte Order Mark (BOM) found in %s.\n", filename);
  } else {
    fprintf (stdout, "\nExisting Byte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);
    fprintf (stdout, "No action taken.\n\n");
    return (EXIT_SUCCESS);
  }

  // Ask if Byte Order Mark (BOM) should be prepended to output file.
  fprintf (stdout, "\nDo you want a UTF-8 Byte Order Mark (BOM) prepended to output file out.txt (y/n)? ");
  choice = 0;  // Default to no.
  memset (temp, 0, MAXLEN * sizeof (char));
  do {
    inputtext (temp);
  } while (((temp[0] != 'y') && (temp[0] != 'Y') && (temp[0] != 'n') && (temp[0] != 'N')) || (strnlen (temp, MAXLEN) > 1));
  if ((temp[0] == 'y') || (temp[0] == 'Y')) choice = 1;

  // Detect probable current encoding of input file.
  encoding = CompactEncDet::DetectEncoding (text, nbytes, nullptr, nullptr, nullptr,
    UNKNOWN_ENCODING, UNKNOWN_LANGUAGE, CompactEncDet::QUERY_CORPUS, true,
    &bytes_consumed, &is_reliable);

  // Obtain name of probable current encoding.
  encname = enc_name (encoding);

  // Present CED's results.
  fprintf (stdout, "\nCED:\nProbable encoding: %s\n", encname);
  fprintf (stdout, "bytes consumed: %i\n", bytes_consumed);
  fprintf (stdout, "Is reliable?: %s\n", is_reliable ? "true" : "false");

  // Obtain and present chardet's results.
  fprintf (stdout, "\nchardet:\n");
  memset (temp, 0, MAXLEN * sizeof (char));
  sprintf (temp, "chardet %s", filename);
  system (temp);
  sleep (1);  // Allow 1 second for chardet() to work and present results.

  // Ask for user to select their choice for most probable encoding.
  fprintf (stdout, "\nChoose most likely encoding: ");
  memset (temp2, 0, MAXLEN * sizeof (char));
  inputtext (temp2);
  fprintf (stdout, "\n");

  // Use iconv to convert from probably current encoding to UTF-8 encoding.
  memset (temp, 0, MAXLEN * sizeof (char));
  sprintf (temp, "iconv -f %s -t UTF-8 %s -o out.tmp", temp2, filename);
  system (temp);
  sleep (1);  // Allow 1 second for iconv() to work and create out.tmp.

  // Open temporary file.
  fi = fopen ("out.tmp", "rb");
  if (fi == NULL) {
    fprintf (stderr, "\nERROR: Unable to open temporary file out.tmp.\n");
    exit (EXIT_FAILURE);
  }

  // Count bytes in file.
  nbytes = 0;
  while (fgetc (fi) != EOF) {
    nbytes++;
  }
  rewind (fi);

  // Allocate memory for various arrays.
  free (text);
  text = allocate_strmem (nbytes);

  // Read file contents.
  for (i=0; i<nbytes; i++) {
    text[i] = fgetc (fi);
  }
  fprintf (stdout, "%i bytes read from temporary file out.tmp\n", i);

  // Close input file.
  fclose (fi);

  // Remove temporary file.
  system ("rm -f out.tmp");

  // Open output file.
  fo = fopen ("out.txt", "r");
  if (fo != NULL) {
    fprintf (stderr, "Output file out.txt already exists.\n");
    exit (EXIT_FAILURE);
  }
  fo = fopen ("out.txt", "w");
  if (fo == NULL) {
    fprintf (stderr, "Can't open output file out.txt.\n");
    exit (EXIT_FAILURE);
  }

  // Write UTF-8 BOM to output file, if requested.
  if (choice > 0) {
    for (i=0; i<bom[0].len; i++) {
      fputc (bom[0].sequence[i], fo);
    }
    fprintf (stdout, "%s Byte Order Mark (BOM) prepended to output file out.txt.\n\n", name[0]);
  } else {
    fprintf (stdout, "No Byte Order Mark (BOM) prepended to output file out.txt.\n\n");
  }

  // Append input file.
  for (i=0; i<nbytes; i++) {
    fputc (text[i], fo);
  }

  // Close output file.
  fclose (fo);

  // Free allocated memory.
  free (temp);
  free (temp2);
  free (text);
  free (bom);
  free (filename);

  return (EXIT_SUCCESS);
}

// Obtain a text string from standard input. It can include spaces.
int
inputtext (char *text) {

  // Request new text from standard input.
  fgets (text, MAXLEN, stdin);

  // Remove trailing newline, if there.
  if ((strnlen(text, MAXLEN) > 0) && (text[strnlen (text, MAXLEN) - 1] == '\n')) {
    text[strnlen (text, MAXLEN) - 1] = '\0';  // Replace newline with string termination.
  }

  return (EXIT_SUCCESS);
}

// Detect Byte Order Mark (BOM), if it exists, at beginning of line.
// Return index of bom array corresponding to type of BOM detected,
// or return -1 if none (or unlisted type) detected.
int
byteordermark (uint8_t *text, BOM *bom) {

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
    return ((char *) tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of unsigned chars.
uint8_t *
allocate_ustrmem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_ustrmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (uint8_t *) malloc (len * sizeof (uint8_t));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (uint8_t));
    return ((uint8_t *) tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_ustrmem().\n");
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
    return ((BOM *) tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_bommem().\n");
    exit (EXIT_FAILURE);
  }
}
