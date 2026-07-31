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

// Create an XML chapters file.
// Use ffmpeg -i filename.mkv to get total duration. Typically use the video duration (not container duration).

// NOTE: ffmpeg gives milliseconds in fraction of a second.
//       i.e., as hh:mm:ss.xxxxxx and NOT as milliseconds as found in a SubRip srt file (hh:mm:ss,ms).

// gcc -Wall chapters.c -o chapters

// Usage: ./chapters
// Input: Asks user for total time duration and number of desired chapters.
// Output: chapters.xml

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Function prototypes
int inputtext (char *);
double mstoclock (double, double *, double *, double *);
double clocktoms (double *, double, double, double);
char *allocate_strmem (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line

int
main (int argc, char **argv) {

  int i;
  double nchap, hh, mm, ss, msdur, deltams;
  double tmpd, ohh, omm, oss;
  char *temp, *hh_str, *mm_str, *ss_str, *endptr;
  FILE *fo;

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAXLEN);
  hh_str = allocate_strmem (3);
  mm_str = allocate_strmem (3);
  ss_str = allocate_strmem (3);

  // Ask for feature duration.
  fprintf (stdout, "What is feature duration (hh:mm:ss.sss)? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  sscanf (temp, "%[^:]:%[^:]:%s", hh_str, mm_str, ss_str);

  // Parse duration entered by user.

  // Hours
  errno = 0;
  hh = strtod (hh_str, &endptr);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == hh_str)) {
    fprintf (stderr, "ERROR: Cannot make type double of hours: %s\n", hh_str);
    exit (EXIT_FAILURE);
  }

  // Minutes
  errno = 0;
  mm = strtod (mm_str, &endptr);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == mm_str)) {
    fprintf (stderr, "ERROR: Cannot make type double of minutes: %s\n", mm_str);
    exit (EXIT_FAILURE);
  }

  // Seconds (with fractional portion).
  errno = 0;
  ss = strtod (ss_str, &endptr);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == ss_str)) {
    fprintf (stderr, "ERROR: Cannot make type double of seconds: %s\n", ss_str);
    exit (EXIT_FAILURE);
  }

  // Convert total duration to milliseconds.
  clocktoms (&msdur, hh, mm, ss);

  fprintf (stdout, "What is desired number of chapters? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  errno = 0;
  nchap = strtod (temp, &endptr);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
    fprintf (stderr, "ERROR: Cannot make type double of number of chapters: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "Duration: %02i:%02i:%06.3lf (%i ms)\n", (int) hh, (int) mm, ss, (int) msdur);

  // Calculate number of ms between chapters.
  // Note that time 00:00:00.000 is defined as Chapter 1 in the standard.
  deltams = msdur / nchap;
  mstoclock (deltams, &ohh, &omm, &oss);
  fprintf (stdout, "Delta: %02i:%02i:%06.3lf (%i ms)\n", (int) ohh, (int) omm, oss, (int) deltams);

  // Open output file.
  fo = fopen ("chapters.xml", "r");
  if (fo != NULL) {
    fprintf (stderr, "ERROR: Output file chapters.xml already exists.\n");
    exit (EXIT_FAILURE);
  }
  fo = fopen ("chapters.xml", "w");
  if (fo == NULL) {
    fprintf (stderr, "ERROR: Unable to open output file chapters.xml.\n");
    exit (EXIT_FAILURE);
  }

  // Write header to output file.
  fprintf (fo, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf (fo, "<!DOCTYPE Chapters SYSTEM \"matroskachapters.dtd\">\n\n");

  // Start chapters entries.
  fprintf (fo, "<Chapters>\n");
  fprintf (fo, "  <EditionEntry>\n");

  // Add chapters.
  hh = 0.0;
  mm = 0.0;
  ss = 0.0;
  tmpd = 0;
  for (i=0; i<(int) nchap; i++) {

    fprintf (fo, "    <ChapterAtom>\n");
    fprintf (fo, "      <ChapterTimeStart>%02i:%02i:%06.3lf</ChapterTimeStart>\n", (int) hh, (int) mm, ss);
    fprintf (fo, "      <ChapterDisplay>\n");
    fprintf (fo, "        <ChapterString>Chapter %i</ChapterString>\n", i + 1);
    fprintf (fo, "        <ChapterLanguage>eng</ChapterLanguage>\n");
    fprintf (fo, "      </ChapterDisplay>\n");
    fprintf (fo, "    </ChapterAtom>\n");

    tmpd += deltams;

    mstoclock (tmpd, &hh, &mm, &ss);

  }  // Next chapter

  // Write footer to output file.
  fprintf (fo, "  </EditionEntry>\n");
  fprintf (fo, "</Chapters>");

  // Close output file.
  fclose (fo);

  // Free allocated memory.
  free (temp);
  free (hh_str);
  free (mm_str);
  free (ss_str);

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

// Convert totalms to hh, mm, and ss.
double
mstoclock (double totalms, double *hh, double *mm, double *ss) {

  // Calculate hh.
  (*hh) = (double) (((int) totalms) / (60 * 60 * 1000));
  totalms -= (*hh) * 60.0 * 60.0 * 1000.0;

  // Calculate mm.
  (*mm) = (double) (((int) totalms) / (60 * 1000));
  totalms -= (*mm) * 60.0 * 1000.0;

  // Calculate ss.
  (*ss) = totalms / 1000.0;

  return (EXIT_SUCCESS);
}

// Convert hh, mm, and ss to totalms.
double
clocktoms (double *totalms, double hh, double mm, double ss) {

  (*totalms) = hh * 60.0 * 60.0 * 1000.0;
  (*totalms) += mm * 60.0 * 1000.0;
  (*totalms) += ss * 1000.0;

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
