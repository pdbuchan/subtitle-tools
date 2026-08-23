/*  Copyright (C) 2024 P. David Buchan (pdbuchan@gmail.com)

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

// bt709.c - Derive the BT.709 normalized primary matrix and luminance constants.
// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4)

// gcc -Wall bt709.c -lm -o bt709

// Usage: ./bt709

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function prototypes
int gaussjordan (int, double **);
double *allocate_doublemem (size_t);
double **allocate_doublememp (size_t);

int
main (void) {

  int i, j, k;
  double zr, zg, zb, zw, **p, *w, **pinv, *coeff, **c, **npm;

  // Red BT.709 color primary.
  const double xr = 0.640;
  const double yr = 0.330;
  zr = 1.0 - (xr + yr);

  // Green BT.709 color primary.
  const double xg = 0.300;
  const double yg = 0.600;
  zg = 1.0 - (xg + yg);

  // Blue BT.709 color primary.
  const double xb = 0.150;
  const double yb = 0.060;
  zb = 1.0 - (xb + yb);

  // BT.709 D65 reference white.
  const double xw = 0.3127;
  const double yw = 0.3290;
  zw = 1.0 - (xw + yw);

  // Standard BT.709 Y'CbCr luma coefficients.
  const double kr = 0.2126;
  const double kb = 0.0722;
  const double kg = 1.0 - kr - kb;

  // Allocate memory for various arrays.
  p = allocate_doublememp (3);
  pinv = allocate_doublememp (3);
  c = allocate_doublememp (3);
  npm = allocate_doublememp (3);
  for (i = 0; i < 3; i++) {
    p[i] = allocate_doublemem (3);
    pinv[i] = allocate_doublemem (3);
    c[i] = allocate_doublemem (3);
    npm[i] = allocate_doublemem (3);
  }
  w = allocate_doublemem (3);
  coeff = allocate_doublemem (3);

  // Populate color primaries matrix p.
  p[0][0] = xr;  p[0][1] = xg;  p[0][2] = xb;
  p[1][0] = yr;  p[1][1] = yg;  p[1][2] = yb;
  p[2][0] = zr;  p[2][1] = zg;  p[2][2] = zb;

  // Populate white vector, normalized so that Y = 1.
  w[0] = xw / yw;
  w[1] = 1.0;
  w[2] = zw / yw;

  fprintf (stdout, "BT.709 - Derivation of Color Constants\n");
  fprintf (stdout, "References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4)\n\n");

  // Show color primaries matrix p.
  fprintf (stdout, "Color primaries matrix p:\n");
  for (i = 0; i < 3; i++) {
    fprintf (stdout, "  ");
    for (j = 0; j < 3; j++) {
      fprintf (stdout, "%0.4lf ", p[i][j]);
      pinv[i][j] = p[i][j];  // Copy matrix p to matrix pinv for later in-place inversion.
    }
    fprintf (stdout, "\n");
  }
  fprintf (stdout, "\n");

  // Compute inverse of color primaries matrix p.
  gaussjordan (3, pinv);

  // Show inverse of color primaries matrix.
  fprintf (stdout, "Inverse (pinv) of color primaries matrix:\n");
  for (i = 0; i < 3; i++) {
    fprintf (stdout, "  ");
    for (j = 0; j < 3; j++) {
      fprintf (stdout, "%0.4lf ", pinv[i][j]);
    }
    fprintf (stdout, "\n");
  }
  fprintf (stdout, "\n");

/*
  // Test to verify that p * pinv is the identity matrix.
  double v, expected;
  int identity_ok;

  identity_ok = 1;
  fprintf (stdout, "Test of p * pinv (should be the identity matrix):\n");
  for (i = 0; i < 3; i++) {
    fprintf (stdout, "  ");
    for (j = 0; j < 3; j++) {
      v = 0.0;
      for (k = 0; k < 3; k++) {
        v += p[i][k] * pinv[k][j];
      }
      fprintf (stdout, "%0.10lf ", v);

      // Diagonal elements should equal 1; off-diagonal elements should equal 0.
      expected = (i == j) ? 1.0 : 0.0;
      if (fabs (v - expected) > 1e-12) {
        identity_ok = 0;
      }
    }
    fprintf (stdout, "\n");
  }
  if (identity_ok) {
    fprintf (stdout, "p * pinv is the identity matrix within the test tolerance.\n\n");
  } else {
    fprintf (stdout, "ERROR: p * pinv is not the identity matrix within the test tolerance.\n\n");
  }
*/

  // Calculate RGB normalization coefficients, where coeff = pinv * w.
  for (i = 0; i < 3; i++) {
    coeff[i] = 0.0;
    for (j = 0; j < 3; j++) {
      coeff[i] += pinv[i][j] * w[j];
    }
  }

  // Show normalization coefficients.
  fprintf (stdout, "Normalization coefficient vector:\n");
  for (i = 0; i < 3; i++) {
    fprintf (stdout, "  %0.4lf\n", coeff[i]);
  }
  fprintf (stdout, "\n");

  // Create diagonal coefficient matrix using normalization coefficients.
  c[0][0] = coeff[0];
  c[1][1] = coeff[1];
  c[2][2] = coeff[2];

  // Show diagonal coefficient matrix.
  fprintf (stdout, "Diagonal normalization coefficient matrix c:\n");
  for (i = 0; i < 3; i++) {
    fprintf (stdout, "  ");
    for (j = 0; j < 3; j++) {
      fprintf (stdout, "%0.4lf ", c[i][j]);
    }
    fprintf (stdout, "\n");
  }
  fprintf (stdout, "\n");

  // Compute NPM matrix, where NPM = P * C.
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      npm[i][j] = 0.0;
      for (k = 0; k < 3; k++) {
        npm[i][j] += p[i][k] * c[k][j];
      }
    }
  }

  // Show NPM matrix.
  fprintf (stdout, "NPM matrix (p * c):\n");
  for (i = 0; i < 3; i++) {
    fprintf (stdout, "  ");
    for (j = 0; j < 3; j++) {
      fprintf (stdout, "%0.4lf ", npm[i][j]);
    }
    fprintf (stdout, "\n");
  }
  fprintf (stdout, "\n");

  // The second row of the RGB-to-XYZ NPM gives the linear-light luminance coefficients.
  // These are derived from the published BT.709 chromaticities and D65 reference white.
  fprintf (stdout, "Derived linear-light RGB-to-XYZ luminance coefficients:\n");
  fprintf (stdout, "Y = YR * R + YG * G + YB * B\n");
  fprintf (stdout, "  YR: %0.10lf\n", npm[1][0]);
  fprintf (stdout, "  YG: %0.10lf\n", npm[1][1]);
  fprintf (stdout, "  YB: %0.10lf\n", npm[1][2]);
  fprintf (stdout, "  SUM: YR + YG + YB = %0.10lf\n\n", npm[1][0] + npm[1][1] + npm[1][2]);

  // BT.709 specifies rounded luma coefficients for the non-linear R', G', and B' signals.
  // They are close to, but should not be confused with, the higher-precision NPM values above.
  fprintf (stdout, "Standard BT.709 Y'CbCr luma coefficients:\n");
  fprintf (stdout, "Y' = KR * R' + KG * G' + KB * B'\n");
  fprintf (stdout, "  KR: %0.4lf\n", kr);
  fprintf (stdout, "  KG: %0.4lf\n", kg);
  fprintf (stdout, "  KB: %0.4lf\n", kb);
  fprintf (stdout, "  SUM: KR + KG + KB = %0.4lf\n\n", kr + kg + kb);

  // Free allocated memory.
  for (i = 0; i < 3; i++) {
    free (p[i]);
    free (pinv[i]);
    free (c[i]);
    free (npm[i]);
  }
  free (p);
  free (pinv);
  free (w);
  free (coeff);
  free (c);
  free (npm);

  return (EXIT_SUCCESS);
}

