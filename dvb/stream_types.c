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

// PES Stream Type Assignments
// Reference: ISO/IEC 13818-1 (Table 2-34)
int
stream_types (STATE *state, uint8_t stream_type, FILE *fo) {

  (void) state;

  fprintf (fo, "    Stream Type (1 byte): 0x%02x ", stream_type);

  switch (stream_type) {

    case 0x00:
      fprintf (fo, "ITU-T | ISO/IEC Reserved\n");
      break;

    case 0x01:
      fprintf (fo, "ISO/IEC 11172-2 Video\n");
      break;

    case 0x02:
      fprintf (fo, "ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream\n");
      break;

    case 0x03:
      fprintf (fo, "ISO/IEC 11172-3 Audio\n");
      break;

    case 0x04:
      fprintf (fo, "ISO/IEC 13818-3 Audio\n");
      break;

    case 0x05:
      fprintf (fo, "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections\n");
      break;

    case 0x06:
      fprintf (fo, "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data\n");
      break;

    case 0x07:
      fprintf (fo, "ISO/IEC 13522 MHEG\n");
      break;

    case 0x08:
      fprintf (fo, "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC\n");
      break;

    case 0x09:
      fprintf (fo, "ITU-T Rec. H.222.1\n");
      break;

    case 0x0a:
      fprintf (fo, "ISO/IEC 13818-6 type A\n");
      break;

    case 0x0b:
      fprintf (fo, "ISO/IEC 13818-6 type B\n");
      break;

    case 0x0c:
      fprintf (fo, "ISO/IEC 13818-6 type C\n");
      break;

    case 0x0d:
      fprintf (fo, "ISO/IEC 13818-6 type D\n");
      break;

    case 0x0e:
      fprintf (fo, "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary\n");
      break;

    case 0x0f:
      fprintf (fo, "ISO/IEC 13818-7 Audio with ADTS transport syntax\n");
      break;

    case 0x10:
      fprintf (fo, "ISO/IEC 14496-2 Visual\n");
      break;

    case 0x11:
      fprintf (fo, "ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3\n");
      break;

    case 0x12:
      fprintf (fo, "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets\n");
      break;

    case 0x13:
      fprintf (fo, "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections\n");
      break;

    case 0x14:
      fprintf (fo, "ISO/IEC 13818-6 Synchronized Download Protocol\n");
      break;

    case 0x15:
      fprintf (fo, "Metadata carried in PES packets\n");
      break;

    case 0x16:
      fprintf (fo, "Metadata carried in metadata_sections\n");
      break;

    case 0x17:
      fprintf (fo, "Metadata carried in ISO/IEC 13818-6 Data Carousel\n");
      break;

    case 0x18:
      fprintf (fo, "Metadata carried in ISO/IEC 13818-6 Object Carousel\n");
      break;

    case 0x19:
      fprintf (fo, "Metadata carried in ISO/IEC 13818-6 Synchronized Download Protocol\n");
      break;

    case 0x1a:
      fprintf (fo, "IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP)\n");
      break;

    case 0x1b:
      fprintf (fo, "AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video\n");
      break;

    case 0x7f:
      fprintf (fo, "IPMP stream\n");
      break;

    default:

      if ((stream_type >= 0x1c) && (stream_type <= 0x7e)) {
        fprintf (fo, "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved\n");
        break;
      } else if (stream_type >= 0x80) {
        fprintf (fo, "User Private\n");
      }
  }  // End switch

  return (EXIT_SUCCESS);
}
