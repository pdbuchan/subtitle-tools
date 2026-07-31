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

// Parse a Service Description Table (SDT)
// PID = 0x0011
// table_id: 0x42 SDT for actual DVB transport stream
// table_id: 0x46 SDT for other DVB transport stream
// Reference: ETSI EN 300 468 (Section 5.2.3)
int
parse_sdt (STATE *state, SECTION *section, FILE *fo) {

  size_t i, offset, service_start, section_length, desc_end, section_end, d, descriptors_length, descriptor_length, provider_name_length, service_name_length;
  uint8_t table_id, version_number, current_next_indicator, section_syntax_indicator, section_number, last_section_number, descriptor_tag, eit_schedule_flag, eit_present_following_flag;
  uint8_t running_status, free_ca_mode, service_type;
  uint16_t pid, transport_stream_id, original_network_id, service_id;

  pid = state->pid;
  offset = 0;
  service_start = offset;

  // Table ID (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }
  table_id = section[pid].buffer[offset];
  offset++;
  if (table_id == 0x4a) {
    fprintf (fo, "Bouquet Association Table (BAT)\n");
    return (EXIT_SUCCESS);
  } else if (table_id == 0x42) {
    fprintf(fo, "Service Description Table (SDT)\n");
  }
  fprintf (fo, "  Table ID (1 byte): 0x%02x\n", table_id);

  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }

  // Section Syntax Indicator (1 bit)
  section_syntax_indicator = (section[pid].buffer[offset] >> 7) & 1;
  fprintf (fo, "  Section Syntax Indicator (1 bit): %u\n", section_syntax_indicator);

  // Reserved for future use (1 bit)

  // Reserved (2 bits)

  // Section Length (12 bits)
  section_length = (section[pid].buffer[offset] & 0x0f) << 8 |
                 section[pid].buffer[offset + 1];
  offset += 2;
  fprintf (fo, "  Section Length (12 bits): %zu bytes (%zu bytes including table ID, SSI, section len)\n", section_length, section_length + 3);

  // Minimum SDT size: header + CRC
  if (section_length < 9) {
    return EXIT_SUCCESS;
  }

  // Transport Stream ID (2 bytes)
  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }
  transport_stream_id = (section[pid].buffer[offset]) << 8 |
                 section[pid].buffer[offset + 1];
  offset += 2;
  fprintf (fo, "  Transport Stream ID (2 bytes): 0x%04x\n", transport_stream_id);

  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }

  // Reserved (2 bits)

  // Version Number (5 bits)
  version_number = (section[pid].buffer[offset] >> 1) & 0x1f;  // 0x1f = 11111
  fprintf (fo, "  Version Number (5 bits): 0x%02x\n", version_number);

  // Normally you don't bother processing anything more if version hasn't changed.
  // We will continue anyway for the sake of fully documenting the .ts file.

  // Current Next Indicator (1 bit)
  current_next_indicator = section[pid].buffer[offset] & 1;
  offset++;
  fprintf (fo, "  Current Next Indicator (1 bit): %d\n", current_next_indicator);

  // Section Number (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }
  section_number = section[pid].buffer[offset];
  offset++;
  fprintf (fo, "  Section Number (1 byte): 0x%02x\n", section_number);

  // Last Section Number (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }
  last_section_number = section[pid].buffer[offset];
  offset++;
  fprintf (fo, "  Last Section Number (1 byte): 0x%02x\n", last_section_number);

  // Original Network ID (2 bytes)
  if ((offset + 1) >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }
  original_network_id = (section[pid].buffer[offset] << 8) |
                         section[pid].buffer[offset + 1];
  offset += 2;
  fprintf (fo, "  Original Network ID (2 bytes): 0x%04x\n", original_network_id);

  // Reserved for future use (1 byte)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }
  offset++;

  // Service loop
  // Runs until we reach the CRC (last 4 bytes of the section)
  while (offset < (3 + section_length - 4)) {

    // Service ID (2 bytes)
    if ((offset + 1) >= MAX_BUFFERLEN) {
      fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
      exit (EXIT_FAILURE);
    }
    service_id = (section[pid].buffer[offset] << 8) |
                  section[pid].buffer[offset + 1];
    offset += 2;
    fprintf (fo, "  Service ID (2 bytes): 0x%04x\n", service_id);

    if (offset >= MAX_BUFFERLEN) {
      fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
      exit (EXIT_FAILURE);
    }

    // Reserved for future use (6 bits)

    // EIT Schedule Flag (1 bit)
    eit_schedule_flag = (section[pid].buffer[offset] >> 1) & 1;

    // EIT Present/Following Flag (1 bit)
    eit_present_following_flag = section[pid].buffer[offset] & 1;
    offset++;
    fprintf (fo, "    EIT Schedule Flag (1 bit): %u\n", eit_schedule_flag);
    fprintf (fo, "    EIT Present/Following Flag (1 bit): %u\n", eit_present_following_flag);

    if ((offset + 1) >= MAX_BUFFERLEN) {
      fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
      exit (EXIT_FAILURE);
    }

    // Running Status (3 bits)
    running_status = (section[pid].buffer[offset] >> 5) & 0x07;
    switch (running_status) {

      case 0:
        fprintf (fo, "    Running Status (3 bits): %u Undefined\n", running_status);
        break;

      case 1:
        fprintf (fo, "    Running Status (3 bits): %u Not running\n", running_status);
        break;

      case 2:
        fprintf (fo, "    Running Status (3 bits): %u Starts in a few seconds (e.g., for video recording)\n", running_status);
        break;

      case 3:
        fprintf (fo, "    Running Status (3 bits): %u Pausing\n", running_status);
        break;

      case 4:
        fprintf (fo, "    Running Status (3 bits): %u Running\n", running_status);
        break;

      case 5:
        fprintf (fo, "    Running Status (3 bits): %u Service off-air\n", running_status);
        break;

      default:
        fprintf (fo, "    Running Status (3 bits): %u Reserved for future use\n", running_status);
        break;
    }

    // Free CA Mode (1 bit)
    free_ca_mode = (section[pid].buffer[offset] >> 4) & 1;
    if (!free_ca_mode) {
      fprintf (fo, "    Free CA Mode (1 bit): %u All component streams of the service are not scrambled\n", free_ca_mode);
    } else {
      fprintf (fo, "    Free CA Mode (1 bit): %u Access to one or more streams may be controlled by a conditional access system\n", free_ca_mode);
    }

    // Descriptors Length (12 bits)
    descriptors_length = (size_t)
        (((section[pid].buffer[offset] & 0x0f) << 8) |
         section[pid].buffer[offset + 1]);
    offset += 2;

    fprintf (fo, "    Descriptors Length (12 bits): %zu bytes\n", descriptors_length);

    // Descriptor Loop
    section_end = 3 + section_length - 4;
    desc_end = offset + descriptors_length;
    if (desc_end > section_end) {
      desc_end = section_end;
    }
    while ((offset + 2) < desc_end) {

      if ((offset + 1) >= MAX_BUFFERLEN) {
        fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
        exit (EXIT_FAILURE);
      }
      descriptor_tag = section[pid].buffer[offset];
      descriptor_length = (size_t) section[pid].buffer[offset + 1];
      if (descriptor_length == 0) break;
      if ((offset + 2 + descriptor_length) > desc_end) break;

      offset += 2;

      fprintf (fo, "      Descriptor Tag: 0x%02x\n", descriptor_tag);
      fprintf (fo, "      Descriptor Length: %zu\n", descriptor_length);

      // Descriptor data (skipped, but dumped as hex)
      fprintf (fo, "      Descriptor Data: ");
      if ((offset + descriptor_length - 1) >= MAX_BUFFERLEN) {
        fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
        exit (EXIT_FAILURE);
      }
      for (i = 0; i < descriptor_length; i++) {
        fprintf (fo, "%02x", section[pid].buffer[offset + i]);
      }
      fprintf (fo, "\n");

      if ((descriptor_tag == 0x48) && (descriptor_length >= 3)) {

        // Descriptor payload start
        d = offset;

        // Service Type (1 byte)
        if (d >= MAX_BUFFERLEN) {
          fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
          exit (EXIT_FAILURE);
        }
        service_type = section[pid].buffer[d++];

        // Provider Name Length (1 byte)
        if (d >= MAX_BUFFERLEN) {
          fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
          exit (EXIT_FAILURE);
        }
        provider_name_length = (size_t) section[pid].buffer[d++];

        fprintf (fo, "        Service Type: 0x%02x\n", service_type);

        fprintf (fo, "        Provider Name Length: %zu\n", provider_name_length);
        fprintf (fo, "        Provider Name: ");
        for (i = 0; i < provider_name_length && d < (offset + descriptor_length); i++) {
          if (d >= MAX_BUFFERLEN) {
            fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
            exit (EXIT_FAILURE);
          }
          fputc (section[pid].buffer[d++], fo);
        }
        fprintf (fo, "\n");

        if (d < (offset + descriptor_length)) {
          service_name_length = (size_t) section[pid].buffer[d];
          d++;
          fprintf (fo, "        Service Name Length: %zu\n", service_name_length);

          fprintf (fo, "        Service Name: ");
          for (i = 0; ((i < service_name_length) && (d < (offset + descriptor_length))); i++) {
            if (d >= MAX_BUFFERLEN) {
              fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
              exit (EXIT_FAILURE);
            }
            fputc (section[pid].buffer[d], fo);
            d++;
          }
          fprintf (fo, "\n");
        }
      }

      offset += descriptor_length;
    }

    // Prevent infinite loop on malformed data.
    if (offset <= service_start) {
      break;
    }
  }

  // CRC (4 bytes)
  if (offset >= MAX_BUFFERLEN) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_sdt().\n");
    exit (EXIT_FAILURE);
  }
  fprintf (fo, "  CRC (4 bytes): ");
  for (i = 0; i < 4; i++) {
    fprintf (fo, "%02x", section[pid].buffer[offset + i]);
  }
  fprintf (fo, "\n");

  return (EXIT_SUCCESS);
}
