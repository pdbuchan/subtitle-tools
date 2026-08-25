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

#include "teletext.h"

// Valid Hamming 8/4 code words for data nibbles 0x0 through 0xf. Teletext
// protects packet addresses and page-header fields with this code. A one-bit
// error is correctable; two or more bit differences are rejected.
static const uint8_t hamming84_code[16] = {
  0x15, 0x02, 0x49, 0x5e, 0x64, 0x73, 0x38, 0x2f,
  0xd0, 0xc7, 0x8c, 0x9b, 0xa1, 0xb6, 0xfd, 0xea
};

// Reverse the bit order of one byte. EN 300 472 stores each of the 42 EBU
// Teletext packet bytes in the opposite bit order from the logical Teletext
// representation used by EN 300 706 decoders.
static uint8_t
reverse_bits (uint8_t value) {

  value = (uint8_t) (((value & 0x55) << 1) | ((value >> 1) & 0x55));
  value = (uint8_t) (((value & 0x33) << 2) | ((value >> 2) & 0x33));
  return ((uint8_t) ((value << 4) | (value >> 4)));
}

static unsigned int
bit_count (uint8_t value) {

  unsigned int n;

  n = 0;
  while (value) {
    n += value & 1;
    value >>= 1;
  }
  return (n);
}

// Decode one Hamming 8/4 byte by selecting a valid code word at Hamming
// distance zero or one. corrected is set only when one bad bit was repaired.
static int
hamming84_decode (uint8_t value, uint8_t *nibble, uint8_t *corrected) {

  unsigned int i, distance, best_distance;
  uint8_t best;

  best = 0;
  best_distance = 9;
  for (i = 0; i < 16; i++) {
    distance = bit_count ((uint8_t) (value ^ hamming84_code[i]));
    if (distance < best_distance) {
      best_distance = distance;
      best = (uint8_t) i;
    }
  }

  if (best_distance > 1) return (EXIT_FAILURE);
  *nibble = best;
  *corrected = (uint8_t) (best_distance == 1);
  return (EXIT_SUCCESS);
}

// Teletext display characters have odd parity over all eight bits. Strip the
// parity bit after checking it. A bad character is represented by '?' so an
// extracted text file makes the damaged position obvious instead of silently
// accepting a corrupted character/control code.
static uint8_t
decode_character (TTX_CONTEXT *ttx, TTX_PAGE *page, uint8_t value) {

  if ((bit_count (value) & 1) == 0) {
    ttx->nparity_errors++;
    page->parity_errors++;
    return ('?');
  }

  return ((uint8_t) (value & 0x7f));
}

static int
active_slot (uint16_t pid, uint8_t magazine) {

  return ((int) ((size_t) pid * 8 + (magazine - 1)));
}

// Find the most recent snapshot of the same page/subpage. Returning an index
// rather than a pointer keeps the result valid if realloc() subsequently moves
// the page array while the new snapshot is appended.
static ssize_t
find_previous_page_index (const TTX_CONTEXT *ttx, uint16_t pid, uint8_t magazine, uint8_t page_number, uint16_t subcode) {

  size_t i;

  for (i = ttx->npages; i > 0; i--) {
    const TTX_PAGE *page = &ttx->page[i - 1];
    if (page->pid == pid && page->magazine == magazine &&
        page->page_number == page_number && page->subcode == subcode) {
      if (i - 1 > (size_t) INT_MAX) return (-1);
      return ((ssize_t) (i - 1));
    }
  }

  return (-1);
}