// Gauss-Jordan in-place inversion of an n*n matrix using partial pivoting.
int
gaussjordan (int n, double **matrix) {

  int i, j, k, pivot_row;
  double **augmented, pivot, factor, max_pivot, *tmp_row;

  if (n <= 0) {
    fprintf (stderr, "ERROR: Matrix dimension n must be greater than zero in gaussjordan().\n");
    return (EXIT_FAILURE);
  }

  // Allocate memory for the augmented matrix [matrix | identity].
  augmented = allocate_doublememp ((size_t) n);
  for (i = 0; i < n; i++) {
    augmented[i] = allocate_doublemem ((size_t) (2 * n));
  }

  // Augment the matrix with the identity matrix.
  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      augmented[i][j] = matrix[i][j];
      augmented[i][j + n] = (i == j) ? 1.0 : 0.0;
    }
  }

  // Perform Gauss-Jordan elimination.
  for (i = 0; i < n; i++) {

    // Use partial pivoting: choose the row with the largest absolute value in this column.
    pivot_row = i;
    max_pivot = fabs (augmented[i][i]);
    for (j = i + 1; j < n; j++) {
      if (fabs (augmented[j][i]) > max_pivot) {
        max_pivot = fabs (augmented[j][i]);
        pivot_row = j;
      }
    }

    // A zero pivot after searching all remaining rows means the matrix is singular.
    if (max_pivot == 0.0) {
      fprintf (stderr, "ERROR: Singular matrix in gaussjordan().\n");
      for (j = 0; j < n; j++) {
        free (augmented[j]);
      }
      free (augmented);
      exit (EXIT_FAILURE);
    }

    // Move the selected pivot row into the current row position.
    if (pivot_row != i) {
      tmp_row = augmented[i];
      augmented[i] = augmented[pivot_row];
      augmented[pivot_row] = tmp_row;
    }

    // Normalize the pivot row so that its pivot element becomes one.
    pivot = augmented[i][i];
    for (j = 0; j < (2 * n); j++) {
      augmented[i][j] /= pivot;
    }

    // Eliminate the current column in all rows except the current row.
    for (j = 0; j < n; j++) {
      if (j != i) {
        factor = augmented[j][i];
        for (k = 0; k < (2 * n); k++) {
          augmented[j][k] -= factor * augmented[i][k];
        }
      }
    }
  }

  // Copy the right half of the augmented matrix back to the original matrix.
  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      matrix[i][j] = augmented[i][j + n];
    }
  }

  // Free allocated memory.
  for (i = 0; i < n; i++) {
    free (augmented[i]);
  }
  free (augmented);

  return (EXIT_SUCCESS);
}

// Allocate and zero memory for an array of doubles.
double *
allocate_doublemem (size_t len) {

  void *tmp;

  if (len == 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = 0 in allocate_doublemem().\n");
    exit (EXIT_FAILURE);
  }

  if (len > SIZE_MAX / sizeof (double)) {
    fprintf (stderr, "ERROR: Requested array is too large in allocate_doublemem().\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (double));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_doublemem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate and zero memory for an array of pointers to arrays of doubles.
double **
allocate_doublememp (size_t len) {

  void *tmp;

  if (len == 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = 0 in allocate_doublememp().\n");
    exit (EXIT_FAILURE);
  }

  if (len > SIZE_MAX / sizeof (double *)) {
    fprintf (stderr, "ERROR: Requested array is too large in allocate_doublememp().\n");
    exit (EXIT_FAILURE);
  }

  tmp = calloc ((size_t) len, sizeof (double *));
  if (tmp != NULL) {
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_doublememp().\n");
    exit (EXIT_FAILURE);
  }
}
