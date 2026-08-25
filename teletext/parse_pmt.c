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

// Return a readable description for the five Teletext service types defined
// for the DVB Teletext descriptor. Unknown values are retained by the parser
// but described as reserved rather than treated as an error.
static const char *
teletext_type_name (uint8_t type) {

  switch (type) {
    case 0x01: return ("initial Teletext page");
    case 0x02: return ("Teletext subtitle page");
    case 0x03: return ("additional information page");
    case 0x04: return ("programme schedule page");
    case 0x05: return ("Teletext subtitle page for hearing impaired people");
    default: return ("reserved for future use");
  }
}

// Parse one ES descriptor loop and retain every DVB Teletext descriptor entry.
// A descriptor_tag of 0x56 identifies a stream carrying EBU Teletext data.
static int
parse_es_descriptors (PMT_STREAM *stream, const uint8_t *buffer, size_t offset, size_t length, FILE *fo) {

  size_t end, desc_end, i;
  uint8_t tag, desc_len, b;
  TELETEXT_SERVICE *service;

  // The caller supplies exactly the ES_info descriptor-loop extent. Keep all
  // descriptor bounds relative to that end point so a malformed descriptor
  // cannot consume bytes belonging to the following elementary-stream entry.
  end = offset + length;
  while (offset < end) {

    // Every descriptor begins with descriptor_tag and descriptor_length.
    if (!bytes_available (offset, 2, end)) return (EXIT_FAILURE);
    tag = buffer[offset++];
    desc_len = buffer[offset++];

    // descriptor_length covers only the bytes following the two-byte header.
    if (!bytes_available (offset, desc_len, end)) return (EXIT_FAILURE);
    desc_end = offset + desc_len;

    fprintf (fo, "    Descriptor tag 0x%02x, length %u", tag, desc_len);
    if (tag == 0x56) fprintf (fo, " (Teletext descriptor)");
    fprintf (fo, "\n");

    if (tag == 0x56) {
      // A DVB Teletext descriptor consists of one or more fixed five-byte
      // service entries. Anything else would leave a partial entry at the end.
      if ((desc_len % 5) != 0) {
        fprintf (stderr, "Teletext descriptor length is not a multiple of five.\n");
        return (EXIT_FAILURE);
      }

      // Mark the elementary stream as Teletext as soon as a valid 0x56
      // descriptor is found. A descriptor may advertise several languages or
      // page types, so retain every entry rather than only the first one.
      stream->is_teletext = 1;
      for (i = 0; i < (size_t) desc_len / 5; i++) {
        if (stream->nteletext_services >= MAX_TELETEXT_SERVICES) {
          fprintf (stderr, "Too many Teletext descriptor entries.\n");
          return (EXIT_FAILURE);
        }

        // Five-byte Teletext descriptor entry:
        //   bytes 0..2  ISO 639 language code
        //   byte  3     teletext_type (upper 5 bits), magazine (lower 3 bits)
        //   byte  4     page number
        service = &stream->teletext_service[stream->nteletext_services++];
        service->language[0] = (char) buffer[offset];
        service->language[1] = (char) buffer[offset + 1];
        service->language[2] = (char) buffer[offset + 2];
        service->language[3] = '\0';
        b = buffer[offset + 3];
        service->teletext_type = b >> 3;
        service->magazine = b & 7;
        service->page_number = buffer[offset + 4];

        // In the Teletext numbering convention a three-bit magazine value of
        // zero denotes magazine 8, which is why the stored zero is displayed
        // as 8 in the report.
        fprintf (fo, "      Language: %s Type: 0x%02x (%s) Magazine: %u Page: %02X\n", service->language, service->teletext_type, teletext_type_name (service->teletext_type), service->magazine ? service->magazine : 8, service->page_number);
        offset += 5;
      }
    }

    // For unrecognized descriptors, or after parsing a recognized one, move
    // to the boundary declared by descriptor_length.
    offset = desc_end;
  }

  return (EXIT_SUCCESS);
}

