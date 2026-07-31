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

// reorder.c - Re-order non-chronological subtitles in a SubRip (srt) by sorting on start times.
// gcc -Wall reorder.c -o reorder

// Usage: ./reorder filename.srt
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

typedef struct {
  int h;
  int m;
  int s; 
  int ms;
  int64_t totalms;
} TIME;

// Function prototypes
int readline (FILE *, char *, int);
int byteordermark (char *, BOM *);
int extract_time (char*, TIME *, TIME *);
int parsetimestamp (char *, TIME *);
int timetoms (TIME *);
int heapsort (int, int64_t *, int *);
int heapify (int64_t *, int *, int, int);
int swap_int (int *, int *);
int swap_int64 (int64_t *, int64_t *);
char *allocate_strmem (int);
char **allocate_strmemp (int);
char ***allocate_strmempp (int);
int *allocate_intmem (int);
int64_t *allocate_int64mem (int);
TIME *allocate_timemem (int);
BOM *allocate_bommem (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define MAXLINES 10  // Maximum number of lines of text per subtitle
#define MAXBOM 11  // Maximum number of Byte Order Mark (BOM) types

int
main (int argc, char **argv) {

  int i, type, alllines, nlines, line, nsubs, sub;
  int *ntext, *aux;
  int64_t *startms;
  char *filename, **input, *temp, ***text;
  BOM *bom;
  TIME *start, *end;
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
    fprintf (stdout, "\nUsage: ./reorder inputfilename.srt\n");
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
  input = allocate_strmemp (alllines);
  for (line=0; line<alllines; line++) {
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

  // Allocate memory for various arrays.
  start = allocate_timemem (nsubs);
  end = allocate_timemem (nsubs);
  text = allocate_strmempp (nsubs);  // Array to hold text of subtitles
  for (sub=0; sub<nsubs; sub++) {
    text[sub] = allocate_strmemp (MAXLINES);
    for (i=0; i<MAXLINES; i++) {
      text[sub][i] = allocate_strmem (MAXLEN);
    }
  }
  startms = allocate_int64mem (nsubs);
  ntext = allocate_intmem (nsubs);  // Number of lines of text per subtitle
  aux = allocate_intmem (nsubs);  // Array to hold sorted sub numbers.

  // Loop through all lines in input file.
  sub = 0;  // Subtitle index
  for (line=0; line<nlines; line++) {

    // Skip line with sub number.
    line++;

    // Check format and extract start and end times for current subtitle.
    extract_time (input[line], &start[sub], &end[sub]);
    line++;

//fprintf (stderr, "Sub: %i  %02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", sub + 1, start[sub].h, start[sub].m, start[sub].s, start[sub].ms, end[sub].h, end[sub].m, end[sub].s, end[sub].ms);

    // Populate array of starting milliseconds for sorting.
    startms[sub] = start[sub].totalms;

    // Count lines of text of current subtitle.
    ntext[sub] = 0;
    while (line < nlines) {

      // Copy line of text from current subtitle.
      if (input[line][0] != '\n') {
        strncpy (text[sub][ntext[sub]], input[line], MAXLEN);
        ntext[sub]++;
      } else {
        break;
      }

      line++;  // Next line of input file
    }

    sub++;

  }  // Next subtitle

  // Create auxiliary matrix of sub numbers.
  // These will be consequently sorted as start times are sorted into ascending order.
  for (sub=0; sub<nsubs; sub++) {
    aux[sub] = sub;
  }

  // Heap sort in-place the array of start times into ascending order consequently sorting auxiliary array of subtitle numbers.
  heapsort (nsubs, startms, aux);

  // Open output file.
  fo = fopen ("out.srt", "r");
  if (fo != NULL) {
    fprintf (stderr, "ERROR: Output file out.srt already exists.\n");
    exit (EXIT_FAILURE);
  }
  fo = fopen ("out.srt", "w");
  if (fo == NULL) {
    fprintf (stderr, "ERROR: Unable to open output file.\n");
    exit (EXIT_FAILURE);
  }

  // Write Byte Order Mark (BOM) to output file if detected in input file.
  if (type != -1) {
    fwrite (bom[type].sequence, bom[type].len * sizeof (uint8_t), 1, fo);
  }

  // Loop through all subtitles.
  for (sub=0; sub<nsubs; sub++) {

    fprintf (fo, "%i\n", sub + 1);
    fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start[aux[sub]].h, start[aux[sub]].m, start[aux[sub]].s, start[aux[sub]].ms, end[aux[sub]].h, end[aux[sub]].m, end[aux[sub]].s, end[aux[sub]].ms);

    for (i=0; i<ntext[sub]; i++) {
      fprintf (fo, "%s", text[aux[sub]][i]);
    }

    fprintf (fo, "\n");

  }  // Next subtitle

  // Close output file.
  fclose (fo);

  // Free allocated memory.
  free (temp);
  free (bom);
  free (filename);
  free (start);
  free (end);
  free (startms);
  for (sub=0; sub<nsubs; sub++) {
    for (i=0; i<MAXLINES; i++) {
      free (text[sub][i]);
    }
    free (text[sub]);
  }
  free (text);
  free (ntext);
  for (line=0; line<alllines; line++) {
    free (input[line]);
  }
  free (input);
  free (aux);

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

      // If there's no end of line at the end of the file, add a line-feed.
      if (i > 0) {
        line[i] = '\n';
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

// Extract and parse start and end timestamps.
// Perform some basic format checks.
int
extract_time (char *text, TIME *start, TIME *end) {

  int i, loc[9] = {0, 1, 3, 4, 6, 7, 9, 10, 11};
  char *temp;

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);

  // Proper format
  //           1         2         3         4         5         6         7         8
  // 012345678901234567890123456789012345678901234567890123456789012345678901234567890
  // 01:12:15,025 --> 01:12:17,645

  // Check for arrow.
  if (strncmp (&text[12], " --> ", 5) != 0) {
    fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
    fprintf (stderr, "       %s\n", text);
    exit (EXIT_FAILURE);
  }

  // Starting timestamp.
  memset (temp, 0, MAXLEN * sizeof (char));
  memcpy (temp, text, 12 * sizeof (char));

  // Check for fatal format errors.
  if ((temp[2] != ':') || (temp[5] != ':') || (temp[8] != ',')) {
    fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
    fprintf (stderr, "       %s\n", text);
    exit (EXIT_FAILURE);
  }
  for (i=0; i<9; i++) {
    if ((temp[loc[i]] < '0') || (temp[loc[i]] > '9')) {
      fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
      fprintf (stderr, "       %s\n", text);
      exit (EXIT_FAILURE);
    }
  }

  // Format appears ok, so parse timestamp.
  parsetimestamp (temp, start);

  // Ending timestamp.
  memset (temp, 0, MAXLEN * sizeof (char));
  memcpy (temp, &text[17], 12 * sizeof (char));

  // Check for fatal format errors.
  if ((temp[2] != ':') || (temp[5] != ':') || (temp[8] != ',')) {
    fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
    fprintf (stderr, "       %s\n", text);
    exit (EXIT_FAILURE);
  }
  for (i=0; i<9; i++) {
    if ((temp[loc[i]] < '0') || (temp[loc[i]] > '9')) {
      fprintf (stderr, "\nERROR: Timestamp is malformed.\n");
      fprintf (stderr, "       %s\n", text);
      exit (EXIT_FAILURE);
    }
  }

  // Format appears ok, so parse timestamp.
  parsetimestamp (temp, end);

  // Free allocated memory.
  free (temp);

  return (EXIT_SUCCESS);
}

// Parse timestamp into TIME struct, and also return total time in milliseconds.
int
parsetimestamp (char *timestamp, TIME *time) {

  char *xx, *xxx, *endptr;

  // Allocate memory for various arrays.
  xx = allocate_strmem (3);
  xxx = allocate_strmem (4);

  // Hours
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, timestamp, 2);
  errno = 0;
  time->h = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of hours: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE); 
  }

  // Minutes
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, &timestamp[3], 2);
  errno = 0;
  time->m = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of minutes: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Seconds
  memset (xx, 0, 3 * sizeof (char));
  strncpy (xx, &timestamp[6], 2);
  errno = 0;
  time->s = (int) strtol (xx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xx)) {
    fprintf (stderr, "ERROR: Cannot make integer of seconds: %s\n", xx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Milliseconds
  memset (xxx, 0, 4 * sizeof (char));
  strncpy (xxx, &timestamp[9], 3);
  errno = 0;
  time->ms = (int) strtol (xxx, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == xxx)) {
    fprintf (stderr, "ERROR: Cannot make integer of milliseconds: %s\n", xxx);
    fprintf (stderr, "       %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  // Total milliseconds.
  timetoms (time);

  // Free allocated memory.
  free (xx);
  free (xxx);

  return (EXIT_SUCCESS);
}

// Calculate totalms from h, m, s, ms in TIME struct.
int
timetoms (TIME *time) {

  time->totalms = time->h * 60 * 60 * 1000;
  time->totalms += time->m * 60 * 1000;
  time->totalms += time->s * 1000;
  time->totalms += time->ms;
    
  return (EXIT_SUCCESS);
}

// Heapsort (in-place)
int
heapsort (int n, int64_t *data, int *aux) {

  int i;

  // Build a max heap from the array.
  for (i=((n/2)-1); i>=0; i--) {
    heapify (data, aux, n, i);
  }

  // One by one, extract elements from the heap.
  for (i=(n-1); i>0; i--) {

    // Move current root to end.
    swap_int64 (&data[0], &data[i]);
    swap_int (&aux[0], &aux[i]);

    // Call heapify on the reduced heap.
    heapify (data, aux, i, 0);
  }

  return (EXIT_SUCCESS);
}

// Recursively heapify a subtree rooted at index i.
// n is the size of the heap.
int
heapify (int64_t *data, int *aux, int n, int i) {

    int largest, left, right;

    largest = i;  // Initialize largest as root.
    left = (2 * i) + 1;
    right = (2 * i) + 2;

    // If left child is larger than root...
    if ((left < n) && (data[left] > data[largest])) {
        largest = left;
    }

    // If right child is larger than largest so far...
    if ((right < n) && (data[right] > data[largest])) {
        largest = right;
    }

    // If largest is not root, swap and heapify the affected subtree.
    if (largest != i) {
        swap_int64 (&data[i], &data[largest]);
        swap_int (&aux[i], &aux[largest]);
        heapify (data, aux, n, largest);
    }

  return (EXIT_SUCCESS);
}

// Swap integer values between two variables.
int
swap_int (int *a, int *b) {

  int temp;

  temp = *a;

  (*a) = (*b);

  (*b) = temp;

  return (EXIT_SUCCESS);
}

// Swap 64-bit integer values between two variables.
int
swap_int64 (int64_t *a, int64_t *b) {

  int64_t temp;

  temp = *a;

  (*a) = (*b);

  (*b) = temp;

  return (EXIT_SUCCESS);
}

// Allocate memory for an array of doubles.
double *
allocate_doublemem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_doublemem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (double *) malloc (len * sizeof (double));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (double));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_doublemem().\n");
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

// Allocate memory for an array of pointers to arrays of pointers to arrays of chars.
char ***
allocate_strmempp (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmempp().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (char ***) malloc (len * sizeof (char *));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (char *));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmempp().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of int64_t.
int64_t *
allocate_int64mem (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_int64mem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (int64_t *) malloc (len * sizeof (int64_t));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (int64_t));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_int64mem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of TIME structs.
TIME * 
allocate_timemem (int len) {
    
  void *tmp;
  
  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_timemem().\n", len); 
    exit (EXIT_FAILURE);
  } 

  tmp = (TIME *) malloc (len * sizeof (TIME));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (TIME));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_timemem().\n");
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
