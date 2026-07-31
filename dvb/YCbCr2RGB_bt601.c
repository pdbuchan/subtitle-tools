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

// Convert YCbCr color to 8-bit sRGB.
// Assumes BT.601 color primaries and gamma-correction was used to produce YCbCr; sRGB uses BT.709 color primaries and sRGB gamma-correction.
// Standard definition video uses BT.601 colorspace.
// References: SMPTE RP 177-1993, ITU-R BT.601-7, ITU-T H.273 (V4), IEC 61966-2-1:1999
int
YCbCr2RGB_bt601 (uint8_t full_range_flag, int luma, int chromab, int chromar, int *rgb) {

  size_t i;
  double r, g, b, rgb1[3], y1, pb, pr, y, cb, cr, cm[3][3], yscale, pbscale, prscale;
  const double KB = 0.114;
  const double KR = 0.299;
  double KG = 1.0 - KB - KR;
  const double Yoffset = 16.0;
  const double Cboffset = 128.0;
  const double Croffset = 128.0;

  // Convert incoming YCbCr values to double.
  y = (double) luma;
  cb = (double) chromab;
  cr = (double) chromar;

  // Variable Definitions
  //   y, cb, and cr are the Luma, Color Difference Blue (Chroma Blue), and Color Difference Red (Chroma Red), where
  //   Limited range: 16 <= Y <= 235, 16 <= Cb <= 240, 16 <= Cr <= 240.
  //   Full range: 0 <= Y <= 255, 0 <= Cb <= 255, 0 <= Cr <= 255
  //   y1, pb, and pr are the normalized YCbCr signals (i.e., prior to scaling and offsets), where
  //   y1 (i.e., Y-prime) has range 0 to 1, and Pb and Pr have range -0.5 to 0.5.
  //   r1, g1, and b1 (i.e., r-prime, g-prime, and b-prime) are the normalized values of r, g, and b, each with range 0 to 1.

  // Set scaling factors to achieve required ranges.
  // Limited range case
  if (!full_range_flag) {

    yscale = 235.0 - 16.0;   // 16 <= Y <= 235, where 16 = black, 235 = white
    pbscale = 240.0 - 16.0;  // 16 <= Cb <= 240
    prscale = 240.0 - 16.0;  // 16 <= Cr <= 240

    // Normalize Y (16 to 235) to Y1 (0 to 1).
    y1 = (y - Yoffset) / yscale;

    // Normalize Cb (16 to 240) to Pb (-0.5 to 0.5).
    pb = (cb - Cboffset) / pbscale;

    // Normalize Cr (16 to 240) to Pr (-0.5 to 0.5).
    pr = (cr - Croffset) / prscale;

  // Full range case
  } else {
    y1 = y / 255.0;
    pb = (cb - 128.0) / 255.0;
    pr = (cr - 128.0) / 255.0;
  }

  // Define the color matrix.
  // cm[row][col]
  cm[0][0] = 1.0;    cm[0][1] = 0.0;                                cm[0][2] = 2.0 - (2.0 * KR);
  cm[1][0] = 1.0;    cm[1][1] = -(KB / KG) * (2.0 - (2.0 * KB));    cm[1][2] = -(KR / KG) * (2.0 - (2.0 * KR));
  cm[2][0] = 1.0;    cm[2][1] = 2.0 - (2.0 * KB);                   cm[2][2] = 0.0;

  // Multiply color matrix by y1,pb,pr vector to obtain r1,g1,b1.
  for (i = 0; i < 3; i++) {
    rgb1[i] = (cm[i][0] * y1) + (cm[i][1] * pb) + (cm[i][2] * pr);
  }

  // Reverse BT.601 camera gamma-correction. i.e., convert to linear rgb
  for (i = 0; i < 3; i++) {
    if (rgb1[i] < 0.081) {
      rgb1[i] /= 4.5;
    } else {
      rgb1[i] = pow ((rgb1[i] + 0.099) / 1.099, 1.0 / 0.45);
    }
  }

  // Apply sRGB gamma-correction to linear rgb.
  for (i = 0; i < 3; i++) {
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
  for (i = 0; i < 3; i++) {
    if (rgb[i] < 0) rgb[i] = 0;
    if (rgb[i] > 255) rgb[i] = 255;
  }

  return (EXIT_SUCCESS);
}