// Create one page-transmission snapshot. Keeping each transmission rather than
// only the latest state is essential for Teletext subtitles, where the same
// page number is repeatedly retransmitted with different caption text.
static TTX_PAGE *
new_page (TTX_CONTEXT *ttx, uint16_t pid, uint8_t data_unit_id, uint8_t magazine, uint8_t page_number, uint16_t subcode, uint8_t erase_page) {

  TTX_PAGE *tmp, *page;
  ssize_t previous_index;
  size_t n;

  // Locate the preceding state before reallocating. Its transmission number
  // gives the new ordinal without rescanning the entire history a second time.
  previous_index = find_previous_page_index (ttx, pid, magazine, page_number, subcode);

  n = ttx->npages + 1;
  if (n > SIZE_MAX / sizeof (*ttx->page)) return (NULL);
  tmp = realloc (ttx->page, n * sizeof (*ttx->page));
  if (!tmp) return (NULL);
  ttx->page = tmp;
  page = &ttx->page[ttx->npages];
  memset (page, 0, sizeof (*page));

  page->pid = pid;
  page->data_unit_id = data_unit_id;
  page->magazine = magazine;
  page->page_number = page_number;
  page->subcode = subcode;
  page->transmissions = previous_index >= 0 ? ttx->page[(size_t) previous_index].transmissions + 1 : 1;
  memset (page->row, ' ', sizeof (page->row));

  // With erase_page clear, the current transmission updates the receiver's
  // existing page memory rather than clearing it first. Seed the new snapshot
  // from the last state of this same page/subpage, then overwrite received rows.
  if (!erase_page && previous_index >= 0) {
    const TTX_PAGE *previous = &ttx->page[(size_t) previous_index];
    memcpy (page->row_present, previous->row_present, sizeof (page->row_present));
    memcpy (page->row, previous->row, sizeof (page->row));
  }

  ttx->npages = n;
  return (page);
}

// A Teletext descriptor language applies to the Teletext service, while its
// page number identifies a particular advertised page (for example the initial
// page or a subtitle page). If every descriptor entry on this PID names the
// same language, that language is also a useful unambiguous fallback for pages
// which are carried in the service but are not individually listed in the PMT.
static const char *
find_unambiguous_stream_language (PAT *pat, uint16_t pid) {

  const PMT_STREAM *stream;
  const char *language;
  size_t i;

  stream = find_pmt_stream_by_pid (pat, pid);
  if (!stream || stream->nteletext_services == 0) return (NULL);

  language = stream->teletext_service[0].language;
  for (i = 1; i < stream->nteletext_services; i++) {
    if (strcmp (language, stream->teletext_service[i].language) != 0) {
      return (NULL);
    }
  }
  return (language);
}

static int
decode_header_nibble (TTX_CONTEXT *ttx, uint8_t value, uint8_t *nibble, const char *name, FILE *fo) {

  uint8_t corrected;

  if (hamming84_decode (value, nibble, &corrected) != EXIT_SUCCESS) {
    ttx->nhamming_errors++;
    fprintf (fo, "    Uncorrectable Hamming 8/4 error in %s (0x%02x).\n", name, value);
    return (EXIT_FAILURE);
  }

  if (corrected) {
    ttx->nhamming_corrected++;
    fprintf (fo, "    Corrected one-bit Hamming 8/4 error in %s.\n", name);
  }
  return (EXIT_SUCCESS);
}

