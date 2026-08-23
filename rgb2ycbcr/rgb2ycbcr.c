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

// Convert 8-bit sRGB to YCbCr (BT.709).
// Assumes the input RGB values are standard nonlinear sRGB values. The sRGB
// transfer function is reversed to obtain linear RGB, then the BT.709 OETF is
// applied before the BT.709 Y'CbCr matrix and studio-range quantization.

// gcc -Wall rgb2ycbcr.c -lm -o rgb2ycbcr

// Usage: ./rgb2ycbcr

// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4), IEC 61966-2-1:1999

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>  // pow(), lround()
#include <errno.h>
#include <ctype.h>

// Function prototypes
int inputtext (char *);
int parse_rgb_value (const char *, int *);
int RGB2YCbCr (int, int, int, int *);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters per line

int
main (void) {

  int r, g, b, ycbcr[3];
  char temp[MAXLEN];

  fprintf (stdout, "\nRed value (0-255)? ");
  memset (temp, 0, sizeof (temp));
  inputtext (temp);
  if (parse_rgb_value (temp, &r) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Red value must be an integer from 0 through 255: %s\n", temp);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "Green value (0-255)? ");
  memset (temp, 0, sizeof (temp));
  inputtext (temp);
  if (parse_rgb_value (temp, &g) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Green value must be an integer from 0 through 255: %s\n", temp);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "Blue value (0-255)? ");
  memset (temp, 0, sizeof (temp));
  inputtext (temp);
  if (parse_rgb_value (temp, &b) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Blue value must be an integer from 0 through 255: %s\n", temp);
    return (EXIT_FAILURE);
  }

  // Convert RGB to YCbCr.
  if (RGB2YCbCr (r, g, b, ycbcr) == EXIT_FAILURE) {
    fprintf (stderr, "ERROR: Unable to convert RGB to YCbCr.\n");
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "\nRGB (%i, %i, %i) = YCbCr (%i, %i, %i)\n", r, g, b, ycbcr[0], ycbcr[1], ycbcr[2]);
  fprintf (stdout, "RGB (0x%02x, 0x%02x, 0x%02x) = YCbCr (0x%02x, 0x%02x, 0x%02x)\n", r, g, b, ycbcr[0], ycbcr[1], ycbcr[2]);

  return (EXIT_SUCCESS);
}

// Parse one RGB component and require the entire non-whitespace input to be a
// decimal integer in the inclusive range 0 through 255.
int
parse_rgb_value (const char *text, int *value) {

  char *endptr;
  long n;

  if ((text == NULL) || (value == NULL)) {
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

  if ((*endptr != '\0') || (n < 0L) || (n > 255L)) {
    return (EXIT_FAILURE);
  }

  *value = (int) n;

  return (EXIT_SUCCESS);
}

// Convert 8-bit nonlinear sRGB color to studio-range BT.709 Y'CbCr.
//
// sRGB and BT.709 use the same D65 chromaticities and primaries but different
// transfer functions. Therefore the input sRGB values are first linearized
// with the inverse sRGB transfer function and then encoded with the BT.709
// OETF before applying the BT.709 Y'CbCr matrix.
//
// The resulting 8-bit code ranges are:
//   16 <= Y' <= 235
//   16 <= Cb <= 240
//   16 <= Cr <= 240
//
// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4), IEC 61966-2-1:1999
int
RGB2YCbCr (int r, int g, int b, int *yCbCr) {

  int i;
  double rgb1[3], y1, pb, pr, y, cb, cr;

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

  if ((yCbCr == NULL) ||
      (r < 0) || (r > 255) ||
      (g < 0) || (g > 255) ||
      (b < 0) || (b > 255)) {
    return (EXIT_FAILURE);
  }

  // Normalize R',G',B' from 0-255 to 0-1.
  rgb1[0] = (double) r / 255.0;
  rgb1[1] = (double) g / 255.0;
  rgb1[2] = (double) b / 255.0;

  // Reverse the sRGB transfer function to obtain linear-light RGB.
  for (i = 0; i < 3; i++) {
    if (rgb1[i] > 0.04045) {
      rgb1[i] = pow ((rgb1[i] + 0.055) / 1.055, 2.4);
    } else {
      rgb1[i] /= 12.92;
    }
  }

  // Apply the BT.709 OETF to linear RGB.
  for (i = 0; i < 3; i++) {
    if (rgb1[i] < 0.018) {
      rgb1[i] *= 4.5;
    } else {
      rgb1[i] = (1.099 * pow (rgb1[i], 0.45)) - 0.099;
    }
  }

  // Derive normalized BT.709 luma and color-difference signals.
  y1 = (KR * rgb1[0]) + (KG * rgb1[1]) + (KB * rgb1[2]);
  pb = 0.5 * (rgb1[2] - y1) / (1.0 - KB);
  pr = 0.5 * (rgb1[0] - y1) / (1.0 - KR);

  // Quantize to 8-bit studio-range Y'CbCr.
  y = (y1 * yscale) + Yoffset;
  cb = (pb * cbscale) + Cboffset;
  cr = (pr * crscale) + Croffset;

  yCbCr[0] = (int) lround (y);
  yCbCr[1] = (int) lround (cb);
  yCbCr[2] = (int) lround (cr);

  // Clip any small numerical undershoot or overshoot to legal code ranges.
  if (yCbCr[0] < 16) yCbCr[0] = 16;
  if (yCbCr[0] > 235) yCbCr[0] = 235;

  if (yCbCr[1] < 16) yCbCr[1] = 16;
  if (yCbCr[1] > 240) yCbCr[1] = 240;

  if (yCbCr[2] < 16) yCbCr[2] = 16;
  if (yCbCr[2] > 240) yCbCr[2] = 240;

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
