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

// Parse one 188-byte MPEG-2 Transport Stream packet and route its payload to
// the appropriate PSI-section or Teletext-PES reassembly buffer.
int
parse_ts_packet (STATE *state, TTX_CONTEXT *ttx, PAT *pat, uint8_t *tsdata, size_t tslen, size_t *packet, PES *pes, SECTION *section, SEGMENT *segment, FILE *fo) {

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
    return (EXIT_FAILURE);
  }

  fprintf (fo, "\nTransport Stream Packet %zu:\n", *packet);

  if (tsdata[offset] != 0x47) {
    fprintf (stderr, "Found invalid sync byte of 0x%02x. Should be 0x47.\n", tsdata[offset]);
    fprintf (stderr, "Index of ts file is 0x%08zx (%zu)\n", offset, offset);
    return (EXIT_FAILURE);
  }
  fprintf (fo, "  Sync Byte (1 byte): 0x%02x\n", tsdata[offset]);
  offset++;

  value = tsdata[offset];
  tei = (value >> 7) & 1;
  fprintf (fo, "  Transport Error Indicator (1 bit): %u%s\n", tei, tei ? " (uncorrectable TS-packet error indicated)" : "");
  state->pusi = (value >> 6) & 1;
  fprintf (fo, "  Payload Unit Start Indicator (1 bit): %u\n", state->pusi);
  fprintf (fo, "  Transport Priority (1 bit): %u\n", (value >> 5) & 1);
  state->pid = (uint16_t) (((value & 0x1f) << 8) | tsdata[offset + 1]);

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

    case 0x0011:  // SDT/BAT
      fprintf (fo, "  PID (13 bits): 0x%04x SDT/BAT\n", state->pid);
      state->pid_type[state->pid] = PID_PSI;
      break;

    case 0x1fff:  // Null packet
      fprintf (fo, "  PID (13 bits): 0x%04x (Null packet)\n", state->pid);
      break;

    default:
      fprintf (fo, "  PID (13 bits): 0x%04x\n", state->pid);
      break;
  }  // End switch
  offset += 2;

  value = tsdata[offset];
  fprintf (fo, "  Transport Scrambling Control (2 bits): 0x%01x\n", (value >> 6) & 3);
  adapt_field_cntrl = (value >> 4) & 3;
  switch (adapt_field_cntrl) {

    case 0:
      fprintf (fo, "  Adaptation Field Control (2 bits): 00 (reserved)\n");
      break;

    case 1:
      fprintf (fo, "  Adaptation Field Control (2 bits): 01 (payload only)\n");
      break;

    case 2:
      fprintf (fo, "  Adaptation Field Control (2 bits): 10 (adaptation field only)\n");
      break;

    case 3:
      fprintf (fo, "  Adaptation Field Control (2 bits): 11 (adaptation field and payload)\n");
      break;

    default:
      return (EXIT_FAILURE);
  }  // End switch
  fprintf (fo, "  Continuity Counter (4 bits): 0x%01x\n", value & 0x0f);

  // EN 300 472 constrains Teletext TS packets to AFC values 01 (payload only)
  // and 10 (adaptation field only). Keep parsing a non-conforming 11 packet,
  // but call it out in the structural report rather than silently accepting it.
  if (state->pid_type[state->pid] == PID_PES && adapt_field_cntrl != 1 && adapt_field_cntrl != 2) {
    fprintf (fo, "  WARNING: EN 300 472 permits only AFC 01 or 10 on a Teletext PID.\n");
  }
  offset++;

  if (adapt_field_cntrl == 2 || adapt_field_cntrl == 3) {
    if (offset >= start + 188) return (EXIT_FAILURE);
    adapt_field_len = tsdata[offset];
    adapt_field_size = adapt_field_len + 1;
    if (adapt_field_len > 183 || offset + adapt_field_size > start + 188) {
      fprintf (stderr, "Invalid Adaptation Field Length of %zu bytes.\n", adapt_field_len);
      return (EXIT_FAILURE);
    }
    fprintf (fo, "  Adaptation Field length (1 byte): %zu bytes (Total AF size: %zu)\n", adapt_field_len, adapt_field_size);
    if (adapt_field_len > 0) {
      if (parse_adapt_field (state, &offset, tsdata, tslen, fo) != EXIT_SUCCESS) {
        return (EXIT_FAILURE);
      }
    } else {
      // Even an empty adaptation field occupies its one-byte length field.
      // parse_adapt_field() normally consumes that byte for non-empty fields.
      offset++;
    }
  }

  if (adapt_field_cntrl == 1) {
    ts_payloadlen = 184;
  } else if (adapt_field_cntrl == 3) {
    ts_payloadlen = 184 - adapt_field_size;
  }
  fprintf (fo, "  Payload length: %zu bytes\n", ts_payloadlen);

  // parse_adapt_field() advances offset to the byte immediately after the
  // complete adaptation field, which is therefore the first payload byte.
  state->ts_index = offset;

  if (ts_payloadlen > 0 && state->pid != 0x1fff) {
    switch (state->pid_type[state->pid]) {

      case PID_PSI:
        if (build_psi_section (state, pat, tsdata, tslen, ts_payloadlen, section, fo) != EXIT_SUCCESS) return (EXIT_FAILURE);
        break;

      case PID_PES:
        if (build_pes_segment (state, ttx, tsdata, tslen, ts_payloadlen, segment, pes, pat, fo) != EXIT_SUCCESS) return (EXIT_FAILURE);
        break;

      case PID_UNKNOWN:  // Fall through to default case.

      default:
        // These fixed PSI PIDs can be recognized before a PMT has classified
        // any elementary streams. Other unknown PIDs are intentionally ignored.
        if (state->pid == 0x0000 || state->pid == 0x0001 ||
            state->pid == 0x0002 || state->pid == 0x0011) {
          if (build_psi_section (state, pat, tsdata, tslen, ts_payloadlen, section, fo) != EXIT_SUCCESS) return (EXIT_FAILURE);
        }
        break;
    }
  }

  state->ts_index = start + 188;

  return (EXIT_SUCCESS);
}
