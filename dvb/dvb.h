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
#include <math.h>  // pow()
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>  // INT_MAX, INT_MIN
#include <sys/mman.h>  // mmap(), munmap()
#include <sys/stat.h>  // fstat()
#include <fcntl.h>  // open()
#include <unistd.h>  // close(), read()

#define MAX_STRINGLEN 512  // Maximum length of a character string
#define MAX_PIDS 8192  // PID is 13 bits; 8191 = binary 1 1111 1111 1111
#define MAX_BUFFERLEN 65540  // Max required size of a PSI section or PES DVB subtitle segment buffer (for one object image)
#define MAX_PROGRAMS 65536  // Maximum number of Programs
#define MAX_STREAMS 65536  // Maximum number of Elementary Streams for a given Program
#define IMG_BUFFER_SIZE 3840 * 2160 * 4  // PAL DVD is 720x576, NTSC DVD is 720x480, but dvb.c will work for .ts files from HD (1920x1080) and UHD (3840x2160) BDs too; x 4 for RGBA
#define PES_LEN_UNBOUNDED (-1)
#define MAX_REGIONS 256  // Maximum number of regions within a page
#define MAX_OBJECTS 65536  // Maximum number of objects

// Packetized Elementary Stream (PES) properties
typedef struct {
  uint16_t elementary_stream_pid;
  uint8_t stream_type;
} PMT_STREAM;

// Program Map Table (PMT)
// Lists all PES PIDs and their Stream Types associated with a given Program Number.
typedef struct {
  uint8_t version;
  size_t nstreams;  // Number of Packetized Elementary Streams
  PMT_STREAM *stream;
} PMT;

// Program
typedef struct {
  uint16_t program_number;
  uint16_t pmt_pid;
  uint8_t have_pmt;  // Flag to indicate we have processed a PMT for this program, if it exists
  PMT pmt;  // We assume only one PMT per program; this is usually, but not always the case.
} PROGRAM;

// Program Association Table (PAT)
// Lists all Programs and their PMT PIDs.
typedef struct {
  uint8_t version;
  size_t nprograms;
  PROGRAM *program;
} PAT;

// Struct to hold a timestamp in hours, minutes, seconds, milliseconds,
// as well as equivalent expressed in total milliseconds.
typedef struct {
  int h;
  int m;
  int s;
  int ms;
  int64_t totalms;
} TIME;

// Flag identifying a Transport Stream (TS) packet as: Unknown Type, PSI, or PES.
typedef enum {
  PID_UNKNOWN = 0,
  PID_PSI,
  PID_PES
} PID_TYPE;

// Various data defining current state
typedef struct {
  size_t ts_index;
  uint8_t makebmp_flag;
  size_t npages;
  uint8_t have_pat;  // Flag to indicate we have processed PAT with latest version number
  PID_TYPE pid_type[MAX_PIDS];  // PID_UNKNOWN, PID_PSU, or PID_PES
  uint8_t pusi;  // Current Payload Unit Start Indicator
  uint16_t pid;  // Current PID
  uint16_t page_id;  // Current Page ID; we assume only 1 page_id per Display Set
  uint8_t region_id;  // Current Region ID
  uint16_t object_id;  // Current Object ID
  uint8_t stream_id;  // Current PES stream ID
  uint16_t display_width;  // Set by parse_dds()
  uint16_t display_height;  // Set by parse_dds()
  size_t nsubs;  // Count of subtitles in .ts file
  // Transport-level parameters:
  size_t section_bytecount[MAX_PIDS];  // Bytes added to a given PSI section buffer
  size_t previous_section_length[MAX_PIDS];  // Total length of previous PSI section
  size_t previous_section_bytecount[MAX_PIDS];  // Bytes added to the previous PSI section buffer
} STATE;

// Important parameters from MPEG-2 packetized elementary stream (PES)
typedef struct {
  size_t packet_length;
  size_t hdr_data_len;
  uint8_t collecting;  // Flag to indicate PES packets being collected for a segment
  ssize_t total_length;  // PES length: -1 = unknown, 0 = unbounded, > 0 means bounded (includes: Start Code (3 bytes), Stream ID (1 byte), PES_packet_length (2 bytes))
  TIME pts;
  TIME dts;
} PES;

