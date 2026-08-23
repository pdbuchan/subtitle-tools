/*  Copyright (C) 2024-2026 P. David Buchan (pdbuchan@gmail.com)

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

// Convert studio-range BT.709 Y'CbCr to 8-bit sRGB.
//
// The Y'CbCr matrix is inverted to obtain nonlinear BT.709 R'G'B'. The BT.709
// transfer function is then reversed to obtain linear RGB, after which the
// sRGB transfer function is applied to produce standard nonlinear sRGB values.

// gcc -Wall ycbcr2rgb.c -lm -o ycbcr2rgb

// Usage: ./ycbcr2rgb

// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4), IEC 61966-2-1:1999

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>  // pow(), lround()
#include <errno.h>
#include <ctype.h>

// Function prototypes
int inputtext (char *);
int parse_ycbcr_value (const char *, int, int, int *);
int YCbCr2RGB (int, int, int, int *);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line

int
main (void) {

  int y, cb, cr, rgb[3];
  char temp[MAXLEN];

  fprintf (stdout, "\nLuminance (Y) (16-235)? ");
  memset (temp, 0, sizeof (temp));
  if (inputtext (temp) == EXIT_FAILURE) {
    return (EXIT_FAILURE);
  }
  if (parse_ycbcr_value (temp, 16, 235, &y) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Luminance must be an integer from 16 through 235: %s\n", temp);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "Color difference blue (Cb) (16-240)? ");
  memset (temp, 0, sizeof (temp));
  if (inputtext (temp) == EXIT_FAILURE) {
    return (EXIT_FAILURE);
  }
  if (parse_ycbcr_value (temp, 16, 240, &cb) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Color difference blue must be an integer from 16 through 240: %s\n", temp);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "Color difference red (Cr) (16-240)? ");
  memset (temp, 0, sizeof (temp));
  if (inputtext (temp) == EXIT_FAILURE) {
    return (EXIT_FAILURE);
  }
  if (parse_ycbcr_value (temp, 16, 240, &cr) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Color difference red must be an integer from 16 through 240: %s\n", temp);
    return (EXIT_FAILURE);
  }

  // Convert Y'CbCr to RGB.
  if (YCbCr2RGB (y, cb, cr, rgb) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Unable to convert YCbCr to RGB.\n");
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "\nYCbCr (%i, %i, %i) = RGB (%i, %i, %i)\n", y, cb, cr, rgb[0], rgb[1], rgb[2]);
  fprintf (stdout, "YCbCr (0x%02x, 0x%02x, 0x%02x) = RGB (0x%02x, 0x%02x, 0x%02x)\n", y, cb, cr, rgb[0], rgb[1], rgb[2]);

  return (EXIT_SUCCESS);
}

// Parse one Y'CbCr component and require the entire non-whitespace input to be
// a decimal integer within the inclusive range minvalue through maxvalue.
int
parse_ycbcr_value (const char *text, int minvalue, int maxvalue, int *value) {

  char *endptr;
  long n;

  if ((text == NULL) || (value == NULL) || (minvalue > maxvalue)) {
    return (EXIT_FAILURE);
  }

  errno = 0;
  endptr = NULL;
  n = strtol (text, &endptr, 10);

  if ((errno == ERANGE) || (endptr == text)) {
    return (EXIT_FAILURE);
  }

  // Permit trailing whitespace, but no other trailing characters.
  while ((*endptr != '\0') && isspace ((unsigned char) *endptr)) {
    endptr++;
  }

  if ((*endptr != '\0') || (n < (long) minvalue) || (n > (long) maxvalue)) {
    return (EXIT_FAILURE);
  }

  *value = (int) n;

  return (EXIT_SUCCESS);
}

// Convert studio-range BT.709 Y'CbCr to 8-bit nonlinear sRGB.
//
// Input code ranges:
//   16 <= Y' <= 235
//   16 <= Cb <= 240
//   16 <= Cr <= 240
//
// BT.709 and sRGB use the same D65 white point and RGB primaries but different
// transfer functions. Therefore the Y'CbCr matrix is first inverted to obtain
// nonlinear BT.709 R'G'B', the BT.709 OETF is reversed to obtain linear RGB,
// and the sRGB OETF is then applied.
//
// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4), IEC 61966-2-1:1999
int
YCbCr2RGB (int luma, int chromab, int chromar, int *rgb) {

  int i;
  double rgb1[3], y1, pb, pr;

  // BT.709 matrix coefficients as specified by ITU-R BT.709.
  const double KR = 0.2126;
  const double KB = 0.0722;
  const double KG = 1.0 - KR - KB;  // 0.7152

  const double Yoffset = 16.0;
  const double Cboffset = 128.0;
  const double Croffset = 128.0;
  const double yscale = 219.0;   // 235 - 16
  const double cbscale = 224.0;  // 240 - 16
  const double crscale = 224.0;  // 240 - 16

  if ((rgb == NULL) ||
      (luma < 16) || (luma > 235) ||
      (chromab < 16) || (chromab > 240) ||
      (chromar < 16) || (chromar > 240)) {
    return (EXIT_FAILURE);
  }

  // Convert studio-range code values to normalized BT.709 Y', Pb, and Pr.
  y1 = ((double) luma - Yoffset) / yscale;
  pb = ((double) chromab - Cboffset) / cbscale;
  pr = ((double) chromar - Croffset) / crscale;

  // Invert the BT.709 Y'PbPr matrix to obtain nonlinear BT.709 R'G'B'.
  rgb1[0] = y1 + (2.0 * (1.0 - KR) * pr);
  rgb1[1] = y1 - ((2.0 * KB * (1.0 - KB) / KG) * pb)
               - ((2.0 * KR * (1.0 - KR) / KG) * pr);
  rgb1[2] = y1 + (2.0 * (1.0 - KB) * pb);

  // Reverse the BT.709 OETF to obtain linear-light RGB.
  // Values below zero can occur for legal Y'CbCr code combinations; the
  // linear branch correctly extends to those values before final clipping.
  for (i = 0; i < 3; i++) {
    if (rgb1[i] < 0.081) {
      rgb1[i] /= 4.5;
    } else {
      rgb1[i] = pow ((rgb1[i] + 0.099) / 1.099, 1.0 / 0.45);
    }
  }

  // Apply the sRGB OETF to linear RGB.
  for (i = 0; i < 3; i++) {
    if (rgb1[i] > 0.0031308) {
      rgb1[i] = (1.055 * pow (rgb1[i], 1.0 / 2.4)) - 0.055;
    } else {
      rgb1[i] *= 12.92;
    }
  }

  // Quantize to 8-bit sRGB and clip out-of-gamut undershoot/overshoot.
  for (i = 0; i < 3; i++) {
    long value;

    value = lround (rgb1[i] * 255.0);

    if (value < 0L) value = 0L;
    if (value > 255L) value = 255L;

    rgb[i] = (int) value;
  }

  return (EXIT_SUCCESS);
}

// Obtain a text string from standard input. It can include spaces.
int
inputtext (char *text) {

  int ch;
  size_t len;

  if (text == NULL) {
    return (EXIT_FAILURE);
  }

  if (fgets (text, MAXLEN, stdin) == NULL) {
    fprintf (stderr, "Unable to read text from standard input.\n");
    return (EXIT_FAILURE);
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
  if (len == (size_t) MAXLEN - 1u) {

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

    fprintf (stderr, "Input text is too long; maximum is %d characters.\n", MAXLEN - 1);
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
