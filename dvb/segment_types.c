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

// DVB Subtitling Segment Types
// ETSI EN 300 743 (Table 2)
int
segment_types (STATE *state, uint8_t segment_type, FILE *fo) {

  switch (segment_type) {

    case 0x10:
      fprintf (fo, "    Segment Type (1 byte): 0x%02x Page Composition Segment (PCS)\n", segment_type);
      break;

    case 0x11:
      fprintf (fo, "    Segment Type (1 byte): 0x%02x Region Composition Segment (RCS)\n", segment_type);
      break;

    case 0x12:
      fprintf (fo, "    Segment Type (1 byte): 0x%02x CLUT Definition Segment (CDS)\n", segment_type);
      break;

    case 0x13:
      fprintf (fo, "    Segment Type (1 byte): 0x%02x Object Data Segment (ODS)\n", segment_type);
      break;

    case 0x14:
      fprintf (fo, "    Segment Type (1 byte): 0x%02x Display Definition Segment (DDS)n", segment_type);
      break;

    case 0x15:
      fprintf (fo, "    Segment Type (1 byte): 0x%02x Disparity Signalling Segment (DSS)\n", segment_type);
      break;

    case 0x80:
      fprintf (fo, "    Segment Type (1 byte): 0x%02x End of Display Set Segment (END)\n", segment_type);
      break;

    default:
      if ((segment_type >= 0x81) && (segment_type <= 0xef)) {
        fprintf (fo, "    Segment Type: 0x%02x Private Data\n", segment_type);
    
      } else if (segment_type == 0xff) {
        fprintf (fo, "    Segment Type: 0x%02x Stuffing\n", segment_type);
    
      } else {
        fprintf (fo, "    Segment Type: 0x%02x Reserved for future use.\n", segment_type);
      }
  
  }  // End switch

  return (EXIT_SUCCESS);
}
