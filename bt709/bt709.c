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

// bt709.c - Derive the color constants for BT.709 YCbCr colorspace.
// References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4)

// gcc -Wall bt709.c -lm -o bt709

// Usage: ./bt709

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Function prototypes
int gaussjordan (int, double **);
int *allocate_intmem (int);
double *allocate_doublemem (int);
double **allocate_doublememp (int);

int
main (int argc, char **argv) {

  int i, j, k;
  double zr, zg, zb, zw, **p, *w, **pinv, *coeff, **c, **npm;

  // Red BT.709 color primaries
  const double xr = 0.640;
  const double yr = 0.330;
  zr = 1.0 - (xr + yr);

  // Green BT.709 color primaries
  const double xg = 0.300;
  const double yg = 0.600;
  zg = 1.0 - (xg + yg);

  // Blue BT.709 color primaries
  const double xb = 0.150;
  const double yb = 0.060;
  zb = 1.0 - (xb + yb);

  // White D65 BT.709 color primaries
  const double xw = 0.3127;
  const double yw = 0.3290;
  zw = 1.0 - (xw + yw);

  // Allocate memory for various arrays.
  p = allocate_doublememp (3);
  pinv = allocate_doublememp (3);
  c = allocate_doublememp (3);
  npm = allocate_doublememp (3);
  for (i=0; i<3; i++) {
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

  // Populate white matrix.
  w[0] = xw / yw;
  w[1] = 1.0;
  w[2] = zw / yw;

  fprintf (stdout, "BT.709 - Derivation of Color Constants\n");
  fprintf (stdout, "References: SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273 (V4)\n\n");

  // Show color primaries matrix p.
  fprintf (stdout, "Color primaries matrix p:\n");
  for (i=0; i<3; i++) {
    fprintf (stdout, "  ");
    for (j=0; j<3; j++) {
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
  for (i=0; i<3; i++) {
    fprintf (stdout, "  ");
    for (j=0; j<3; j++) {
      fprintf (stdout, "%0.4lf ", pinv[i][j]);
    }
    fprintf (stdout, "\n");
  }
  fprintf (stdout, "\n");

// Test to see if p * pinv = identity vector.
/*
double v;
  for (i=0; i<3; i++) {
    v = 0.0;
    for (j=0; j<3; j++) {
      v += p[i][j] * pinv[j][i];
    }
    fprintf (stdout, "%0.4lf\n", v);
  }
*/

  // Calculate RGB normalization coefficients.
  // coef = pinv * w
  for (i=0; i<3; i++) {
    for (j=0; j<3; j++) {
      coeff[i] += pinv[i][j] * w[j];
    }
  }

  // Show normalization coefficients.
  fprintf (stdout, "Normalization coefficient vector:\n");
  for (i=0; i<3; i++) {
    fprintf (stdout, "  %0.4lf\n", coeff[i]);
  }
  fprintf (stdout, "\n");

  // Create diagonal coefficient matrix using normalization coefficients.
  c[0][0] = coeff[0];
  c[1][1] = coeff[1];
  c[2][2] = coeff[2];

  // Show diagonal coefficient matrix.
  fprintf (stdout, "Diagonal normalization coefficient matrix c:\n");
  for (i=0; i<3; i++) {
    fprintf (stdout, "  ");
    for (j=0; j<3; j++) {
      fprintf (stdout, "%0.4lf ", c[i][j]);
    }
    fprintf (stdout, "\n");
  }
  fprintf (stdout, "\n");

  // Compute NPM matrix, where NPM = P * C.
  for (i=0; i<3; i++) {
    for (j=0; j<3; j++) {
      for (k=0; k<3; k++) {
        npm[i][j] += p[i][k] * c[j][k];
      }
    }
  }

  // Show NPM matrix.
  fprintf (stdout, "NPM matrix (p * c):\n");
  for (i=0; i<3; i++) {
    fprintf (stdout, "  ");
    for (j=0; j<3; j++) {
      fprintf (stdout, "%0.4lf ", npm[i][j]);
    }
    fprintf (stdout, "\n");
  }
  fprintf (stdout, "\n");

  // Show luminance equation constants (second row of NPM matrix).
  fprintf (stdout, "\nLuminance equation constants YR, YG, and YB (second row of NPM matrix) (sometimes referred to as KR, KG, and KB)\n");
  fprintf (stdout,"Y = YR * R + YG * G + YB * B\n");
  fprintf (stdout, "  YR: %0.10lf\n", npm[1][0]);
  fprintf (stdout, "  YG: %0.10lf\n", npm[1][1]);
  fprintf (stdout, "  YB: %0.10lf\n", npm[1][2]);
  fprintf (stdout, "  SUM: YR + YG + YB = %0.10lf\n\n", npm[1][0] + npm[1][1] + npm[1][2]);

  // Free allocated memory.
  for (i=0; i<3; i++) {
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

// Gauss-Jordan in-place inversion of n*n matrix.
int
gaussjordan (int n, double **matrix) {

  int i, j, k;
  double **augmented, pivot, factor;

  // Allocate memory for various arrays.
  augmented = allocate_doublememp (n);
  for (i=0; i<n; i++) {
    augmented[i] = allocate_doublemem (2 * n);  // Original matrix augmented by identity matrix
  }

  // Augment the matrix with the identity matrix.
  for (i=0; i<n; i++) {
    for (j=0; j<n; j++) {
      augmented[i][j] = matrix[i][j];
      augmented[i][j + n] = (i == j) ? 1.0 : 0.0;
    }
  }

  // Perform Gauss-Jordan elimination.
  for (i=0; i<n; i++) {

    // Find the pivot element.
    pivot = augmented[i][i];
    if (fabs (pivot) < 1e-9) {  // If pivot is too small, the matrix is singular.
      fprintf (stdout, "ERROR: Singular matrix in gaussjordan().\n");
      exit (EXIT_FAILURE);
    }

    // Normalize the pivot row.
    for (j=0; j<(2 * n); j++) {
      augmented[i][j] /= pivot;
    }

    // Eliminate the current column in all rows except the current row.
    for (j=0; j<n; j++) {
      if (j != i) {
        factor = augmented[j][i];
        for (k=0; k<(2 * n); k++) {
          augmented[j][k] -= factor * augmented[i][k];
        }
      }
    }
  }

  // Copy the right half of the augmented matrix back to the original matrix.
  for (i=0; i<n; i++) {
    for (j=0; j<n; j++) {
      matrix[i][j] = augmented[i][j + n];
    }
  }

  // Free allocated memory.
  for (i=0; i<n; i++) {
    free (augmented[i]);
  }
  free (augmented);

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

// Allocate memory for an array of doubles.
double *
allocate_doublemem (int len)
{ 
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
allocate_doublememp (int len)
{ 
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
