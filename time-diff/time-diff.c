/*  Copyright (C) 2025-2026 P. David Buchan (pdbuchan@gmail.com)
  
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

// gcc -std=c11 -Wall -Wextra -Wpedantic time-diff.c -o time-diff

// Run without command line arguments.
// Input: starting timestamp, ending timestamp
// Output: reports to stdout

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

typedef struct {
  int h;
  int m;
  int s;
  int ms;
  int64_t totalms;
} TIME;

// Function prototypes
int inputtext (char *);
int parsetimestamp (const char *, TIME *);
int timetoms (TIME *);
int mstotime (TIME *);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line
#define TIMESTAMP_LEN 12  // Length of hh:mm:ss.xxx or hh:mm:ss,xxx

int
main (void) {

  char timestamp[MAXLEN];
  TIME start, end, diff;

  // Ask for starting timestamp.
  fprintf (stdout, "\nStarting timestamp (hh:mm:ss.xxx or hh:mm:ss,xxx)? ");
  inputtext (timestamp);
  parsetimestamp (timestamp, &start);

  // Ask for ending timestamp.
  fprintf (stdout, "Ending timestamp (hh:mm:ss.xxx or hh:mm:ss,xxx)? ");
  inputtext (timestamp);
  parsetimestamp (timestamp, &end);

  // Calculate difference and express as a timestamp.
  if (end.totalms < start.totalms) {
    fprintf (stderr, "ERROR: Ending timestamp must not be earlier than starting timestamp.\n");
    return (EXIT_FAILURE);
  }
  diff.totalms = end.totalms - start.totalms;
  mstotime (&diff);

  // Show difference on standard output.
  fprintf (stdout, "Difference is (hh:mm:ss.ms): %02i:%02i:%02i.%03i\n\n", diff.h, diff.m, diff.s, diff.ms);

  return (EXIT_SUCCESS);
}

// Obtain a text string from standard input. It can include spaces.
int
inputtext (char *text) {

  int ch;
  size_t len;

  if (text == NULL) {
    fprintf (stderr, "ERROR: Invalid text buffer in inputtext().\n");
    exit (EXIT_FAILURE);
  }

  if (fgets (text, MAXLEN, stdin) == NULL) {
    fprintf (stderr, "ERROR: Unable to read text from standard input.\n");
    exit (EXIT_FAILURE);
  }

  len = strlen (text);

  // Remove trailing newline, and a preceding carriage return if present.
  if ((len > 0u) && (text[len - 1u] == '\n')) {
    text[--len] = '\0';
    if ((len > 0u) && (text[len - 1u] == '\r')) {
      text[--len] = '\0';
    }
    return (EXIT_SUCCESS);
  }

  // If the buffer is full, determine whether the input was exactly
  // MAXLEN - 1 characters or was genuinely too long.
  if (len == (MAXLEN - 1u)) {

    ch = getchar ();

    // Exactly MAXLEN - 1 characters followed by newline or EOF.
    if ((ch == '\n') || (ch == EOF)) {
      return (EXIT_SUCCESS);
    }

    // Handle CRLF after an exactly full input line.
    if (ch == '\r') {
      ch = getchar ();
      if ((ch == '\n') || (ch == EOF)) {
        return (EXIT_SUCCESS);
      }
    }

    // Discard the remainder of an overlong input line.
    while ((ch != '\n') && (ch != EOF)) {
      ch = getchar ();
    }

    fprintf (stderr, "ERROR: Input text is too long; maximum is %d characters.\n", MAXLEN - 1);
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Parse timestamp into TIME struct, and also calculate total time in milliseconds.
// Accepted forms are exactly hh:mm:ss.xxx and hh:mm:ss,xxx.
int
parsetimestamp (const char *timestamp, TIME *time) {

  size_t i, len;
  static const size_t digitpos[] = {0u, 1u, 3u, 4u, 6u, 7u, 9u, 10u, 11u};

  if ((timestamp == NULL) || (time == NULL)) {
    fprintf (stderr, "ERROR: Invalid argument to parsetimestamp().\n");
    exit (EXIT_FAILURE);
  }

  len = strlen (timestamp);
  if (len != TIMESTAMP_LEN) {
    fprintf (stderr, "ERROR: Timestamp must have form hh:mm:ss.xxx or hh:mm:ss,xxx: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  if ((timestamp[2] != ':') || (timestamp[5] != ':') ||
      ((timestamp[8] != '.') && (timestamp[8] != ','))) {
    fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  for (i=0u; i<(sizeof (digitpos) / sizeof (digitpos[0])); i++) {
    if ((timestamp[digitpos[i]] < '0') || (timestamp[digitpos[i]] > '9')) {
      fprintf (stderr, "ERROR: Timestamp is malformed: %s\n", timestamp);
      exit (EXIT_FAILURE);
    }
  }

  time->h = ((timestamp[0] - '0') * 10) + (timestamp[1] - '0');
  time->m = ((timestamp[3] - '0') * 10) + (timestamp[4] - '0');
  time->s = ((timestamp[6] - '0') * 10) + (timestamp[7] - '0');
  time->ms = ((timestamp[9] - '0') * 100) + ((timestamp[10] - '0') * 10) + (timestamp[11] - '0');

  if (time->m > 59) {
    fprintf (stderr, "ERROR: Timestamp minutes must be in range 00-59: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }
  if (time->s > 59) {
    fprintf (stderr, "ERROR: Timestamp seconds must be in range 00-59: %s\n", timestamp);
    exit (EXIT_FAILURE);
  }

  timetoms (time);

  return (EXIT_SUCCESS);
}

// Calculate totalms from h, m, s, ms in TIME struct.
int
timetoms (TIME *time) {

  if (time == NULL) {
    fprintf (stderr, "ERROR: Invalid TIME pointer in timetoms().\n");
    exit (EXIT_FAILURE);
  }

  time->totalms = (int64_t) time->h * INT64_C(60) * INT64_C(60) * INT64_C(1000);
  time->totalms += (int64_t) time->m * INT64_C(60) * INT64_C(1000);
  time->totalms += (int64_t) time->s * INT64_C(1000);
  time->totalms += (int64_t) time->ms;

  return (EXIT_SUCCESS);
}

// Calculate h, m, s, ms from totalms in TIME struct.
int
mstotime (TIME *time) {

  int64_t hours, totalms;

  if (time == NULL) {
    fprintf (stderr, "ERROR: Invalid TIME pointer in mstotime().\n");
    exit (EXIT_FAILURE);
  }
  if (time->totalms < 0) {
    fprintf (stderr, "ERROR: Negative timestamp cannot be converted.\n");
    exit (EXIT_FAILURE);
  }

  totalms = time->totalms;

  hours = totalms / (INT64_C(60) * INT64_C(60) * INT64_C(1000));
  if (hours > INT_MAX) {
    fprintf (stderr, "ERROR: Timestamp hours exceed supported range.\n");
    exit (EXIT_FAILURE);
  }
  time->h = (int) hours;
  totalms -= hours * INT64_C(60) * INT64_C(60) * INT64_C(1000);

  time->m = (int) (totalms / (INT64_C(60) * INT64_C(1000)));
  totalms -= (int64_t) time->m * INT64_C(60) * INT64_C(1000);

  time->s = (int) (totalms / INT64_C(1000));
  totalms -= (int64_t) time->s * INT64_C(1000);

  time->ms = (int) totalms;

  return (EXIT_SUCCESS);
}
