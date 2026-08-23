/*  Copyright (C) 2025-2026 P. David Buchan (pdbuchan@gmail.com)

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

#ifndef SUB_H
#define SUB_H

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_STRINGLEN 512
#define MAX_PALETTE 16
#define MAX_HD_PALETTE 256
#define PES_TS_MASK UINT64_C(0x1ffffffff)
#define PES_TS_MAX  PES_TS_MASK

// Array of byte changes to original file if we do offset or sync.
typedef struct {
  size_t offset;
  uint8_t new_value;
} CHANGE;

typedef struct {
  int h;
  int m;
  int s;
  int ms;
  int64_t totalms;
} TIME;

// User options.
typedef struct {
  uint8_t makebmp_flag;
  uint8_t offset_flag;
  TIME offset;
  uint8_t sync_flag;
  int64_t oldfirstms;
  int64_t oldlastms;
  int64_t newfirstms;
  int64_t newlastms;
  size_t nchanges;
  size_t change_capacity;
  CHANGE *change;
} OPTIONS;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} RGB;

// Data from .idx file.
typedef struct {
  TIME time_offset;
  size_t n_palette;
  RGB palette[MAX_PALETTE];
  size_t *n_timestamps;
  size_t langidx;
  size_t n_id;
  char **id;
  size_t *id_index;
  size_t **offset;
} IDX;

// Important parameters from MPEG-2 packetized elementary stream (PES).
typedef struct {
  size_t pes_packet_len;
  uint64_t pts90;
  uint64_t dts90;
  TIME pts;
  TIME dts;
} PES;

// One PX_CTLI color/contrast change region.  Coordinates are absolute display
// coordinates; the mapping applies from x_start rightward within y_start..y_end.
typedef struct {
  size_t x_start;
  size_t y_start;
  size_t y_end;
  uint8_t clut[4];
  uint8_t alpha[4];  // Unified 8-bit alpha values.
} COLCON_ENTRY;

// Important parameters from subpicture unit (SPU) data.
typedef struct {
  uint8_t clut[4];
  uint8_t alpha[MAX_HD_PALETTE];  // 8-bit alpha for classic and HD modes.
  RGB hd_palette[MAX_HD_PALETTE];
  uint8_t has_hd_palette;
  uint8_t is_8bit;
  uint8_t forced;
  size_t x_start, x_end;
  size_t y_start, y_end;
  size_t pxd_tf, pxd_bf;
  COLCON_ENTRY *colcon;
  size_t n_colcon;
  size_t colcon_capacity;
} SPU_PARMS;

// Results from decoding runlength-encoded (RLE) pixel data (PXD).
typedef struct {
  uint8_t color;
  size_t runlength;
  uint8_t to_eol;
} RLE;

// Some properties of a subtitle.
typedef struct {
  TIME start;
  TIME end;
  size_t width;
  size_t height;
} SUB;

int inputtext (char *);
int readline (FILE *, char *, int);
int parse_idx (IDX *, char **, size_t, FILE *);
int extract_subs (uint8_t *, size_t, OPTIONS *, IDX *, PES *, FILE *);
int parse_packets (OPTIONS *, uint8_t *, size_t, uint8_t **, size_t *, size_t, IDX *, size_t, PES *, SUB *, FILE *);
int parse_spu (const uint8_t *, size_t, IDX *, PES *, SPU_PARMS *, SUB *, FILE *);
int unpack_pxd (const uint8_t *, size_t, SPU_PARMS *, IDX *, uint8_t **, SUB *);
int decode_rle (const uint8_t *, size_t, size_t *, uint8_t, RLE *);
int write_bmp (const uint8_t *, IDX *, size_t, SUB *);
int write_idx_file (const char *, OPTIONS *);
int parse_timestamp (const char *, TIME *);
int mstotime (TIME *);
int timetoms (TIME *);
int transform_timestamp90 (const OPTIONS *, uint64_t, uint64_t *);
int transform_timestamp_ms (const OPTIONS *, int64_t, int64_t *);
void write_u16_le (FILE *, uint16_t);
void write_u32_le (FILE *, uint32_t);
void write_s32_le (FILE *, int32_t);
void record_pes_timestamp_change (OPTIONS *, size_t, uint64_t, uint8_t);
void sort_and_compact_changes (OPTIONS *);
void free_spu_parms (SPU_PARMS *);
uint8_t *allocate_u8mem (size_t);
char *allocate_strmem (size_t);
char **allocate_strmemp (size_t);
size_t *allocate_sizetmem (size_t);
size_t **allocate_sizetmemp (size_t);

#endif