// PSI section buffer
// Max required size of a PSI section is 1021 bytes.
typedef struct {
  size_t length;  // Length of a given PSI section buffer
  size_t bytecount;  // Bytes added to a given PSI section buffer
  uint8_t *buffer;
} SECTION;

// PES segment buffer
typedef struct {
  size_t length;  // Length of a given PES segment buffer
  uint8_t *buffer;  // Add 1 padding byte to prevent overflow in get_8bits() function.
} SEGMENT;

// One pixel's color and Alpha
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} RGBA;

// CLUT family; identified by clut_id
// One CLUT family = (2-bit, 4-bit, and 8-bit CLUTs representing the same palette)
typedef struct {
  uint8_t clut_id;
  uint8_t version;
  RGBA clut2[4];  // 2-bit CLUT
  RGBA clut4[16];  // 4-bit CLUT
  RGBA clut8[256];  // 8-bit CLUT
  uint8_t state2;  // 'c' = customized CLUT contents, 'd' = default CLUT contents
  uint8_t state4;  // 'c' = customized CLUT contents, 'd' = default CLUT contents
  uint8_t state8;  // 'c' = customized CLUT contents, 'd' = default CLUT contents
} CLUT_FAMILY;

// Object
typedef struct {
  uint16_t page_id;  // Object is specific to a page_id.
  uint16_t object_id;
  uint8_t version;
  size_t width;
  size_t height;
  uint8_t *buffer;  // Image buffer for object decoded from RLE data to object w, object h, array of CLUT entry values
  uint8_t non_modifying_colour_flag;
} OBJECT;

// Object Positions within Region
typedef struct {
  uint16_t object_id;
  uint8_t object_type;
  uint16_t horizontal_position;  // Object Horizontal Position Within Region
  uint16_t vertical_position;  // Object Vertical Position Within Region
  uint8_t foreground_color;
  uint8_t background_color;
} OBJECT_POS;

// Region
typedef struct {
  uint16_t page_id;  // Region is specific to a page_id.
  uint8_t region_id;
  uint8_t version;
  uint8_t fill_flag;
  uint16_t width;
  uint16_t height;
  uint8_t region_level_of_compatibility;
  uint8_t depth;
  uint8_t clut_id;
  uint8_t pixel_code_8bit;
  uint8_t pixel_code_4bit;
  uint8_t pixel_code_2bit;
  size_t nobjects;
  OBJECT_POS object_pos[MAX_OBJECTS];  // Positions of objects within a region.
} REGION;

// Region Position on Page
// Defined in parse_pcs(). These are the regions to be displayed.
typedef struct {
  uint8_t region_id;
  uint16_t region_horizontal_address;  // px from left of page
  uint16_t region_vertical_address;  // px from top of page
} REGION_POS;

// Page
typedef struct {
  uint16_t page_id;
  uint8_t version;
  uint8_t complete;  // Flag indicating Display Set (and in our case, page) is complete and ready for rendering
  uint8_t time_out;
  size_t nobjects;
  OBJECT *object;
  size_t ncluts;  // Number of CLUT families
  CLUT_FAMILY *clut;  // Array of CLUT families, where one family is 2-bit, 4-bit, & 8-bit CLUTs representing same palette
  size_t nregion_pos;  // Number of region positions defined for page[page_id]
  REGION_POS *region_pos;  // Positions of each region within page[page_id]. These are the regions to be displayed.
  size_t nregions;  // Number of regions defined for page[page_id]
  REGION *region;  // Regions within page[page_id]. These are the defined regions, but none, some, or all may be displayed. See region_pos array.
  size_t width;  // Width of final page composition (px)
  size_t height;  // Height of final page composition (px)
  uint8_t *buffer;  // Final page composition as RGBA.
  TIME start;
  TIME end;
} PAGE;

// Results from decoding runlength-encoding (RLE) pixel data
typedef struct {
  uint8_t color;
  size_t runlength;
  uint8_t end_of_string_signal;
  uint8_t emit_two_00_pixels;
  uint8_t emit_one_00_pixel;
} RLE;

