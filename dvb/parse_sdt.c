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

// Parse a Service Description Table (SDT).
// PID = 0x0011
// table_id 0x42 = SDT for the actual DVB transport stream
// table_id 0x46 = SDT for another DVB transport stream
// Reference: ETSI EN 300 468, section 5.2.3.
int
parse_sdt (STATE *state, SECTION *section, FILE *fo) {

  size_t offset, section_length, end, desc_len, desc_end, i;
  uint16_t pid = state->pid, tsid, onid, service_id;
  uint8_t *buf, table_id, version, current, section_number, last_section;

  if (!section[pid].buffer || section[pid].length < 3) {
    return (EXIT_FAILURE);
  }
  buf = section[pid].buffer;

  // Table ID (1 byte).
  table_id = buf[0];

  // BAT shares the PID but is not parsed here.
  if (table_id == 0x4a) {
    fprintf (fo, "Bouquet Association Table (BAT)\n");
    return (EXIT_SUCCESS);
  }
  if (table_id != 0x42 && table_id != 0x46) {
    return (EXIT_FAILURE);
  }

  // Section Syntax Indicator, reserved bits, and Section Length occupy bytes
  // 1 and 2. parse_psi_section() has already validated the CRC.
  section_length = (size_t) (((buf[1] & 0x0f) << 8) | buf[2]);
  if (section_length < 12 || 3 + section_length != section[pid].length) {
    fprintf (stderr, "Invalid SDT section length.\n");
    return (EXIT_FAILURE);
  }
  end = section[pid].length - 4;  // CRC begins here; already verified by caller.
  offset = 3;
  if (!bytes_available (offset, 8, end)) {
    return (EXIT_FAILURE);
  }

  // Transport Stream ID (2 bytes).
  tsid = (uint16_t) (((uint16_t) buf[offset] << 8) | buf[offset + 1]);
  offset += 2;

  // Version Number (5 bits) and Current/Next Indicator (1 bit).
  version = (buf[offset] >> 1) & 0x1f;
  current = buf[offset++] & 1;

  // Section Number and Last Section Number (1 byte each).
  section_number = buf[offset++];
  last_section = buf[offset++];

  // Original Network ID (2 bytes), followed by reserved_future_use (1 byte).
  onid = (uint16_t) (((uint16_t) buf[offset] << 8) | buf[offset + 1]);
  offset += 2;
  offset++;

  fprintf (fo, "Service Description Table (SDT)\n");
  fprintf (fo, "  Table ID: 0x%02x\n", table_id);
  fprintf (fo, "  Section Length: %zu bytes\n", section_length);
  fprintf (fo, "  Transport Stream ID: 0x%04x\n", tsid);
  fprintf (fo, "  Version Number: 0x%02x\n", version);
  fprintf (fo, "  Current Next Indicator: %u\n", current);
  fprintf (fo, "  Section Number: 0x%02x\n", section_number);
  fprintf (fo, "  Last Section Number: 0x%02x\n", last_section);
  fprintf (fo, "  Original Network ID: 0x%04x\n", onid);

  // Service loop. Entries continue until the four-byte CRC at end.
  while (offset < end) {
    uint8_t eit_schedule, eit_pf, running, free_ca;

    if (!bytes_available (offset, 5, end)) {
      fprintf (stderr, "Truncated SDT service entry.\n");
      return (EXIT_FAILURE);
    }

    // Service ID (2 bytes).
    service_id = (uint16_t) (((uint16_t) buf[offset] << 8) | buf[offset + 1]);
    offset += 2;

    // Reserved future use (6 bits), EIT Schedule Flag (1 bit), and EIT
    // Present/Following Flag (1 bit).
    eit_schedule = (buf[offset] >> 1) & 1;
    eit_pf = buf[offset++] & 1;

    // Running Status (3 bits), Free CA Mode (1 bit), and high four bits of
    // Descriptors Loop Length.
    running = (buf[offset] >> 5) & 7;
    free_ca = (buf[offset] >> 4) & 1;
    desc_len = (size_t) (((buf[offset] & 0x0f) << 8) | buf[offset + 1]);
    offset += 2;

    if (!bytes_available (offset, desc_len, end)) {
      fprintf (stderr, "SDT descriptor loop exceeds section length.\n");
      return (EXIT_FAILURE);
    }
    desc_end = offset + desc_len;

    fprintf (fo, "  Service ID: 0x%04x\n", service_id);
    fprintf (fo, "    EIT Schedule Flag: %u\n", eit_schedule);
    fprintf (fo, "    EIT Present/Following Flag: %u\n", eit_pf);
    fprintf (fo, "    Running Status: %u\n", running);
    fprintf (fo, "    Free CA Mode: %u\n", free_ca);
    fprintf (fo, "    Descriptors Length: %zu bytes\n", desc_len);

    // Descriptor loop for this service.
    while (offset < desc_end) {
      uint8_t tag, dlen;
      size_t payload_start, d;

      if (!bytes_available (offset, 2, desc_end)) {
        return (EXIT_FAILURE);
      }
      tag = buf[offset++];
      dlen = buf[offset++];
      if (!bytes_available (offset, dlen, desc_end)) {
        return (EXIT_FAILURE);
      }
      payload_start = offset;

      // Descriptor Tag (1 byte), Descriptor Length (1 byte), followed by the
      // descriptor payload. Always dump the raw payload even when the specific
      // descriptor is not decoded below.
      fprintf (fo, "      Descriptor Tag: 0x%02x\n", tag);
      fprintf (fo, "      Descriptor Length: %u\n", dlen);
      fprintf (fo, "      Descriptor Data: ");
      for (i = 0; i < dlen; i++) {
        fprintf (fo, "%02x", buf[offset + i]);
      }
      fprintf (fo, "\n");

      // Service Descriptor (tag 0x48): Service Type, Provider Name Length and
      // string, then Service Name Length and string.
      if (tag == 0x48 && dlen >= 3) {
        size_t provider_len, service_len;
        d = payload_start;
        fprintf (fo, "        Service Type: 0x%02x\n", buf[d++]);
        provider_len = buf[d++];
        if (provider_len > payload_start + dlen - d) {
          return (EXIT_FAILURE);
        }
        fprintf (fo, "        Provider Name: ");
        for (i = 0; i < provider_len; i++) fputc (buf[d++], fo);
        fprintf (fo, "\n");
        if (d < payload_start + dlen) {
          service_len = buf[d++];
          if (service_len > payload_start + dlen - d) {
            return (EXIT_FAILURE);
          }
          fprintf (fo, "        Service Name: ");
          for (i = 0; i < service_len; i++) fputc (buf[d++], fo);
          fprintf (fo, "\n");
        }
      }

      offset = payload_start + dlen;
    }
  }

  // CRC (4 bytes). Already checked by parse_psi_section(), but include it in
  // the human-readable report.
  fprintf (fo, "  CRC (4 bytes): %02x%02x%02x%02x\n", buf[end], buf[end + 1], buf[end + 2], buf[end + 3]);

  return (EXIT_SUCCESS);
}
