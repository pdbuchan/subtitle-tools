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

#include "dvb.h"

// Parse current Transport Stream packet.
// Reference: ISO/IEC 13818-1
int
parse_ts_packet (STATE *state, PAGE **page, PAT *pat, uint8_t *tsdata, size_t tslen, size_t *packet, PES *pes, SECTION *section, SEGMENT *segment, FILE *fo) {

  size_t start, offset, ts_payloadlen, adapt_field_len, adapt_field_size;
  uint8_t value, tei, adapt_field_cntrl;

  start = state->ts_index;
  offset = start;
  adapt_field_len = 0;
  adapt_field_size = 0;
  ts_payloadlen = 0;

  if ((start + 188) > tslen) {
    fprintf (stderr, "Unexpected end of transport stream.\n");
    fprintf (stderr, "Starting address of current TS packet: 0x%012zx\n", start);
    fprintf (stderr, "...plus one packet length of 188 bytes: 0x%012zx\n", start + 188);
    fprintf (stderr, "Length of transport stream file: 0x%012zx\n", tslen);
    exit (EXIT_FAILURE);
  }

  fprintf(fo, "\nTransport Stream Packet %lu:\n", *packet);

  // Sync Byte (1 byte)
  if (tsdata[offset] != 0x47) {
    fprintf (stderr, "Found invalid sync byte of 0x%02x. Should be 0x47.\n", tsdata[offset]);
    fprintf (stderr, "Index of ts file is 0x%08zx (%zu)\n", start + offset, start + offset);
    exit (EXIT_FAILURE);
  }
  fprintf (fo, "  Sync Byte (1 byte): 0x%02x\n", tsdata[offset]);
  offset++;

  // TEI, PUSI, Priority, PID
  value = tsdata[offset];

  // Transport Error Indicator (1 bit)
  tei = (value >> 7) & 1;
  if (tei == 0) {
    fprintf (fo, "  Transport Error Indicator (1 bit): 0 (no error in transport stream packet)\n");
  } else {
    fprintf (fo, "  Transport Error Indicator (1 bit): 1 (at least one uncorrectable bit error in transport stream packet)\n");
  }

  // Payload Unit Start Indicator (1 bit)
  state->pusi = (value >> 6) & 1;
  fprintf (fo, "  Payload Unit Start Indicator (1 bit): %u\n", state->pusi);

  // Transport Priority (1 bit)
  fprintf (fo, "  Transport Priority (1 bit): %u\n", ((value >> 5) & 1));

  // Packet Identifier (PID) (13 bits)
  state->pid = ((value & 0x1F) << 8) | tsdata[offset + 1];

  switch (state->pid) {

    case 0x0000:  // Program Association Table (PAT)
      fprintf (fo, "  PID (13 bits): 0x%04x Program Association Table (PAT)\n", state->pid);
      state->pid_type[state->pid] = PID_PSI;
      break;

    case 0x0001:  // Conditional Access Table (CAT)
      fprintf (fo, "  PID (13 bits): 0x%04x Conditional Access Table (CAT)\n", state->pid);
      state->pid_type[state->pid] = PID_PSI;
      break;

    case 0x0002:  // Transport Stream Description Table (TSDT)
      fprintf (fo, "  PID (13 bits): 0x%04x Transport Stream Description Table (TSDT)\n", state->pid);
      state->pid_type[state->pid] = PID_PSI;
      break;

    case 0x0003:  // IPMP Control Information Table
      fprintf (fo, "  PID (13 bits): 0x%04x IPMP Control Information Table\n", state->pid);
      state->pid_type[state->pid] = PID_PSI;
      break;

    case 0x1fff:  // Null packet
      fprintf (fo, "  PID (13 bits): 0x%04x (Null packet)\n", state->pid);
      break;

    default:

  }  // End switch
  offset += 2;

  // TSC, AFC, CC (1 byte)
  value = tsdata[offset];

  // Transport Scrambling Control (2 bits)
  fprintf (fo, "  Transport Scrambling Control (2 bits): 0x%01x\n", (value >> 6) & 3);

  // Adaptation Field Control (2 bits)
  // Note that Transport Stream packets are 188 bytes long.
  adapt_field_cntrl = (value >> 4) & 3;
  switch (adapt_field_cntrl) {

    case 0:  // Reserved for future use by ISO/IEC. Neither adaptation field nor payload present. This should not occur.
      fprintf (fo, "  Adaptation Field Control (2 bits): %u%u (Reserved for future use by ISO/IEC)\n",
              (adapt_field_cntrl >> 1) & 1, adapt_field_cntrl & 1);
      break;

    case 1:  // 0x01 - No adaptation field; payload only
      fprintf (fo, "  Adaptation Field Control (2 bits): %u%u (No adaptation field; payload only)\n",
              (adapt_field_cntrl >> 1) & 1, adapt_field_cntrl & 1);
      break;

    case 2:  // 0x10 - Adaptation field only; no payload
      fprintf (fo, "  Adaptation Field Control (2 bits): %u%u (Adaptation field only; no payload)\n",
              (adapt_field_cntrl >> 1) & 1, adapt_field_cntrl & 1);
      if ((state->ts_index + 1) >= tslen) {
        fprintf (stderr, "Unexpected end of buffer\n");
        exit (EXIT_FAILURE);
      }
      break;

    case 3:  // 0x11 - Adaptation field followed by payload
      fprintf (fo, "  Adaptation Field Control (2 bits): %u%u (Adaptation field followed by payload)\n",
              (adapt_field_cntrl >> 1) & 1, adapt_field_cntrl & 1);
      if ((state->ts_index + 1) >= tslen) {
        fprintf (stderr, "Unexpected end of buffer\n");
        exit (EXIT_FAILURE);
      }
      break;

    default:
      fprintf (stderr, "Unknown Adaptation Field Control (2 bits) value: %u%u\n", (adapt_field_cntrl >> 1) & 1, adapt_field_cntrl & 1);
      fprintf (stderr, "Index of ts file is %zu\n", start + offset);
      exit (EXIT_FAILURE);
  }

  // Validate Adaptation Field Length.
  if (adapt_field_len > 183) {
    fprintf (stderr, "Invalid Adaptation Field Length of %zu bytes.\n", adapt_field_len);
    exit (EXIT_FAILURE);
  }

  // Continuity Counter (4 bits)
  fprintf (fo, "  Continuity Counter (4 bits): 0x%01x\n", value & 0x0f);
  offset++;

  // Parse Adaptation Field if it exists.
  if (adapt_field_cntrl == 2 || adapt_field_cntrl == 3) {

    adapt_field_len = (size_t) tsdata[offset];
    adapt_field_size = adapt_field_len + 1;

    // Validate Adaptation Field Length.
    if (adapt_field_len > 183) {
      fprintf (stderr, "Invalid Adaptation Field Length of %zu bytes.\n", adapt_field_len);
      exit (EXIT_FAILURE);
    }

    fprintf (fo, "  Adaptation Field length (1 byte): %zu bytes (Total AF size: %zu)\n", adapt_field_len, adapt_field_size);

    // Only attempt to parse Adaptation Field if there is Adaptation Field data (sometimes there isn't).
    if (adapt_field_len > 0) {
      parse_adapt_field (state, &offset, tsdata, tslen, fo);
    }

  }

  switch (adapt_field_cntrl) {

    case 1:  // 0x01 - No adaptation field; payload only
      ts_payloadlen = 188 - 4;
      break;

    case 3:  // 0x11 - Adaptation field followed by payload
      ts_payloadlen = 188 - 4 - adapt_field_size;
      break;

    default:
      ts_payloadlen = 0;
      break;

  }  // End switch adapt_field_cntrl
  fprintf (fo, "  Payload length: %lu bytes\n", ts_payloadlen);

  // Temporarily update state->ts_index for the benefit of parse_pes_header().
  state->ts_index = offset;

  // Add transport stream payload to appropriate stream buffer (PSI or PES) if not NULL packet.
  if ((ts_payloadlen > 0) && (state->pid != 0x1fff)) {

    switch (state->pid_type[state->pid]) {

      case PID_PSI:
        build_psi_section (state, pat, tsdata, tslen, ts_payloadlen, section, fo);
        break;

      case PID_PES:
        build_pes_segment (state, page, tsdata, tslen, ts_payloadlen, segment, pes, fo);
        break;

      case PID_UNKNOWN:  // Unknown; fall through to default.

      default:
        // Only PSI PIDs are allowed before classification by a PMT.
        if (state->pid == 0x0000 ||     // PAT
            state->pid == 0x0001 ||     // CAT
            state->pid == 0x0002 ||     // TSDT
            state->pid == 0x0011) {     // SDT/BAT
          build_psi_section (state, pat, tsdata, tslen, ts_payloadlen, section, fo);
        }
        // Else: ignore silently
        break;

    }  // End switch
  }

  // Ensure we never get out of step with packet boundaries.
  state->ts_index = start + 188;

  return EXIT_SUCCESS;
}
