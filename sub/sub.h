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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/mman.h>  // mmap(), munmap()
#include <sys/stat.h>  // fstat()
#include <fcntl.h>  // open()
#include <unistd.h>  // close(), read()
#include <errno.h>

#define MAX_STRINGLEN 512  // Maximum length of a character string
#define MAX_PALETTE 16  // Maximum number of entries in palette; constrained by maximum value of a nibble; set by SET_COLOR DCSQ command
#define MAX_SPU_SIZE 65536  // Maximum size in bytes of a single SPU; constrained by maximum value SPU_SZ can hold
#define MAX_DCSQ 1000  // Maximum number of command sequence blocks (DCSQ) per table (DCSQT)
#define MAX_CMD 1000  // Maximum number of commands within a DCSQ block.
#define IMG_BUFFER_SIZE 3840 * 2160 * 4  // PAL DVD is 720x576, NTSC DVD is 720x480, but sub.c will work for idx/sub files from HD (1920x1080) and UHD (3840x2160) BDs too; x 4 for RGBA
#define MAX_CHANGES 1000000  // Huge; Maximum number of bytes we can change in a .sub file to account for all PTS/DTS changes due to offset or sync option

// Array of byte changes to original file if we do offset or sync
typedef struct {
  size_t offset;  // Index within .sup file of changed byte
  uint8_t new_value;
} CHANGE;

typedef struct {
  int h;
  int m;
  int s;
  int ms;
  int64_t totalms;
} TIME;

// User options
typedef struct {
  uint8_t makebmp_flag;
  uint8_t offset_flag;
  TIME offset;
  uint8_t sync_flag;
  // "First" and "last" subtitle total ms for resyncronizing subtitles to anchor points:
  int64_t oldfirstms;
  int64_t oldlastms;
  int64_t newfirstms;
  int64_t newlastms;
  size_t nchanges;
  CHANGE *change;
} OPTIONS;

typedef struct { 
  uint8_t r;
  uint8_t g;
  uint8_t b;
} RGB;

// Data from .idx file
typedef struct {
  TIME time_offset;  // For "time offset" keyword data.
  size_t n_palette;  // Should always count to 16.
  RGB palette[MAX_PALETTE];
  size_t *n_timestamps;  // Number of timestamps for a given language index
  size_t langidx;  // Default language index
  size_t n_id;  // Number of language IDs
  char **id;  // Array of language IDs
  size_t *id_index;  // Array of language indices corresponding to array of Language IDs above
  size_t **offset;  // Offsets within .sub file to find MPEG-2 PES stream for specific subtitle
} IDX;

// Important parameters from MPEG-2 packetized elementary stream (PES)
typedef struct {
  size_t pes_packet_len;
  TIME pts;
  TIME dts;
} PES;

// Important parameters from subpicture unit (SPU) data
typedef struct {
  uint8_t clut[4];  // Color lookup table index (4 bits); set by SET_COLOR
  uint8_t alpha[4];  // Alpha (4 bits); Set by SET_CONTR
  size_t x_start, x_end;  // Set by SET_DAREA
  size_t y_start, y_end;  // Set by SET_DAREA
  int pxd_tf, pxd_bf;  // Set by SET_DSPXA
} SPU_PARMS;

// Results from decoding runlength-encoded (RLE) pixel data (PXD)
typedef struct {
  uint8_t color;  // "color" from PXD is 2-bit index of CLUT, which is 4-bit index of palette in .idx
  size_t runlength;  // Number of pixels in run with same color
  uint8_t to_eol;  // Flag indicating the same pixel type to end of line
} RLE;

// Some properties of a subtitle
typedef struct {
  TIME start;
  TIME end;
  size_t width;
  size_t height;
} SUB;

// Function prototypes
int inputtext (char *);
int readline (FILE *, char *, int);
int parse_idx (IDX *, char **, size_t, FILE *);
int extract_subs (uint8_t *, size_t, OPTIONS *, IDX *, PES *, FILE *);
int parse_packets (OPTIONS *, uint8_t *, size_t, uint8_t *, int, IDX *, int, PES *, SUB *, FILE *);
int parse_spu (OPTIONS *, uint8_t *, IDX *, PES *, SPU_PARMS *, SUB *, FILE *);
int unpack_pxd (uint8_t *, size_t, SPU_PARMS *, IDX *, uint8_t *, SUB *);
int decode_rle (uint8_t *, size_t *, RLE *);
int get_16bits (uint8_t *, size_t *, uint16_t *);
int write_bmp (uint8_t *, IDX *, int, SUB *);
int write_idx_file (const char*, OPTIONS *);
int parse_timestamp (char *, TIME *);
int mstotime (TIME *);
int timetoms (TIME *);
void write_u16_le (FILE *, uint16_t);
void write_u32_le (FILE *, uint32_t);
void write_s32_le (FILE *, int32_t);
void record_pes_timestamp_change (OPTIONS *, size_t, uint64_t, uint8_t);
uint8_t *allocate_u8mem (int);
char *allocate_strmem (int);
char **allocate_strmemp (int);
size_t *allocate_sizetmem (int);
size_t **allocate_sizetmemp (int);
CHANGE *allocate_changemem (int);
