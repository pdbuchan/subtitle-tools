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

// webvtt2srt.h

// Shared definitions for the WebVTT-to-SubRip converter.
//
// The converter:
//   - verifies the WEBVTT header;
//   - ignores WebVTT header metadata and repeated segment headers;
//   - ignores NOTE, STYLE, and REGION blocks;
//   - discards optional WebVTT cue identifiers;
//   - discards WebVTT cue settings after the ending timestamp;
//   - converts WebVTT timestamps from '.' milliseconds to SRT ',';
//   - supplies a 00-hour field when WebVTT uses MM:SS.mmm timestamps;
//   - preserves WebVTT bold, italic, and underline formatting;
//   - converts recognized WebVTT foreground colors to SRT <font> markup;
//   - strips unsupported WebVTT cue-text tags while preserving visible text;
//   - discards ruby annotation text and intra-cue timestamp tags;
//   - accepts either LF or CRLF input line endings;
//   - accepts an optional UTF-8 BOM at the beginning of the input file.
//
// Usage:
//   webvtt2srt input.webvtt
//
// Output:
//   out.srt
//
// Build:
//   make

#ifndef WEBVTT2SRT_H
#define WEBVTT2SRT_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_LINE_SIZE 256u
#define INITIAL_BLOCK_SIZE 8u

typedef struct {
  char *text;
  unsigned long number;
} LINE;

typedef struct {
  LINE *line;
  size_t count;
  size_t capacity;
} BLOCK;

// Function prototypes.
int append_line (BLOCK *, char *, unsigned long);
int convert_cue_text (const BLOCK *, size_t, char **);
int convert_file (FILE *, FILE *, const char *);
int convert_timestamp (const char *, char *, size_t);
int convert_timing_line (const char *, char *, size_t);
void free_block (BLOCK *);
int is_webvtt_header (const char *);
int process_block (FILE *, const BLOCK *, unsigned long *);
int readline (FILE *, char **);
int starts_keyword (const char *, const char *);

#endif
