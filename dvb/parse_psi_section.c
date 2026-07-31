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

// Parse Transport Stream PSI section which was reassembled from TS payloads.
// Reference: ETSI EN 300 468 - Digital Video Broadcasting (DVB): Specification for Service Information (SI) in DVB Systems
int
parse_psi_section (STATE *state, PAT *pat, SECTION *section, FILE *fo) {

  uint8_t table_id;
  uint16_t pid;

  fprintf (fo, "\nReassembled Program Specific Information (PSI) Section (PID 0x%04x):\n", state->pid);

  pid = state->pid;

  // Parse PSI section as appropriate for table_id.
  // See Table 2: "Allocation of table_id values" of ETSI EN 300 468
  if (section[pid].buffer == NULL) {
    fprintf (stderr, "Unexpectedly reached end of section in parse_psi_section().\n");
    exit (EXIT_FAILURE);
  }
  table_id = section[pid].buffer[0];
  switch (table_id) {

    case 0x00:  // Program Association Table (PAT)
      if (pid != 0x0000) {
        fprintf (stderr, "PAT is on a non-zero PID.\n");
        fprintf (stderr, "Expected PID 0x0000, but have 0x%04x\n", pid);
        exit (EXIT_FAILURE);
      }
      parse_pat (state, pat, section, fo);
      break;

    case 0x01:  // Conditional Access Table (CAT)
//      parse_cat ();
      break;

    case 0x02:  // Program Map Table (PMT)
      parse_pmt (state, pat, section, fo);
      break;

    case 0x03:  // Transport Stream Description Table (TSDT)
//      parse_tsdt ();
      break;

    case 0x42:  // Service Description Table (SDT) (actual DVB transport stream); fall through to case 0x46.
    case 0x46:  // Service Description Table (SDT) (other DVB transport stream)
      parse_sdt (state, section, fo);
      break;

    case 0x4a:  // Bouquet Association Table (BAT)
//      parse_bat (state, section, fo);
      break;

    default:

      // Fail if reserved PIDs encountered.
      if ((pid >= 0x0004) && (pid <= 0x000f)) {
        fprintf (stderr, "PIDs 0x0004 to 0x000f are reserved.\n");
        fprintf (stderr, "PID: 0x%04x\n", pid);
        exit (EXIT_FAILURE);
      } else {
        fprintf (fo, "Unknown/Unhandled PSI table_id 0x%02x on PID 0x%04x\n", table_id, pid);
        break;
      }

  }  // End switch pid

  return (EXIT_SUCCESS);
}
