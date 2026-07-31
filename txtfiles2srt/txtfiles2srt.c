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

// txtfiles2srt.c - Take the timestamps from the filenames in a collection of individual subtitle text files, each containing the text for a subtitle,
//                  and produce a SubRip (.srt) file. No Byte Order Mark (BOM) is prepended. Each text file should contain only lines of text
//                  without blank lines or trailing line-feeds.

// gcc -Wall txtfiles2srt.c -o txtfiles2srt

// Usage: ./txtfiles2srt filelistfilename
// Input: filelistfilename is a text file containing only a list of the subtitle text files. Each filename is expected to be:
//        hh_mm_ss_ms__hh_mm_ss_ms.txt. For example: 00_13_11_959__00_13_15_213.txt
// Output: out.srt

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>  // uint8_t
#include <string.h>
#include <errno.h>

// Definition of structs
typedef struct {
  int h; 
  int m;
  int s;
  int ms; 
} TIME;

// Function prototypes
int readline (FILE *, char *, int);
int extract_time (char*, TIME *, TIME *);
int parsetimestamp (char *, TIME *);
char *allocate_strmem (int);
char **allocate_strmemp (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line

int
main (int argc, char **argv) {

  int i, alllines, line, nsubs, sub;
  char *temp, *list_filename, **textfilenames;
  TIME start, end;
  FILE *fi_list, *fi, *fo;

  // Allocate memory for various arrays.
  list_filename = allocate_strmem (MAXLEN);

  // Process the command line arguments, if any.
  if (argc == 2) {
    strncpy (list_filename, argv[1], MAXLEN);

  } else {
    fprintf (stdout, "\nUsage: ./txtfiles2srt filelistfilename\n");
    fprintf (stdout, "       Output filename will be out.srt.\n\n");
    return (EXIT_SUCCESS);
  }

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);

  // Open input file containing list of text filenames.
  fi_list = fopen (list_filename, "r");
  if (fi_list == NULL) {
    fprintf (stderr, "ERROR: Unable to open input file %s.\n", list_filename);
    exit (EXIT_FAILURE);
  }

  // Count lines of input file.
  alllines = 0;  // Count of lines
  while (readline (fi_list, temp, MAXLEN) != -1) {
    alllines++;
  }
  fprintf (stdout, "\n%s: %i lines found including any excess trailing line-feeds.\n", list_filename, alllines);
  rewind (fi_list);

  // Allocate memory for array to hold input file.
  textfilenames = allocate_strmemp (alllines);
  for (line=0; line<alllines; line++) {
    textfilenames[line] = allocate_strmem (MAXLEN);
  }

  // Read input file into array textfilenames.
  for (line=0; line<alllines; line++) {
    if (readline (fi_list, textfilenames[line], MAXLEN) == -1) {
      fprintf (stderr, "\nERROR: Cannot read line %i from input file %s.\n", line + 1, list_filename);
      exit (EXIT_FAILURE);
    }
  }  // Next line

  // Close input file.
  fclose (fi_list);

  // Remove excess line-feeds at end of list of filenames.
  nsubs = alllines;
  for (line=alllines; line>1; line--) {
    if ((textfilenames[line - 1][0] == '\n') && (textfilenames[line - 2][0] == '\n')) {
      nsubs--;
    } else {
      break;
    }
  }
  fprintf (stdout, "%s: %i subtitles found (lines found excluding trailing line-feeds).\n", list_filename, nsubs);

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

  // Loop through the text filenames (subtitles).
  for (sub=0; sub<nsubs; sub++) {

    // Extract start and end times for this subtitle from the filename.
    extract_time (textfilenames[sub], &start, &end);

    // Open a text file representing one subtitle.
    fi = fopen (textfilenames[sub], "r");
    if (fi == NULL) {
      fprintf (stderr, "ERROR: Unable to open input file %s.\n", textfilenames[sub]);
      exit (EXIT_FAILURE);
    }

    // Write current subtitle number to new .srt file.
    fprintf (fo, "%i\n", sub + 1);

    // Write starting and ending timestamps for this subtitle.
    fprintf (fo, "%02i:%02i:%02i,%03i --> %02i:%02i:%02i,%03i\n", start.h, start.m, start.s, start.ms, end.h, end.m, end.s, end.ms);

    // Copy lines of subtitle text file to new SubRip (.srt) file.
    while (readline (fi, temp, MAXLEN) != -1) {
      fprintf (fo, "%s\n", temp);
    }

    // Close the current input text (subtitle) file.
    fclose (fi);

    // Add a blank line (line-feed) indicating end of current subtitle.
    fputc ('\n', fo);

  }  // Next subtitle

  fprintf (stdout, "\n");

  // Close output file.
  fclose (fo);

  // Free allocated memory.
  free (temp);
  free (list_filename);
  for (i=0; i<alllines; i++) {
    free (textfilenames[i]);
  }
  free (textfilenames);

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

    // Found a newline.
    // Terminate string with 0.
    // Break out of loop since this is the end of the current line.
    if (n == '\n') {
      line[i] = 0;
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

// Extract and parse start and end timestamps.
// Perform some basic format checks.
int
extract_time (char *text, TIME *start, TIME *end) {

  int i, loc[9] = {0, 1, 3, 4, 6, 7, 9, 10, 11};
  char *temp;

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);

  // Expected format
  //           1         2         3         4         5         6         7         8
  // 012345678901234567890123456789012345678901234567890123456789012345678901234567890
  // 01_12_15_025__01_12_17_645.txt

  // Starting timestamp.
  memset (temp, 0, MAXLEN * sizeof (char));
  memcpy (temp, text, 12 * sizeof (char));

  // Check for fatal format errors.
  if ((temp[2] != '_') || (temp[5] != '_') || (temp[8] != '_')) {
    fprintf (stderr, "ERROR1: Timestamp is malformed.\n");
    fprintf (stderr, "       %s\n", text);
    exit (EXIT_FAILURE);
  }
  for (i=0; i<9; i++) {
    if ((temp[loc[i]] < '0') || (temp[loc[i]] > '9')) {
      fprintf (stderr, "ERROR2: Timestamp is malformed.\n");
      fprintf (stderr, "       %s\n", text);
      exit (EXIT_FAILURE);
    }
  }

  // Format appears ok, so parse timestamp.
  parsetimestamp (temp, start);

  // Ending timestamp.
  if (strncmp (&text[12], "__", 2) != 0) {
    fprintf (stderr, "\nERROR3: Timestamp is malformed.\n");
    fprintf (stderr, "         %s\n", text);
    exit (EXIT_FAILURE);
  }
  memset (temp, 0, MAXLEN * sizeof (char));
  memcpy (temp, &text[14], 12 * sizeof (char));

  // Check for fatal format errors.
  if ((temp[2] != '_') || (temp[5] != '_') || (temp[8] != '_')) {
    fprintf (stderr, "ERROR4: Timestamp is malformed.\n");
    fprintf (stderr, "       %s\n", text);
    exit (EXIT_FAILURE);
  }
  for (i=0; i<9; i++) {
    if ((temp[loc[i]] < '0') || (temp[loc[i]] > '9')) {
      fprintf (stderr, "ERROR5: Timestamp is malformed.\n");
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

  // Free allocated memory.
  free (xx);
  free (xxx);

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
