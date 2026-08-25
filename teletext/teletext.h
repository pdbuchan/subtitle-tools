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

#ifndef TELETEXT_H
#define TELETEXT_H

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_STRINGLEN 512
#define MAX_PIDS 8192
#define MAX_BUFFERLEN 65541
#define MAX_PROGRAMS 65536
#define MAX_STREAMS 65536
#define MAX_TELETEXT_SERVICES 51  // descriptor_length is one byte; each entry is 5 bytes.
#define TELETEXT_ROWS 25
#define TELETEXT_COLUMNS 40
#define PES_LEN_UNBOUNDED (-1)

// One five-byte entry from the DVB Teletext descriptor (descriptor_tag 0x56).
typedef struct {
  char language[4];
  uint8_t teletext_type;
  uint8_t magazine;
  uint8_t page_number;
} TELETEXT_SERVICE;

// Packetized Elementary Stream (PES) properties recorded from one PMT ES loop.
typedef struct {
  uint16_t elementary_stream_pid;
  uint8_t stream_type;
  uint8_t is_teletext;
  size_t nteletext_services;
  TELETEXT_SERVICE teletext_service[MAX_TELETEXT_SERVICES];
} PMT_STREAM;

typedef struct {
  uint8_t version;
  size_t nstreams;
  PMT_STREAM *stream;
} PMT;

typedef struct {
  uint16_t program_number;
  uint16_t pmt_pid;
  uint8_t have_pmt;
  PMT pmt;
} PROGRAM;

typedef struct {
  uint8_t version;
  size_t nprograms;
  PROGRAM *program;
} PAT;

// Timestamp as both split fields and total milliseconds.
typedef struct {
  int h;
  int m;
  int s;
  int ms;
  int64_t totalms;
} TIME;

typedef enum {
  PID_UNKNOWN = 0,
  PID_PSI,
  PID_PES
} PID_TYPE;

// Important parameters from one reassembled MPEG-2 PES packet.
typedef struct {
  size_t packet_length;
  size_t hdr_data_len;
  uint8_t collecting;
  ssize_t total_length;
  TIME pts;
  TIME dts;
  uint8_t have_pts;
  uint8_t have_dts;
  uint8_t data_alignment_indicator;
} PES;

typedef struct {
  size_t length;
  size_t bytecount;
  uint8_t *buffer;
} SECTION;

typedef struct {
  size_t length;
  uint8_t *buffer;
} SEGMENT;

// One assembled Level 1 Teletext page/subpage. We retain Teletext character
// codes rather than rendered UTF-8 so a character subset can be selected when
// the completed page is written.
typedef struct {
  uint16_t pid;
  uint8_t data_unit_id;
  uint8_t magazine;
  uint8_t page_number;
  uint16_t subcode;
  uint8_t erase_page;
  uint8_t newsflash;
  uint8_t subtitle;
  uint8_t suppress_header;
  uint8_t update_indicator;
  uint8_t interrupted_sequence;
  uint8_t inhibit_display;
  uint8_t magazine_serial;
  uint8_t national_option;
  char language[4];
  uint8_t teletext_type;
  uint8_t row_present[TELETEXT_ROWS];
  uint8_t row[TELETEXT_ROWS][TELETEXT_COLUMNS];
  size_t transmissions;
  size_t parity_errors;
  TIME first_pts;
  TIME last_pts;
  uint8_t have_first_pts;
  uint8_t have_last_pts;
} TTX_PAGE;

// Teletext assembly state. active_page is indexed by PID and magazine 1..8;
// each element stores an index into page[], or -1 when no page is active.
typedef struct {
  size_t npages;
  TTX_PAGE *page;
  int *active_page;
  size_t ndata_units;
  size_t nteletext_units;
  size_t nhamming_errors;
  size_t nhamming_corrected;
  size_t nparity_errors;
} TTX_CONTEXT;

// Transport-level state shared by the TS, PSI, and PES parsers.
typedef struct {
  size_t ts_index;
  uint8_t write_text_flag;
  uint8_t have_pat;
  PID_TYPE pid_type[MAX_PIDS];
  uint8_t pusi;
  uint16_t pid;
  uint8_t stream_id;
  size_t section_bytecount[MAX_PIDS];
  size_t previous_section_length[MAX_PIDS];
  size_t previous_section_bytecount[MAX_PIDS];
} STATE;

int parse_ts_packet (STATE *, TTX_CONTEXT *, PAT *, uint8_t *, size_t, size_t *, PES *, SECTION *, SEGMENT *, FILE *);
int parse_adapt_field (STATE *, size_t *, uint8_t *, size_t, FILE *);
int build_psi_section (STATE *, PAT *, uint8_t *, size_t, size_t, SECTION *, FILE *);
int build_pes_segment (STATE *, TTX_CONTEXT *, uint8_t *, size_t, size_t, SEGMENT *, PES *, PAT *, FILE *);
int parse_psi_section (STATE *, PAT *, SECTION *, FILE *);
int parse_pes_header (STATE *, size_t *, SEGMENT *, PES *, FILE *);
int parse_pes_segment (STATE *, TTX_CONTEXT *, PAT *, SEGMENT *, PES *, FILE *);
int parse_sdt (STATE *, SECTION *, FILE *);
int parse_pat (STATE *, PAT *, SECTION *, FILE *);
int parse_pmt (STATE *, PAT *, SECTION *, FILE *);
int parse_teletext_data_unit (STATE *, TTX_CONTEXT *, PAT *, uint8_t, const uint8_t *, size_t, const PES *, FILE *);
int write_teletext_pages (TTX_CONTEXT *, const char *);
int find_program_by_pmt_pid (PAT *, uint16_t);
const PMT_STREAM *find_pmt_stream_by_pid (PAT *, uint16_t);
const TELETEXT_SERVICE *find_teletext_service (PAT *, uint16_t, uint8_t, uint8_t);
int stream_types (STATE *, uint8_t, FILE *);
int stream_ids (STATE *, FILE *);
int data_ids (uint8_t, FILE *);
int mstotime (TIME *);
uint32_t mpeg2_crc32 (const uint8_t *, size_t);
uint8_t *allocate_u8mem (size_t);
PROGRAM *allocate_progmem (size_t);
SECTION *allocate_sectionmem (size_t);
SEGMENT *allocate_segmentmem (size_t);
PES *allocate_pesmem (size_t);

static inline int
bytes_available (size_t offset, size_t count, size_t length) {

  return ((offset <= length) && (count <= (length - offset)));
}

#endif
