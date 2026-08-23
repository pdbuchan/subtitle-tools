/*  Copyright (C) 2026 P. David Buchan (pdbuchan@gmail.com)
  
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

#include "dvb.h"

// Convert BT.601 YCbCr colour values to 8-bit sRGB.
//
// This decoder interprets the DVB subtitle CLUT values using BT.601 colour
// coding. At this point reduced-resolution CDS values have already been
// expanded to their normal 8-bit component positions by parse_cds().
//
// References: SMPTE RP 177-1993, ITU-R BT.601-7, ITU-T H.273 (V4),
// IEC 61966-2-1:1999.
int
YCbCr2RGB_bt601 (int luma, int chromab, int chromar, int *rgb) {

  size_t i;
  double v[3], y = (double) luma, cb = (double) chromab, cr = (double) chromar, y1, pb, pr, r, g, b;
  const double KB = 0.114, KR = 0.299, KG = 1.0 - KB - KR;
  double m[3][3];

  // Convert incoming integer YCbCr values to normalized Y', Pb, and Pr.
  // The conversion uses the conventional limited-range BT.601 representation:
  //   16 <= Y <= 235, 16 <= Cb <= 240, 16 <= Cr <= 240.
  // The normalized ranges are 0 to 1 for Y', and -0.5 to 0.5 for Pb and Pr.
  y1 = (y - 16.0) / 219.0;
  pb = (cb - 128.0) / 224.0;
  pr = (cr - 128.0) / 224.0;

  // Define the BT.601 Y'PbPr-to-R'G'B' colour matrix.
  // m[row][column]
  m[0][0] = 1;
  m[0][1] = 0;
  m[0][2] = 2 - 2 * KR;
  m[1][0] = 1;
  m[1][1] = -(KB / KG) * (2 - 2 * KB);
  m[1][2] = -(KR / KG) * (2 - 2 * KR);
  m[2][0] = 1;
  m[2][1] = 2 - 2 * KB;
  m[2][2] = 0;

  // Multiply the colour matrix by the Y', Pb, Pr vector to obtain
  // gamma-corrected BT.601 R', G', and B'.
  for (i = 0; i < 3; i++) v[i] = m[i][0] * y1 + m[i][1] * pb + m[i][2] * pr;

  // Reverse the BT.601 camera gamma correction to obtain linear RGB.
  for (i = 0; i < 3; i++) {
    if (v[i] < 0.081) v[i] /= 4.5;
    else v[i] = pow ((v[i] + 0.099) / 1.099, 1.0 / 0.45);
  }

  // Apply the sRGB transfer function to the linear RGB components.
  for (i = 0; i < 3; i++) {
    if (v[i] > 0.0031308) v[i] = 1.055 * pow (v[i], 1.0 / 2.4) - 0.055;
    else v[i] *= 12.92;
  }

  // Scale the normalized sRGB values to the 8-bit range and round to the
  // nearest integer.
  r = v[0] * 255.0;
  g = v[1] * 255.0;
  b = v[2] * 255.0;
  rgb[0] = (int) (r + 0.5);
  rgb[1] = (int) (g + 0.5);
  rgb[2] = (int) (b + 0.5);

  // Clip any undershoot or overshoot caused by conversion between the
  // slightly different BT.601 YCbCr and sRGB gamuts.
  for (i = 0; i < 3; i++) {
    if (rgb[i] < 0) rgb[i] = 0;
    if (rgb[i] > 255) rgb[i] = 255;
  }

  return (EXIT_SUCCESS);
}
