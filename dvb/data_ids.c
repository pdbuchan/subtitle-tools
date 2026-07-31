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

// Data Identifiers for DVB Transport Streams
// ETSI EN 301 192 (Table 2)
int
data_ids (STATE *state, uint8_t data_identifier, FILE *fo) {

  fprintf (fo, "  Data Identifier (1 byte): 0x%02x ", data_identifier);

  switch (data_identifier) {

    case 0x20:
      fprintf (fo, "DVB subtitling (see ETSI EN 300 743)\n");
      break;

    case 0x21:
      fprintf (fo, "DVB synchronous data stream\n");
      break;

    case 0x22:
      fprintf (fo, "DVB synchronized data stream\n");
      break;

    default:
      if ((data_identifier >= 0x00) && (data_identifier <= 0x0f)) {
        fprintf (fo, "Reserved for future use\n");
      } else if ((data_identifier >= 0x10) && (data_identifier <= 0x1f)) {
        fprintf (fo, "Reserved for EBU data (see ETSI EN 300 472)\n");
      } else if ((data_identifier >= 0x23) && (data_identifier <= 0x7f)) {
        fprintf (fo, "Reserved for future use\n");
      } else if ((data_identifier >= 0x80) && (data_identifier <= 0xff)) {
        fprintf (fo, "User defined\n");
     }

  }  // End switch

  return (EXIT_SUCCESS);
}
