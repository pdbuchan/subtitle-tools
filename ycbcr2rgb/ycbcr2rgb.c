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

// Convert YCbCr (BT.709) to 8-bit sRGB.
// Assumes BT.709 color primaries and gamma-correction was used to produce YCbCr; sRGB uses BT.709 color primaries and sRGB gamma-correction.

// gcc -Wall ycbcr2rgb.c -lm -o ycbcr2rgb

// Usage: ./ycbcr2rgb

// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4), IEC 61966-2-1:1999

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>  // for pow()
#include <errno.h>

// Function prototypes
int inputtext (char *);
int YCbCr2RGB (int, int, int, int *);
int *allocate_intmem (int);
char *allocate_strmem (int);
double *allocate_doublemem (int);
double **allocate_doublememp (int);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line

int
main (int argc, char **argv) {

  int y, cb, cr, *rgb;
  char *temp , *endptr;

  // Allocate memory for various arrays.
  rgb = allocate_intmem (3);
  temp = allocate_strmem (MAXLEN);

  fprintf (stdout, "\nLuminance (Y) (16-235)? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  errno = 0;
  y = (int) strtol (temp, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
    fprintf (stderr, "ERROR: Cannot make integer of luminance: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "Color difference blue (Cb) (16-240)? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  errno = 0;
  cb = (int) strtol (temp, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
    fprintf (stderr, "ERROR: Cannot make integer of color difference blue: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  fprintf (stdout, "Color difference red (Cr) (16-240)? ");
  memset (temp, 0, MAXLEN * sizeof (char));
  inputtext (temp);
  errno = 0;
  cr = (int) strtol (temp, &endptr, 10);
  if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
    fprintf (stderr, "ERROR: Cannot make integer of color difference red: %s\n", temp);
    exit (EXIT_FAILURE);
  }

  // Convert (y, Cb, Cr) to RGB.
  YCbCr2RGB (y, cb, cr, rgb);

  fprintf (stdout, "\nYCbCr (%i, %i, %i) = RGB (%i, %i, %i)\n", y, cb, cr, rgb[0], rgb[1], rgb[2]);
  fprintf (stdout, "YCbCr (0x%02x, 0x%02x, 0x%02x) = RGB (0x%02x, 0x%02x, 0x%02x)\n", y, cb, cr, rgb[0], rgb[1], rgb[2]);

  // Free allocated memory.
  free (rgb);
  free (temp);

  return (EXIT_SUCCESS);
}

// Convert YCbCr color to 8-bit sRGB.
// Assumes BT.709 color primaries and gamma-correction was used to produce YCbCr; sRGB uses BT.709 color primaries and sRGB gamma-correction.
// High definition video use BT.709 colorspace (standard definition video uses BT.601 colorspace). 
// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4), IEC 61966-2-1:1999
int
YCbCr2RGB (int luma, int chromab, int chromar, int *rgb) {

  int i;
  double r, g, b, *rgb1, y1, pb, pr, y, cb, cr, **cm, yscale, pbscale, prscale;
  const double KB = 0.0721923154;
  const double KR = 0.2126390059;
  const double KG = 0.7151686788;
  const double Yoffset = 16.0;
  const double Cboffset = 128.0;
  const double Croffset = 128.0;

  // Convert incoming YCbCr values to double.
  y = (double) luma;
  cb = (double) chromab;
  cr = (double) chromar;

  // Variable Definitions
  //   y, cb, and cr are the Luma, Color Difference Blue (Chroma Blue), and Color Difference Red (Chroma Red), where
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

  // Set scaling factors to achieve required ranges.
  yscale = 235.0 - 16.0;   // 16 <= Y <= 235, where 16 = black, 235 = white
  pbscale = 240.0 - 16.0;  // 16 <= Cb <= 240
  prscale = 240.0 - 16.0;  // 16 <= Cr <= 240

  // Normalize Y (16 to 235) to Y1 (0 to 1).
  y1 = (y - Yoffset) / yscale;

  // Normalize Cb (16 to 240) to Pb (-0.5 to 0.5).
  pb = (cb - Cboffset) / pbscale;

  // Normalize Cr (16 to 240) to Pr (-0.5 to 0.5).
  pr = (cr - Croffset) / prscale;

  // Define the color matrix.
  // cm[row][col]
  cm[0][0] = 1.0;    cm[0][1] = 0.0;                                cm[0][2] = 2.0 - (2.0 * KR);
  cm[1][0] = 1.0;    cm[1][1] = -(KB / KG) * (2.0 - (2.0 * KB));    cm[1][2] = -(KR / KG) * (2.0 - (2.0 * KR));
  cm[2][0] = 1.0;    cm[2][1] = 2.0 - (2.0 * KB);                   cm[2][2] = 0.0;

  // Multiply color matrix by y1,pb,pr vector to obtain r1,g1,b1.
  for (i=0; i<3; i++) {
    rgb1[i] = (cm[i][0] * y1) + (cm[i][1] * pb) + (cm[i][2] * pr);
  }

  // Reverse BT.709 camera gamma-correction. i.e., convert to linear rgb
  for (i=0; i<3; i++) {
    if (rgb1[i] < 0.081) {
      rgb1[i] /= 4.5;
    } else {
      rgb1[i] = pow ((rgb1[i] + 0.099) / 1.099, 1 / 0.45);
    }
  }

  // Apply sRGB gamma-correction to linear rgb.
  for (i=0; i<3; i++) {
    if (rgb1[i] > 0.0031308) {
      rgb1[i] = (1.055 * pow (rgb1[i], (1.0 / 2.4))) - 0.055;
    } else {
      rgb1[i] *= 12.92;
    }
  }
    
  // Apply scaling factors to obtain required range of 0 to 255.
  r = rgb1[0] * 255.0;
  g = rgb1[1] * 255.0;
  b = rgb1[2] * 255.0;

  // Convert to integer.
  rgb[0] = (int) (r + 0.5);
  rgb[1] = (int) (g + 0.5);
  rgb[2] = (int) (b + 0.5);

  // Clip any undershoot or overshoot resulting from the fact that
  // 8-bit RGB (0-255) has a somewhat different color gamut than YCbCr.
  for (i=0; i<3; i++) {
    if (rgb[i] < 0) rgb[i] = 0;
    if (rgb[i] > 255) rgb[i] = 255;
  }

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