// Parse a Program Map Table (PMT), retaining only enough descriptor detail to
// identify DVB Teletext elementary streams and their advertised pages.
int
parse_pmt (STATE *state, PAT *pat, SECTION *section, FILE *fo) {

  int program_index;
  size_t offset, section_length, total, entries_end, program_info_length;
  size_t es_info_length, nstreams, capacity, i, pidx, descriptor_offset;
  uint8_t table_id, ssi, version, current, section_number, last_section;
  uint8_t stream_type;
  uint16_t pid, program_number, pcr_pid, elementary_pid;
  PMT_STREAM *list, *tmp;

  // The PMT section is stored under the PID currently being processed.
  pid = state->pid;
  offset = 0;
  nstreams = 0;
  capacity = 0;
  list = NULL;

  fprintf (fo, "Program Map Table (PMT):\n");

  // Read the three-byte PSI section prefix. For a PMT, table_id must be 0x02
  // and section_syntax_indicator must be set.
  if (!bytes_available (offset, 3, section[pid].length)) return (EXIT_FAILURE);
  table_id = section[pid].buffer[offset++];
  ssi = (section[pid].buffer[offset] >> 7) & 1;
  section_length = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) | section[pid].buffer[offset + 1]);
  offset += 2;

  // section_length starts immediately after its own field and includes the
  // four-byte CRC. Validate both the legal PMT size and the reassembled buffer
  // extent before using any offsets derived from it.
  if (table_id != 0x02 || ssi != 1 || section_length < 13 || section_length > 1021 || !bytes_available (0, section_length + 3, section[pid].length)) {
    return (EXIT_FAILURE);
  }
  // total is the complete PMT section size including table_id and the two-byte
  // section-length field. entries_end points at the first byte of the CRC, so
  // all PMT syntax and descriptor parsing must stop before that position.
  total = section_length + 3;
  entries_end = total - 4;

  // Parse the fixed PMT fields through program_info_length.
  if (!bytes_available (offset, 9, entries_end)) return (EXIT_FAILURE);
  // program_number links this PMT back to the corresponding PAT program.
  program_number = (uint16_t) (((uint16_t) section[pid].buffer[offset] << 8) | section[pid].buffer[offset + 1]);
  offset += 2;

  // The next byte carries reserved bits, version_number, and current_next.
  version = (section[pid].buffer[offset] >> 1) & 0x1f;
  current = section[pid].buffer[offset++] & 1;
  section_number = section[pid].buffer[offset++];
  last_section = section[pid].buffer[offset++];
  // PID fields use 13 significant bits; the upper three bits are reserved.
  pcr_pid = (uint16_t) (((section[pid].buffer[offset] & 0x1f) << 8) | section[pid].buffer[offset + 1]);
  offset += 2;

  // program_info_length uses the low 12 bits; the high four bits are reserved.
  program_info_length = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) | section[pid].buffer[offset + 1]);
  offset += 2;

  // This implementation keeps one complete PMT per program and therefore does
  // not merge PMT sections spread across section_number values.
  if (last_section != 0 || section_number != 0) {
    fprintf (stderr, "Multi-section PMTs are not supported.\n");
    return (EXIT_FAILURE);
  }
  // Program-level descriptors precede the elementary-stream loop. None are
  // needed for Teletext discovery, but their declared extent must be valid and
  // must be skipped before reading the first ES entry.
  if (!bytes_available (offset, program_info_length, entries_end)) return (EXIT_FAILURE);
  offset += program_info_length;

  fprintf (fo, "  Program: 0x%04x Version: %u Current: %u Section: %u/%u PCR PID: 0x%04x\n", program_number, version, current, section_number, last_section, pcr_pid);

  // Each elementary-stream entry has a five-byte fixed header followed by an
  // ES_info descriptor loop of es_info_length bytes.
  while (offset < entries_end) {
    if (!bytes_available (offset, 5, entries_end)) {
      free (list);
      return (EXIT_FAILURE);
    }

    // stream_type identifies the coding/carriage class. DVB Teletext normally
    // appears as stream_type 0x06 and is distinguished from other private data
    // by its Teletext descriptor (tag 0x56).
    stream_type = section[pid].buffer[offset++];

    // elementary_PID is another 13-bit PID field.
    elementary_pid = (uint16_t) (((section[pid].buffer[offset] & 0x1f) << 8) | section[pid].buffer[offset + 1]);
    offset += 2;

    // ES_info_length gives the byte count of descriptors for this ES entry.
    es_info_length = (size_t) (((section[pid].buffer[offset] & 0x0f) << 8) | section[pid].buffer[offset + 1]);
    offset += 2;
    if (!bytes_available (offset, es_info_length, entries_end)) {
      free (list);
      return (EXIT_FAILURE);
    }

    // Build a temporary PMT stream list while parsing. Grow geometrically to
    // avoid reallocating for every ES entry, but never exceed MAX_STREAMS.
    if (nstreams == capacity) {
      size_t new_capacity = capacity ? capacity * 2 : 8;
      if (new_capacity > MAX_STREAMS) new_capacity = MAX_STREAMS;
      if (new_capacity <= capacity) {
        free (list);
        return (EXIT_FAILURE);
      }
      tmp = realloc (list, new_capacity * sizeof (*list));
      if (!tmp) {
        free (list);
        return (EXIT_FAILURE);
      }
      list = tmp;
      capacity = new_capacity;
    }

    // Zero the new record so descriptor-derived flags and counters start in a
    // known state before parse_es_descriptors() fills them.
    memset (&list[nstreams], 0, sizeof (list[nstreams]));
    list[nstreams].elementary_stream_pid = elementary_pid;
    list[nstreams].stream_type = stream_type;

    fprintf (fo, "  Elementary Stream %zu PID 0x%04x type 0x%02x\n", nstreams, elementary_pid, stream_type);
    stream_types (state, stream_type, fo);

    // Parse descriptors without moving the main PMT offset. The caller advances
    // by the complete ES_info_length after the descriptor parser returns.
    descriptor_offset = offset;
    if (parse_es_descriptors (&list[nstreams], section[pid].buffer, descriptor_offset, es_info_length, fo) != EXIT_SUCCESS) {
      free (list);
      return (EXIT_FAILURE);
    }
    offset += es_info_length;
    nstreams++;
  }

  // entries_end was defined as the first CRC byte above, so these four bytes
  // are reported separately from the elementary-stream loop.
  fprintf (fo, "  CRC: %02x%02x%02x%02x\n", section[pid].buffer[entries_end], section[pid].buffer[entries_end + 1], section[pid].buffer[entries_end + 2], section[pid].buffer[entries_end + 3]);

  // Associate this PMT PID with the program previously discovered in the PAT.
  // A syntactically valid PMT for an unknown program is reportable, but there
  // is no PAT-owned state in which to install it.
  program_index = find_program_by_pmt_pid (pat, pid);
  if (program_index < 0) {
    free (list);
    return (EXIT_SUCCESS);
  }
  pidx = (size_t) program_index;

  // The PMT's program_number must agree with the PAT entry that assigned this
  // PMT PID. A mismatch indicates inconsistent PSI signalling.
  if (program_number != pat->program[pidx].program_number) {
    free (list);
    return (EXIT_FAILURE);
  }

  // Ignore a PMT marked not-current, and avoid replacing the installed PMT
  // when this is merely another copy of the version already in use.
  if (!current || (pat->program[pidx].have_pmt && pat->program[pidx].pmt.version == version)) {
    free (list);
    return (EXIT_SUCCESS);
  }

  // A new PMT version supersedes the previous ES list. First remove the old
  // PID classifications so streams deleted or repurposed by the new PMT do not
  // remain active accidentally.
  for (i = 0; i < pat->program[pidx].pmt.nstreams; i++) {
    elementary_pid = pat->program[pidx].pmt.stream[i].elementary_stream_pid;
    state->pid_type[elementary_pid] = PID_UNKNOWN;
  }
  free (pat->program[pidx].pmt.stream);

  // Transfer ownership of the newly parsed list into the PAT program record.
  // From this point list must not be freed locally.
  pat->program[pidx].pmt.stream = list;
  pat->program[pidx].pmt.nstreams = nstreams;
  pat->program[pidx].pmt.version = version;
  pat->program[pidx].have_pmt = 1;

  // Only PES streams explicitly identified by descriptor_tag 0x56 are sent to
  // the Teletext PES parser. This prevents video/audio/private PES packets from
  // filling the Teletext reassembly buffers merely because they occur in PMT.
  state->pid_type[pid] = PID_PSI;
  for (i = 0; i < nstreams; i++) {
    if (list[i].stream_type == 0x06 && list[i].is_teletext) {
      state->pid_type[list[i].elementary_stream_pid] = PID_PES;
    } else {
      state->pid_type[list[i].elementary_stream_pid] = PID_UNKNOWN;
    }
  }

  return (EXIT_SUCCESS);
}