// Parse one 44-byte EBU Teletext data-unit payload from a DVB PES packet.
// Reference: ETSI EN 300 472 for the data-unit wrapper and ETSI EN 300 706 for
// packet addressing, page headers, Hamming coding, parity, and display rows.
int
parse_teletext_data_unit (STATE *state, TTX_CONTEXT *ttx, PAT *pat, uint8_t data_unit_id, const uint8_t *data, size_t length, const PES *pes, FILE *fo) {

  uint8_t packet[42], address0, address1, corrected;
  uint8_t magazine, packet_number, field_parity, line_offset;
  uint8_t page_units, page_tens, s1, s2c4, s3, s4c56, c7c10, c11c14;
  uint8_t page_number, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14;
  uint8_t s2, s4, national_option;
  uint16_t subcode, pid;
  size_t i;
  int slot, page_index;
  TTX_PAGE *page;
  const TELETEXT_SERVICE *service;
  const char *stream_language;

  if (length != 44) return (EXIT_FAILURE);
  pid = state->pid;

  // EN 300 472 reserves the two most-significant bits of this byte as '11'.
  // Line offsets 7 through 22 identify usable VBI lines; zero is also defined
  // for data units for which no corresponding VBI line is specified.
  if ((data[0] & 0xc0) != 0xc0) {
    fprintf (fo, "    WARNING: reserved bits in field/line byte are not '11'.\n");
  }

  field_parity = (data[0] >> 5) & 1;
  line_offset = data[0] & 0x1f;
  fprintf (fo, "    Reserved/field/line byte: 0x%02x (field parity %u, line offset %u)\n", data[0], field_parity, line_offset);
  if (line_offset != 0 && (line_offset < 7 || line_offset > 22)) {
    fprintf (fo, "    WARNING: Teletext line offset is outside 7-22 (or zero).\n");
  }
  fprintf (fo, "    Framing code: 0x%02x\n", data[1]);

  if (data[1] != 0xe4) {
    fprintf (fo, "    Unexpected framing code; Teletext packet ignored.\n");
    return (EXIT_SUCCESS);
  }

  // Convert the DVB byte representation to logical EBU Teletext bytes.
  for (i = 0; i < 42; i++) packet[i] = reverse_bits (data[i + 2]);

  if (hamming84_decode (packet[0], &address0, &corrected) != EXIT_SUCCESS) {
    ttx->nhamming_errors++;
    fprintf (fo, "    Uncorrectable Hamming error in packet-address byte 0.\n");
    return (EXIT_SUCCESS);
  }
  if (corrected) ttx->nhamming_corrected++;

  if (hamming84_decode (packet[1], &address1, &corrected) != EXIT_SUCCESS) {
    ttx->nhamming_errors++;
    fprintf (fo, "    Uncorrectable Hamming error in packet-address byte 1.\n");
    return (EXIT_SUCCESS);
  }
  if (corrected) ttx->nhamming_corrected++;

  magazine = address0 & 7;
  if (magazine == 0) magazine = 8;
  packet_number = (uint8_t) (((address0 >> 3) & 1) | ((address1 & 0x0f) << 1));
  fprintf (fo, "    Magazine: %u\n", magazine);
  fprintf (fo, "    Packet number: %u\n", packet_number);

  // Packet X/0 starts a new page transmission and carries the page/subpage
  // address, receiver-control bits, national-option selection, and 32 display
  // characters occupying columns 8 through 39 of row zero.
  if (packet_number == 0) {
    if (decode_header_nibble (ttx, packet[2], &page_units, "page units", fo) != EXIT_SUCCESS ||
        decode_header_nibble (ttx, packet[3], &page_tens, "page tens", fo) != EXIT_SUCCESS ||
        decode_header_nibble (ttx, packet[4], &s1, "subcode S1", fo) != EXIT_SUCCESS ||
        decode_header_nibble (ttx, packet[5], &s2c4, "subcode S2/C4", fo) != EXIT_SUCCESS ||
        decode_header_nibble (ttx, packet[6], &s3, "subcode S3", fo) != EXIT_SUCCESS ||
        decode_header_nibble (ttx, packet[7], &s4c56, "subcode S4/C5/C6", fo) != EXIT_SUCCESS ||
        decode_header_nibble (ttx, packet[8], &c7c10, "C7-C10", fo) != EXIT_SUCCESS ||
        decode_header_nibble (ttx, packet[9], &c11c14, "C11-C14", fo) != EXIT_SUCCESS) {
      return (EXIT_SUCCESS);
    }

    page_number = (uint8_t) ((page_tens << 4) | page_units);
    s2 = s2c4 & 7;
    c4 = (s2c4 >> 3) & 1;
    s4 = s4c56 & 3;
    c5 = (s4c56 >> 2) & 1;
    c6 = (s4c56 >> 3) & 1;
    c7 = c7c10 & 1;
    c8 = (c7c10 >> 1) & 1;
    c9 = (c7c10 >> 2) & 1;
    c10 = (c7c10 >> 3) & 1;
    c11 = c11c14 & 1;
    c12 = (c11c14 >> 1) & 1;
    c13 = (c11c14 >> 2) & 1;
    c14 = (c11c14 >> 3) & 1;
    subcode = (uint16_t) ((s4 << 12) | (s3 << 8) | (s2 << 4) | s1);
    national_option = (uint8_t) ((c14 << 2) | (c13 << 1) | c12);

    fprintf (fo, "    Page: %u%02X\n", magazine, page_number);
    fprintf (fo, "    Subcode: 0x%04X\n", subcode);
    fprintf (fo, "    C4 erase page: %u\n", c4);
    fprintf (fo, "    C5 newsflash: %u\n", c5);
    fprintf (fo, "    C6 subtitle: %u\n", c6);
    fprintf (fo, "    C7 suppress header: %u\n", c7);
    fprintf (fo, "    C8 update indicator: %u\n", c8);
    fprintf (fo, "    C9 interrupted sequence: %u\n", c9);
    fprintf (fo, "    C10 inhibit display: %u\n", c10);
    fprintf (fo, "    C11 magazine serial mode: %u\n", c11);
    fprintf (fo, "    C12-C14 national option: %u\n", national_option);

    // Page number FF is a time-filling/null header and is not a displayable
    // page to extract. It still appears in the analyzer report above.
    if (page_number == 0xff) {
      if (c11) {
        for (i = 0; i < 8; i++) ttx->active_page[(size_t) pid * 8 + i] = -1;
      } else {
        ttx->active_page[active_slot (pid, magazine)] = -1;
      }
      return (EXIT_SUCCESS);
    }

    // In serial mode any new page header terminates the preceding page on this
    // PID. In parallel mode only the current magazine changes its active page.
    if (c11) {
      for (i = 0; i < 8; i++) ttx->active_page[(size_t) pid * 8 + i] = -1;
    }

    page = new_page (ttx, pid, data_unit_id, magazine, page_number, subcode, c4);
    if (!page) {
      fprintf (stderr, "Unable to allocate Teletext page.\n");
      return (EXIT_FAILURE);
    }
    page_index = (int) (ttx->npages - 1);
    page->erase_page = c4;
    page->newsflash = c5;
    page->subtitle = c6;
    page->suppress_header = c7;
    page->update_indicator = c8;
    page->interrupted_sequence = c9;
    page->inhibit_display = c10;
    page->magazine_serial = c11;
    page->national_option = national_option;

    service = find_teletext_service (pat, pid, magazine, page_number);
    if (service) {
      memcpy (page->language, service->language, sizeof (page->language));
      page->teletext_type = service->teletext_type;
      fprintf (fo, "    PMT Teletext service: language %s, type 0x%02x\n", page->language, page->teletext_type);
    } else {
      stream_language = find_unambiguous_stream_language (pat, pid);
      if (stream_language) {
        memcpy (page->language, stream_language, sizeof (page->language));
        fprintf (fo, "    PMT Teletext language fallback: %s (page not individually advertised)\n", page->language);
      } else {
        memcpy (page->language, "und", 4);
      }
    }

    if (pes->have_pts) {
      page->first_pts = pes->pts;
      page->last_pts = pes->pts;
      page->have_first_pts = 1;
      page->have_last_pts = 1;
    }

    // Header display text contains only 32 characters; the first eight screen
    // columns are receiver-generated/header-address positions.
    memset (page->row[0], ' ', TELETEXT_COLUMNS);
    for (i = 0; i < 32; i++) {
      page->row[0][i + 8] = decode_character (ttx, page, packet[i + 10]);
    }
    page->row_present[0] = 1;

    slot = active_slot (pid, magazine);
    ttx->active_page[slot] = page_index;
    return (EXIT_SUCCESS);
  }

  // Packets X/1 through X/24 carry the normal 40-column page rows of the
  // currently active page. Together with X/0, these are the 25 Level 1 rows
  // represented by this plain-text extractor.
  if (packet_number >= 1 && packet_number <= 24) {
    slot = active_slot (pid, magazine);
    page_index = ttx->active_page[slot];
    if (page_index < 0 || (size_t) page_index >= ttx->npages) {
      fprintf (fo, "    No active page for this magazine; row ignored.\n");
      return (EXIT_SUCCESS);
    }

    page = &ttx->page[page_index];
    for (i = 0; i < TELETEXT_COLUMNS; i++) {
      page->row[packet_number][i] = decode_character (ttx, page, packet[i + 2]);
    }
    page->row_present[packet_number] = 1;
    if (pes->have_pts) {
      page->last_pts = pes->pts;
      page->have_last_pts = 1;
    }
    return (EXIT_SUCCESS);
  }

  if (packet_number >= 25 && packet_number <= 31) {
    // X/25 is outside the standard 25-row Level 1 display area, while X/26
    // through X/31 carry enhancement and other non-row data. Their coding and
    // application can differ from ordinary display rows, so preserve the raw
    // logical bytes in the report instead of inventing a plain-text rendering.
    fprintf (fo, "    Packet X/%u retained in report only (not rendered as a Level 1 row).\n", packet_number);
    fprintf (fo, "    Logical data bytes: ");
    for (i = 2; i < 42; i++) {
      fprintf (fo, "%02x%s", packet[i], i == 41 ? "\n" : " ");
    }
  } else {
    fprintf (fo, "    Invalid Teletext packet number %u.\n", packet_number);
  }

  return (EXIT_SUCCESS);
}
