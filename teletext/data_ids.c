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

// DVB PES data_identifier values relevant to Teletext.
// Reference: ETSI EN 300 472, Table 3.
int
data_ids (uint8_t data_identifier, FILE *fo) {

  fprintf (fo, "  Data Identifier (1 byte): 0x%02x ", data_identifier);

  if (data_identifier <= 0x0f) {
    fprintf (fo, "Reserved for future use\n");
  } else if (data_identifier <= 0x1f) {
    fprintf (fo, "EBU data\n");
  } else if (data_identifier <= 0x7f) {
    fprintf (fo, "Reserved for future use\n");
  } else {
    fprintf (fo, "User defined\n");
  }

  return (EXIT_SUCCESS);
}
