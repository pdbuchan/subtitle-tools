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

// Convert 8-bit sRGB to YCbCr (BT.709).
// Assumes sRGB gamma-correction was applied to sRGB; sRGB uses BT.709 color primaries. Applies BT.709 gamma-correction to produce YCbCr.

// gcc -Wall rgb2ycbcr.c -lm -o rgb2ycbcr

// Usage: ./rgb2ycbcr

// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4), IEC 61966-2-1:1999

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>  // for pow()
#include <errno.h>

// Function prototypes
int inputtext (char *);
int RGB2YCbCr (int, int, int, int *);
int *allocate_intmem (int);
char *allocate_strmem (int);
double *allocate_doublemem (int);
double **allocate_doublememp (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line

int
main (int argc, char **argv) {

  int r, g, b, *ycbcr;
  char *temp, *endptr;

  // Allocate memory for various arrays.
  ycbcr = allocate_intmem (3);
  temp = allocate_strmem (MAXLEN);

  fprintf (stdout, "\nRed value (0-255)? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  errno = 0;
  r = (int) strtol (temp, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
    fprintf (stderr, "ERROR: Cannot make integer of red value: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "Green value (0-255)? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  errno = 0;
  g = (int) strtol (temp, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
    fprintf (stderr, "ERROR: Cannot make integer of green value: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "Blue value (0-255)? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  errno = 0;
  b = (int) strtol (temp, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
    fprintf (stderr, "ERROR: Cannot make integer of blue value: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  // Convert RGB to (y, Cb, Cr).
  RGB2YCbCr (r, g, b, ycbcr);

  fprintf (stdout, "\nRGB (%i, %i, %i) = YCbCr (%i, %i, %i)\n", r, g, b, ycbcr[0], ycbcr[1], ycbcr[2]);
  fprintf (stdout, "RGB (0x%02x, 0x%02x, 0x%02x) = YCbCr (0x%02x, 0x%02x, 0x%02x)\n", r, g, b, ycbcr[0], ycbcr[1], ycbcr[2]);

  // Free allocated memory.
  free (ycbcr);
  free (temp);

  return (EXIT_SUCCESS);
}

// Convert 8-bit sRGB color to BT.709 YCbCr.
// Assumes sRGB gamma-correction was applied to sRGB; sRGB uses BT.709 color primaries. Applies BT.709 gamma-correction to produce YCbCr.
// High definition video use BT.709 colorspace (standard definition video uses BT.601 colorspace).
// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4), IEC 61966-2-1:1999
int
RGB2YCbCr (int r, int g, int b, int *yCbCr) {

  int i;
  double *rgb1, y1, pb, pr, y, cb, cr, **cm, yscale, pbscale, prscale;
  const double KB = 0.0721923154;
  const double KR = 0.2126390059;
  const double KG = 0.7151686788;
  const double Yoffset = 16.0;
  const double Cboffset = 128.0;
  const double Croffset = 128.0;

  // Variable Definitions
  //   yCbCr[3] array contains the Luma, Color Difference Blue (Chroma Blue), and Color Difference Red (Chroma Red), where
  //   16 <= Y <= 235, 16 <= Cb <= 240, 16 <= Cr <= 240.
  //   y1, pb, and pr are the normalized YCbCr signals (i.e., prior to scaling and offsets), where
  //   y1 (i.e., Y-prime) has range 0 to 1, and Pb and Pr have range -0.5 to 0.5.
  //   r1, g1, and b1 (i.e., r-prime, g-prime, and b-prime) are the normalized values of r, g, and b, each with range 0 to 1.

  // Allocate memory for various arrays.
  cm = allocate_doublememp (3);
  for (i=0; i<3; i++) {
    cm[i] = allocate_doublemem (3);
  }
  rgb1 = allocate_doublemem (3);

  // Normalize r,g,b (0-255) to r1,g1,b1 (0-1).
  rgb1[0] = (double) r / 255.0;
  rgb1[1] = (double) g / 255.0;
  rgb1[2] = (double) b / 255.0;

  // Reverse sRGB gamma-correction. i.e., convert to linear rgb.
  for (i=0; i<3; i++) {
    if (rgb1[i] > (12.92 * 0.0031308)) {
      rgb1[i] = pow ((rgb1[i] + 0.055) / 1.055, 2.4);
    } else {
      rgb1[i] /= 12.92;
    }
  }

  // Apply BT.709 gamma-correction to linear rgb.
  for (i=0; i<3; i++) {
    if (rgb1[i] < 0.018) {
      rgb1[i]*= 4.5;
    } else {
      rgb1[i] = (1.099 * pow (rgb1[i], 0.45)) - 0.099;
    }
  }

  // Define the color matrix.
  // cm[row][col]
  cm[0][0] = KR;                          cm[0][1] = KG;                          cm[0][2] = KB;
  cm[1][0] = -0.5 * KR / (1.0 - KB);      cm[1][1] = -0.5 * KG / (1.0 - KB);      cm[1][2] = 0.5;
  cm[2][0] = 0.5;                         cm[2][1] = -0.5 * KG / (1.0 - KR);      cm[2][2] = -0.5 * KB / (1.0 - KR);

  // Set scaling factors to achieve required ranges.
  yscale = 235.0 - 16.0;   // 16 <= Y <= 235, where 16 = black, 235 = white
  pbscale = 240.0 - 16.0;  // 16 <= Cb <= 240
  prscale = 240.0 - 16.0;  // 16 <= Cr <= 240

  // Multiply color matrix by r1,g1,b1 vector to obtain Y1PbPr.
  y1 = (cm[0][0] * rgb1[0]) + (cm[0][1] * rgb1[1]) + (cm[0][2] * rgb1[2]);
  pb = (cm[1][0] * rgb1[0]) + (cm[1][1] * rgb1[1]) + (cm[1][2] * rgb1[2]);
  pr = (cm[2][0] * rgb1[0]) + (cm[2][1] * rgb1[1]) + (cm[2][2] * rgb1[2]);

  // Apply scaling and offsets to obtain YCbCr.
  y = (y1 * yscale) + Yoffset;
  cb = (pb * pbscale) + Cboffset;
  cr = (pr * prscale) + Croffset;

  // Round and take integer.
  yCbCr[0] = (int) (y + 0.5);
  yCbCr[1] = (int) (cb + 0.5);
  yCbCr[2] = (int) (cr + 0.5);

  // Clip any undershoot or overshoot resulting from the fact that
  // 8-bit RGB (0-255) has a somewhat different color gamut than YCbCr.

  // Luminance bounds check
  // 16 <= Y <= 235
  if (yCbCr[0] < 16) yCbCr[0] = 16;
  if (yCbCr[0] > 235) yCbCr[0] = 235;

  // Color difference blue bounds check
  // 16 <= Cb <= 240
  if (yCbCr[1] < 16) yCbCr[1] = 16;
  if (yCbCr[1] > 240) yCbCr[1] = 240;

  // Color difference red bounds check
  // 16 <= Cr <= 240
  if (yCbCr[2] < 16) yCbCr[2] = 16;
  if (yCbCr[2] > 240) yCbCr[2] = 240;

  // Free allocated memory.
  for (i=0; i<3; i++) {
    free (cm[i]);
  }
  free (cm);
  free (rgb1);

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

// Allocate memory for an array of pointers to arrays of doubles.
double **
allocate_doublememp (int len) {

  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_doublememp().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (double **) malloc (len * sizeof (double *));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (double *));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_doublememp().\n");
    exit (EXIT_FAILURE);
  }
}
