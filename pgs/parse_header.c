/*  Copyright (C) 2024-2026 P. David Buchan (pdbuchan@gmail.com)

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

#include "pgs.h"

// Parse Segment Header.
int
parse_header (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, FILE *fo) {

  uint16_t magic_number;

  if (!state->prescan) fprintf (fo, "Segment Header:\n");

  // A PGS segment header is 13 bytes: magic (2), PTS (4), DTS (4), type (1), size (2).
  if (*index > suplen || (suplen - *index) < 13) {
    fprintf (stderr, "Unexpectedly reached end of segment header in parse_header().\n");
    exit (EXIT_FAILURE);
  }

  // Magic Number (2 bytes), should be 0x5047 ("PG").
  magic_number = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
  if (magic_number != 0x5047) {
    fprintf (stderr, "Found invalid Magic Number of 0x%04x in Header. Should be 0x5047.\n", magic_number);
    fprintf (stderr, "Index of sup file is %zu\n", *index);
    exit (EXIT_FAILURE);
  }
  if (!state->prescan) fprintf (fo, "  Magic Number (2 bytes): 0x%04x\n", magic_number);
  *index += 2;

  // Presentation Timestamp (PTS), 90-kHz ticks.
  head->pts_offset = *index;
  head->pts_ticks =
    ((uint32_t) sup[*index] << 24) |
    ((uint32_t) sup[*index + 1] << 16) |
    ((uint32_t) sup[*index + 2] << 8) |
    (uint32_t) sup[*index + 3];
  head->pts.totalms = (int64_t) ((head->pts_ticks + 45u) / 90u);
  mstotime (&head->pts);
  if (!state->prescan) {
    fprintf (fo, "  Presentation Timestamp (PTS) (4 bytes): %02d:%02d:%02d,%03d\n",
             head->pts.h, head->pts.m, head->pts.s, head->pts.ms);
  }
  *index += 4;
  state->pts = head->pts;

  // Decoding Timestamp (DTS), 90-kHz ticks.
  head->dts_offset = *index;
  head->dts_ticks =
    ((uint32_t) sup[*index] << 24) |
    ((uint32_t) sup[*index + 1] << 16) |
    ((uint32_t) sup[*index + 2] << 8) |
    (uint32_t) sup[*index + 3];
  head->dts.totalms = (int64_t) ((head->dts_ticks + 45u) / 90u);
  mstotime (&head->dts);
  if (!state->prescan) {
    fprintf (fo, "  Decoding Timestamp (DTS) (4 bytes): %02d:%02d:%02d,%03d\n",
             head->dts.h, head->dts.m, head->dts.s, head->dts.ms);
  }
  *index += 4;

  // Segment Type (1 byte).
  head->segment_type = sup[*index];
  switch (head->segment_type) {
    case 0x14:
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x14 = Palette Definition Segment (PDS)\n");
      break;
    case 0x15:
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x15 = Object Definition Segment (ODS)\n");
      break;
    case 0x16:
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x16 = Presentation Composition (\"Control\") Segment (PCS)\n");
      break;
    case 0x17:
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x17 = Window Definition Segment (WDS)\n");
      break;
    case 0x80:
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x80 = End of Display Segment (END)\n");
      break;
    default:
      fprintf (stderr, "Unknown Segment Type value: 0x%02x\n", head->segment_type);
      exit (EXIT_FAILURE);
  }
  (*index)++;

  // Segment Size (2 bytes).
  head->segment_size = ((size_t) sup[*index] << 8) | sup[*index + 1];
  *index += 2;

  if (head->segment_size > (suplen - *index)) {
    fprintf (stderr, "Segment at file offset %zu declares %zu payload bytes, but only %zu remain.\n",
             *index - 13, head->segment_size, suplen - *index);
    exit (EXIT_FAILURE);
  }
  head->segment_end = *index + head->segment_size;

  if (!state->prescan) {
    fprintf (fo, "  Segment Size (2 bytes): %zu bytes", head->segment_size);
    if (head->segment_type == 0x80) fprintf (fo, "\n");
  }

  return (EXIT_SUCCESS);
}
