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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>  // uint8_t, size_t, etc.
#include <string.h>
#include <math.h>  // pow()
#include <sys/mman.h>  // mmap(), munmap()
#include <sys/stat.h>  // fstat()
#include <fcntl.h>  // open()
#include <unistd.h>  // close(), read()
#include <errno.h>

#define MAX_STRINGLEN 256  // Maximum length of a character string
#define MAX_NUMLEN 8  // Maximum number of bytes to compose a value
#define MAX_PALETTES 16  // Maximum number of palettes per epoch
#define MAX_PALETTE_ENTRIES 256 // Maximum number of palette entries within one palette; must be 256 since one byte is used for ID
#define MAX_OBJECTS 65536  // Maximum number of image objects
#define MAX_OBJECT_BUFFER_LEN 8294400  // Worst case of 1 RLE byte per pixel for 4K give 3840 x 2160.
#define MAX_SUBS 10000  // Assume a maximum of 10,000 subtitles to sync in a PGS .sup file.
#define MAX_CHANGES 1000000  // Huge; Maximum number of bytes we can change in a .sup file to account for all PTS/DTS changes due to offset or sync option

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

// Segment header parameters
typedef struct {
  TIME pts;
  TIME dts;
  uint8_t segment_type;  // PDS, ODS, PCS, WDS, or END
  size_t segment_size;
} HEAD;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t alpha;
} PALETTE_ENTRY;

typedef struct {
  uint8_t version;
  PALETTE_ENTRY *entry;
} PALETTE;

// Undecoded RLE image data to be fed to decode().
typedef struct {
  size_t length;
  uint8_t *buffer;  // Undecoded RLE image data buffer
} OBJECT;

// Subtitle
typedef struct {
  TIME start;
  TIME end;
  size_t height;  // Subtitle height in px
  size_t width;  // Subtitle width in px
  uint8_t *buffer;  // Contains decoded image from decode()
} SUB;

// Flag indicating whether current PTS is the subtitle start time, end time, or in the midddle.
// Used by state.pts_type
typedef enum {
  PTS_MIDDLE = 0,
  PTS_START,
  PTS_END
} PTS_TYPE;

typedef struct {
  int prescan;  // Flag to indicate we're pre-analyzing the .sup file to get subtitle durations, needed for sync option.
  int subtitle_active;
  uint8_t num_objects;  // Number of Composition Objects
  uint8_t composition_state;
  size_t npalettes;
  uint8_t palette_update_flag;
  size_t current_palette;
  uint8_t seq_flag;  // Flag indicating ODS sequence: 0 = not in a sequence or in last ODS, 1 = within a sequence
  uint16_t object_id;
  uint16_t prev_object_id;
  TIME pts;
  PTS_TYPE pts_type;
} STATE;

// Subtitle start and end times; populated by prescanning the .sup file.
typedef struct {
  TIME start;
  TIME end;
} SYNC;

// Function Prototypes
int inputtext (char *);
int parse_header (STATE *, uint8_t *, size_t, size_t *, OPTIONS *, HEAD *, SYNC *, FILE *);
int parse_pcs (STATE *, uint8_t *, size_t, size_t *, HEAD *, OBJECT *, PALETTE *, FILE *);
int parse_wds (STATE *, uint8_t *, size_t, size_t *, HEAD *, FILE *);
int parse_pds (STATE *, uint8_t *, size_t, size_t *, HEAD *, PALETTE *, FILE *);
int parse_ods (STATE *, uint8_t *, size_t, size_t *, HEAD *, PALETTE *, OBJECT *, SUB *, FILE *);
int decode_rle (STATE *, PALETTE *, uint8_t *, size_t, uint8_t *);
int write_bmp (SUB *);
int rowcol2index (int, int, int, int);
int parse_timestamp (char *, TIME *);
int timetoms (TIME *);
int mstotime (TIME *);
double framerates (uint8_t);
int YCbCr2RGB_bt709 (int, int, int, int *);
int clear_palettes (PALETTE *);
int clear_objects (OBJECT *);
void write_u16_le (FILE *, uint16_t);
void write_u32_le (FILE *, uint32_t);
void write_s32_le (FILE *, int32_t);
uint8_t *inttofourbytes (uint8_t *, int32_t);
void record_u32_change (OPTIONS *, size_t, uint32_t);
char *allocate_strmem (int);
uint8_t *allocate_u8mem (int);
PALETTE_ENTRY *allocate_palentrymem (int);
PALETTE *allocate_pdsmem (int);
OBJECT *allocate_objmem (int);
SYNC *allocate_syncmem (int);
CHANGE *allocate_changemem (int);
