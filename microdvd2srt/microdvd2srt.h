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

// microdvd2srt.h

// Shared definitions for the MicroDVD-to-SubRip converter.
//
// The converter:
//   - asks the user for the video frame rate;
//   - accepts decimal frame rates and rational values such as 24000/1001;
//   - recognizes and skips an optional first {1}{1}FPS information line;
//   - converts MicroDVD frame numbers to SRT HH:MM:SS,mmm timestamps;
//   - converts the MicroDVD '|' line separator to an SRT newline;
//   - preserves bold, italic, and underline formatting using SRT markup;
//   - converts MicroDVD BGR colors to SRT RGB <font> colors;
//   - converts font-name and font-size tags to SRT <font> attributes;
//   - honors {DEFAULT} formatting regardless of where it occurs in the file;
//   - recognizes the older leading '/' convention for italic text;
//   - discards strike-through, charset, and positioning controls because
//     SubRip has no sufficiently portable equivalents for them;
//   - ignores optional [BEGIN] and [END] marker lines;
//   - accepts either LF or CRLF input line endings;
//   - accepts an optional UTF-8 BOM at the beginning of the input file;
//   - detects and rejects other recognized BOM-marked encodings because the
//     MicroDVD parser is byte-oriented and does not transcode character data.
//
// Usage:
//   microdvd2srt input.sub
//
// Output:
//   out.srt
//
// Build:
//   make

#ifndef MICRODVD2SRT_H
#define MICRODVD2SRT_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_LINE_SIZE 256u
#define FONT_NAME_SIZE 256u
#define BOM_BUFFER_SIZE 4u

#define STYLE_ITALIC 0x01u
#define STYLE_BOLD 0x02u
#define STYLE_UNDERLINE 0x04u
#define STYLE_STRIKE 0x08u

typedef struct {
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

typedef struct {
  unsigned int styles;
  unsigned int color;
  unsigned int size;
  char font[FONT_NAME_SIZE];
  int has_color;
  int has_size;
  int has_font;
} FORMAT_STATE;

// Byte Order Mark table.
extern const BOM bom[];
extern const size_t nbom;

// Function prototypes.
int apply_format_tag (FORMAT_STATE *, char, const char *, size_t, int *);
int byteordermark (const uint8_t *, size_t, const BOM *, size_t);
int convert_file (FILE *, FILE *, const char *, long double);
int convert_text (const char *, const FORMAT_STATE *, char **);
int frame_to_timestamp (unsigned long long, long double, char *, size_t);
int get_frame_rate (long double *);
int parse_default_line (const char *, FORMAT_STATE *, int *);
int parse_frame_rate (const char *, long double *);
int parse_subtitle_line (const char *, unsigned long long *, unsigned long long *, const char **);
int readline (FILE *, char **);
int scan_defaults (FILE *, const char *, FORMAT_STATE *);

#endif
