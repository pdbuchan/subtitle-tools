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

// Parse a Transport Stream PSI section which has been reassembled from one or
// more TS packet payloads.
//
// References:
//   ISO/IEC 13818-1 for PAT and PMT
//   ETSI EN 300 468 for DVB Service Information tables such as the SDT
int
parse_psi_section (STATE *state, PAT *pat, SECTION *section, FILE *fo) {

  size_t declared_length;
  uint8_t table_id, ssi;
  uint16_t pid = state->pid;

  fprintf (fo, "\nReassembled PSI Section (PID 0x%04x):\n", pid);

  // Every PSI section begins with table_id and the 12-bit section_length.
  if (!section[pid].buffer || section[pid].length < 3) {
    return (EXIT_FAILURE);
  }

  // Confirm that the amount reassembled by build_psi_section() agrees exactly
  // with the section_length carried by the section itself.
  declared_length = 3U + (size_t) (((section[pid].buffer[1] & 0x0f) << 8) | section[pid].buffer[2]);
  if (declared_length != section[pid].length) {
    return (EXIT_FAILURE);
  }

  // Long-form PSI sections finish with MPEG-2 CRC-32. Computing the CRC over
  // the complete section, including its transmitted CRC, must produce zero.
  ssi = (section[pid].buffer[1] >> 7) & 1;
  if (ssi && mpeg2_crc32 (section[pid].buffer, section[pid].length) != 0) {
    fprintf (stderr, "Invalid MPEG-2 CRC-32 on PID 0x%04x.\n", pid);
    return (EXIT_FAILURE);
  }

  // Parse the section according to its table_id.
  table_id = section[pid].buffer[0];

  // Program Association Table (PAT), required on PID 0x0000.
  if (table_id == 0x00) {
    if (pid != 0) return (EXIT_FAILURE);
    return (parse_pat (state, pat, section, fo));
  }

  // Program Map Table (PMT).
  if (table_id == 0x02) {
    return (parse_pmt (state, pat, section, fo));
  }

  // Service Description Table (SDT), actual or other transport stream.
  if (table_id == 0x42 || table_id == 0x46) {
    return (parse_sdt (state, section, fo));
  }

  // CAT, TSDT, and BAT are recognized but are not needed for subtitle
  // extraction, so they are not treated as errors.
  if (table_id == 0x01 || table_id == 0x03 || table_id == 0x4a) {
    return (EXIT_SUCCESS);
  }

  fprintf (fo, "Unknown/Unhandled PSI table_id 0x%02x on PID 0x%04x\n", table_id, pid);

  return (EXIT_SUCCESS);
}
