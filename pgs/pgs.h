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
#include <inttypes.h>  // uint8_t, uint32_t, int64_t, size_t, etc.
#include <stdint.h>
#include <string.h>
#include <math.h>  // llround()
#include <limits.h>
#include <sys/mman.h>  // mmap(), munmap()
#include <sys/stat.h>  // fstat()
#include <fcntl.h>  // open()
#include <unistd.h>  // close(), read()
#include <errno.h>

#define MAX_STRINGLEN 256  // Maximum length of a character string
#define MAX_PALETTES 8  // Maximum number of palettes per epoch
#define MAX_PALETTE_ENTRIES 256  // Palette entry IDs are one byte.
#define MAX_OBJECTS 65536  // Object IDs are two bytes.
#define MAX_COMPOSITION_OBJECTS 2  // A PGS presentation may reference up to two objects.
#define MAX_SUBS 10000  // Maximum number of subtitle presentation intervals used for synchronization.
#define MAX_CHANGES 1000000  // Maximum number of changed bytes in an offset/synchronized .sup file.

// Array of byte changes to original file if we do offset or sync.
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

// User options.
typedef struct {
  uint8_t makebmp_flag;
  uint8_t offset_flag;
  TIME offset;
  int64_t offset_ticks;
  uint8_t sync_flag;
  // "First" and "last" subtitle start times for resynchronizing subtitles to anchor points.
  int64_t oldfirstms;
  int64_t oldlastms;
  int64_t newfirstms;
  int64_t newlastms;
  size_t nchanges;
  CHANGE *change;
} OPTIONS;

// Segment header parameters.
typedef struct {
  TIME pts;
  TIME dts;
  uint32_t pts_ticks;
  uint32_t dts_ticks;
  size_t pts_offset;
  size_t dts_offset;
  uint8_t segment_type;  // PDS, ODS, PCS, WDS, or END
  size_t segment_size;
  size_t segment_end;
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

// One decoded PGS object. RLE bytes are retained while a fragmented object is assembled;
// pixels contains the final decoded 8-bit palette indices.
typedef struct {
  uint8_t version;
  size_t width;
  size_t height;
  size_t length;
  size_t expected_length;
  size_t remaining_length;
  uint8_t *buffer;  // RLE-compressed bytes
  uint8_t *pixels;  // Decoded palette-index image, width * height bytes
  int complete;
} OBJECT;

// One object reference in a PCS presentation.
typedef struct {
  uint16_t object_id;
  uint8_t window_id;
  uint8_t composition_flag;  // 0x80 = cropped, 0x40 = forced
  uint16_t x;
  uint16_t y;
  uint16_t crop_x;
  uint16_t crop_y;
  uint16_t crop_width;
  uint16_t crop_height;
} COMPOSITION_OBJECT;

// Subtitle bitmap snapshot.
typedef struct {
  TIME start;
  TIME end;
  size_t height;
  size_t width;
  size_t buffer_size;
  uint8_t *buffer;  // RGBA pixels
} SUB;

// Flag indicating whether current PTS is a subtitle start time, end time, both, or neither.
typedef enum {
  PTS_MIDDLE = 0,
  PTS_START,
  PTS_END,
  PTS_END_START
} PTS_TYPE;

typedef struct {
  int prescan;  // Pre-analyzing the .sup file to obtain subtitle durations for synchronization.
  int subtitle_active;
  uint8_t num_objects;  // Number of Composition Objects
  uint8_t composition_state;
  uint16_t composition_number;
  uint8_t palette_update_flag;
  size_t current_palette;
  uint16_t video_width;
  uint16_t video_height;
  COMPOSITION_OBJECT composition_object[MAX_COMPOSITION_OBJECTS];
  TIME pts;
  PTS_TYPE pts_type;
} STATE;

// Subtitle start and end times/ticks; populated by the synchronization prescan.
typedef struct {
  TIME start;
  TIME end;
  uint32_t start_ticks;
  uint32_t end_ticks;
} SYNC;

// Function Prototypes
int inputtext (char *);
int parse_header (STATE *, uint8_t *, size_t, size_t *, HEAD *, FILE *);
int parse_pcs (STATE *, uint8_t *, size_t, size_t *, HEAD *, OBJECT *, PALETTE *, FILE *);
int parse_wds (STATE *, uint8_t *, size_t, size_t *, HEAD *, FILE *);
int parse_pds (STATE *, uint8_t *, size_t, size_t *, HEAD *, PALETTE *, FILE *);
int parse_ods (STATE *, uint8_t *, size_t, size_t *, HEAD *, OBJECT *, FILE *);
int decode_rle (uint8_t *, size_t, size_t, size_t, uint8_t *);
int render_subtitle (STATE *, PALETTE *, OBJECT *, SUB *);
int adjust_timestamps (STATE *, HEAD *, OPTIONS *, SYNC *);
int write_bmp (SUB *);
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
void record_u32_change (OPTIONS *, size_t, uint32_t);
char *allocate_strmem (size_t);
PALETTE_ENTRY *allocate_palentrymem (size_t);
PALETTE *allocate_pdsmem (size_t);
OBJECT *allocate_objmem (size_t);
SYNC *allocate_syncmem (size_t);
CHANGE *allocate_changemem (size_t);
