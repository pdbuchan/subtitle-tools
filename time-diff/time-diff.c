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

// time-diff.c - Given two timestamps from standard input, calculate the time difference and output as a timestamp.

// gcc -Wall time-diff.c -o time-diff

// Run without command line arguments.
// Input: starting timestamp, ending timestamp
// Output: reports to stdout

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>  // uint8_t
#include <string.h>
#include <errno.h>

typedef struct {
  int h;
  int m;
  int s;
  int ms;
  int64_t totalms;
} TIME;

// Function prototypes
int inputtext (char *);
int parsetimestamp (char *, TIME *);
int timetoms (TIME *);
int mstotime (TIME *);
char *allocate_strmem (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line

int
main (int argc, char **argv) {

  char *timestamp;
  TIME start, end, diff;

  // Allocate memory for various arrays.
  timestamp = allocate_strmem (MAXLEN);

  // Ask for starting timestamp.
  fprintf (stdout, "\nStarting timestamp (hh:mm:ss.xxx or hh:mm:ss,xxx)? ");
  memset (timestamp, 0, MAXLEN * sizeof (char));
  inputtext (timestamp);
  parsetimestamp (timestamp, &start);

  // Ask for ending timestamp.
  fprintf (stdout, "Ending timestamp (hh:mm:ss.xxx or hh:mm:ss,xxx)? ");
  memset (timestamp, 0, MAXLEN * sizeof (char));
  inputtext (timestamp);
  parsetimestamp (timestamp, &end);

  // Calcalate difference and express as a timestamp.
  diff.totalms = end.totalms - start.totalms;
  mstotime (&diff);

  // Show difference on standard output.
  fprintf (stdout, "Difference is (hh:mm:ss.ms): %02i:%02i:%02i.%03i\n\n", diff.h, diff.m, diff.s, diff.ms);

  // Free allocated memory.
  free (timestamp);

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

// Calculate h, m, s, ms from totalms in TIME struct.
int
mstotime (TIME *time) {

  int64_t totalms;

  totalms = time->totalms;

  time->h = (int) (totalms / (60 * 60 * 1000));
  totalms -= (time->h * 60 * 60 * 1000);

  time->m = (int) (totalms / (60 * 1000));
  totalms -= (time->m * 60 * 1000);

  time->s = (int) (totalms / 1000);
  totalms -= (time->s * 1000);

  time->ms = totalms;

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
