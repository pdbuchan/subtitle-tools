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

// PES stream ID assignments
// ISO/IEC 13818-1 (Table 2-22)
int
stream_ids (STATE *state, FILE *fo) {

  fprintf (fo, "    Stream ID (1 byte): 0x%02x ", state->stream_id);

  switch (state->stream_id) {

    case 0xbc:
      fprintf (fo, "program_stream_map\n");
      break;

    case 0xbd:
      fprintf (fo, "private_stream_1\n");
      break;

    case 0xbe:
      fprintf (fo, "padding_stream\n");
      break;

    case 0xbf:
      fprintf (fo, "private_stream_2\n");
      break;

    case 0xf0:
      fprintf (fo, "ECM_stream\n");
      break;

    case 0xf1:
      fprintf (fo, "EMM_stream\n");
      break;

    case 0xf2:
      fprintf (fo, "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A or ISO/IEC 13818-6_DSMCC_stream\n");
      break;

    case 0xf3:
      fprintf (fo, "ISO/IEC_13522_stream\n");
      break;

    case 0xf4:
      fprintf (fo, "ITU-T Rec. H.222.1 type A\n");
      break;

    case 0xf5:
      fprintf (fo, "ITU-T Rec. H.222.1 type B\n");
      break;

    case 0xf6:
      fprintf (fo, "ITU-T Rec. H.222.1 type C\n");
      break;

    case 0xf7:
      fprintf (fo, "ITU-T Rec. H.222.1 type D\n");
      break;

    case 0xf8:
      fprintf (fo, "ITU-T Rec. H.222.1 type E\n");
      break;

    case 0xf9:
      fprintf (fo, "ancillary_stream\n");
      break;

    case 0xfa:
      fprintf (fo, "ISO/IEC 14496-1_SL-packetized_stream\n");
      break;

    case 0xfb:
      fprintf (fo, "ISO/IEC 14496-1_FlexMux_stream\n");
      break;

    case 0xfc:
      fprintf (fo, "metadata stream\n");
      break;

    case 0xfd:
      fprintf (fo, "extended_stream_id\n");
      break;

    case 0xfe:
      fprintf (fo, "reserved data stream\n");
      break;

    case 0xff:
      fprintf (fo, "program_stream_directory\n");
      break;

    default:
      if ((state->stream_id >= 0xc0) && (state->stream_id <= 0xdf)) {
        fprintf (fo, "ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC 14496-3 audio stream number 0x%02x\n", state->stream_id & 0x1f);  // 0x1f = 11111
        break;
      } else if ((state->stream_id >= 0xe0) && (state->stream_id <= 0xef)) {
        fprintf (fo, "ITU-T Rec. H.262 | ISO/IEC 13818-2, ISO/IEC 11172-2, ISO/IEC 14496-2 or ITU-T Rec. H.264 | ISO/IEC 14496-10 video stream number 0x%02x\n", state->stream_id & 0x0f);
        break;
      }

  }  // End switch

  return (EXIT_SUCCESS);
}