// Function prototypes
int parse_ts_packet (STATE *, PAGE **, PAT *, uint8_t *, size_t, size_t *, PES *, SECTION *, SEGMENT *, FILE *);
int parse_adapt_field (STATE *, size_t *, uint8_t *, size_t, FILE *);
int build_psi_section (STATE *, PAT *, uint8_t *, size_t, int, SECTION *, FILE *);
int build_pes_segment (STATE *, PAGE **, uint8_t *, size_t, int, SEGMENT *, PES *, FILE *);
int parse_psi_section (STATE *, PAT *, SECTION *, FILE *);
int parse_pes_header (STATE *, PAGE **, size_t *, SEGMENT *, PES *, FILE *);
int parse_pes_segment (STATE *, PAGE **, SEGMENT *, PES *, FILE *);
int parse_sdt (STATE *, SECTION *, FILE *);
int parse_pat (STATE *, PAT *, SECTION *, FILE *);
int parse_pmt (STATE *, PAT *, SECTION *, FILE *);
int parse_pcs (STATE *, PAGE **, size_t *, SEGMENT *, PES *, FILE *);
void finalize_page_if_needed (STATE *, PAGE *, size_t, PES *);
int parse_rcs (STATE *, PAGE **, size_t *, SEGMENT *, FILE *);
int parse_cds (STATE *, PAGE **, size_t *, SEGMENT *, FILE *);
int parse_ods (STATE *, PAGE **, size_t *, SEGMENT *, FILE *);
int parse_dds (STATE *, PAGE **, size_t *, SEGMENT *, FILE *);
int parse_dss (STATE *, PAGE **, size_t *, SEGMENT *, FILE *);
int disparity_shift_update_sequence (STATE *, size_t *, size_t *, SEGMENT *, FILE *);
int parse_end (STATE *, PAGE **, size_t *, SEGMENT *, FILE *);
size_t parse_two_bit_code_string (STATE *, SEGMENT *, size_t *, RLE *);
size_t parse_four_bit_code_string (STATE *, SEGMENT *, size_t *, RLE *);
size_t parse_eight_bit_code_string (STATE *, SEGMENT *, size_t *, RLE *);
int find_page_index (STATE *, PAGE *, uint16_t);
int find_region_index (STATE *, PAGE *, uint8_t);
int find_object_index (STATE *, PAGE *, uint16_t);
int find_clut_index (STATE *, PAGE *, uint8_t);
int find_program_by_pmt_pid (PAT *, uint16_t);
int initialize_clut_family (STATE *, PAGE *, size_t);
RGBA default_2clut (uint8_t);
RGBA default_4clut (uint8_t);
RGBA default_8clut (uint8_t);
uint8_t mask_entry (uint8_t, uint8_t);
RGBA resolve_clut_color (CLUT_FAMILY *, uint8_t, uint8_t);
uint8_t reduce_8to4 (uint8_t);
uint8_t reduce_8to2 (uint8_t);
uint8_t reduce_4to2 (uint8_t);
uint8_t map_2to4 (uint8_t);
uint8_t map_2to8 (uint8_t);
uint8_t map_4to8 (uint8_t);
int get_8bits (STATE *, SEGMENT *, size_t *, uint8_t *);
int emit_pixels (STATE *, PAGE **, RLE *, size_t *, size_t);
int YCbCr2RGB_bt601 (uint8_t, int, int, int, int *);
int stream_types (STATE *, uint8_t, FILE *);  // PES Stream Type Assignments - ISO/IEC 13818-1 (Table 2-34)
int stream_ids (STATE *, FILE *);  // PES stream ID assignments - ISO/IEC 13818-1 (Table 2-22)
int data_ids (STATE *, uint8_t, FILE *);  // Data Identifiers for DVB Transport Streams - ETSI EN 301 192 (Table 2)
int segment_types (STATE *, uint8_t, FILE *fo);
int mstotime (TIME *);
int assemble_composition (STATE *, PAGE **);
int write_bmp (STATE *, PAGE *, uint8_t *);
void write_u16_le (FILE *, uint16_t);
void write_u32_le (FILE *, uint32_t);
void write_s32_le (FILE *, int32_t);
void clear_page (STATE *, PAGE *);
uint8_t *allocate_u8mem (int);
char *allocate_strmem (int);
size_t *allocate_sizemem (int);
PROGRAM *allocate_progmem (int);
SECTION *allocate_sectionmem (int);
SEGMENT *allocate_segmentmem (int);
