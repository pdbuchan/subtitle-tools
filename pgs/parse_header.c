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

// Parse Segment Header
int
parse_header (STATE *state, uint8_t *sup, size_t suplen, size_t *index, OPTIONS *options, HEAD *head, SYNC *sync, FILE *fo) {

  int64_t temp;
  double ratio, scaled_start;
  uint32_t magic_number, pts_ticks, pts_ms, dts_ticks, dts_ms;

  if (!state->prescan) fprintf (fo, "Segment Header:\n");

  // Magic Number (2 bytes)
  // Should be: 0x5047
  if (((*index) + 1) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment header in parse_header().\n");
    exit (EXIT_FAILURE);
  }
  magic_number = ((uint16_t) sup[*index] << 8) | sup[*index + 1];
  if (magic_number != 0x5047) {
    fprintf (stderr, "Found invalid Magic Number of 0x%04x in Header. Should be 20551 (0x5047).\n", magic_number);
    fprintf (stderr, "Index of sup file is %ld\n", *index);
    exit (EXIT_FAILURE);
  }
  if (!state->prescan) fprintf (fo, "  Magic Number (2 bytes): 0x%04x\n", magic_number);
  (*index) += 2;

  // Compute scaling ratio for sync option.
  ratio = (((double) options->newlastms - (double) options->newfirstms) / ((double) options->oldlastms - (double) options->oldfirstms));

  // Presentation Timestamp (PTS) (4 bytes)
  // PTS is the deadline for display of the window, which could contain a subtitle (Composition Object) or be blank (clearing a subtitle).
  // 90 kHz means /90 to get ms.
  if (((*index) + 3) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment header in parse_header().\n");
    exit (EXIT_FAILURE);
  }
  pts_ticks =
     ((uint32_t) sup[*index]     << 24) |
     ((uint32_t) sup[(*index) + 1] << 16) |
     ((uint32_t) sup[(*index) + 2] <<  8) |
     ((uint32_t) sup[(*index) + 3]);

    // Convert to ms via integer math.
    pts_ms = (pts_ticks + 45) / 90;

    head->pts.totalms = (int64_t) pts_ms;
    mstotime (&head->pts);
    if (!state->prescan) fprintf (fo, "  Presentation Timestamp (PTS) (4 bytes): %02d:%02d:%02d,%03d\n", head->pts.h, head->pts.m, head->pts.s, head->pts.ms);

  // Apply offsets to PTS timestamp, if requested.
  if ((!state->prescan) && (options->offset_flag)) {
    temp = (int64_t) pts_ms + options->offset.totalms;
    if (temp < 0) temp = 0;
    pts_ms = (uint32_t) temp;
    pts_ticks = pts_ms * 90;

    // Record new PTS in changes array.
    record_u32_change (options, *index, pts_ticks);

  // Synchronize PTS timestamp, if requested.
  } else if ((!state->prescan) && (options->sync_flag)) {

    scaled_start = ((double) options->newfirstms) + ((((double) sync->start.totalms) - ((double) options->oldfirstms)) * ratio);

    // END timestamp; preserve duration.
    if (state->pts_type == PTS_END) {

      pts_ms = (uint32_t) (scaled_start + (double) (sync->end.totalms - sync->start.totalms));

    // START or MIDDLE; scale normally.
    } else {

      pts_ms = (uint32_t) (((double) options->newfirstms) + ((((double) pts_ms) - ((double) options->oldfirstms)) * ratio));
    }

    pts_ticks = pts_ms * 90;
    record_u32_change (options, *index, pts_ticks);
  }

  head->pts.totalms = (int64_t) pts_ms;
  mstotime (&head->pts);
  (*index) += 4;

  // Save current PTS.
  state->pts = head->pts;

  // Decoding Timestamp (DTS) (4 bytes)
  // DTS is the time at which decoding of the image should begin. This is to allow the media player time to
  // decode the image by the PTS deadline. For the purposes of extracting subtitle bitmaps, DTS doesn't matter.
  // 90 kHz means /90 to get ms.
  if (((*index) + 3) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment header in parse_header().\n");
    exit (EXIT_FAILURE);
  }
  dts_ticks =
    ((uint32_t) sup[*index]     << 24) |
    ((uint32_t) sup[(*index) + 1] << 16) |
    ((uint32_t) sup[(*index) + 2] <<  8) |
    ((uint32_t) sup[(*index) + 3]);

  // Convert to ms via integer math.
  dts_ms = (dts_ticks + 45) / 90;

  head->dts.totalms = (int64_t) dts_ms;
  mstotime (&head->dts);
  if (!state->prescan) fprintf (fo, "  Decoding Timestamp (DTS) (4 bytes): %02d:%02d:%02d,%03d\n", head->dts.h, head->dts.m, head->dts.s, head->dts.ms);

  // Apply offsets to DTS timestamp, if requested.
  if (options->offset_flag) {
    temp = (int64_t) dts_ms + options->offset.totalms;
    if (temp < 0) temp = 0;
    dts_ms = (uint32_t) temp;
    dts_ticks = dts_ms * 90;

    // Record new DTS in changes array.
    record_u32_change (options, *index, dts_ticks);

  // Synchronize DTS timestamp, if requested.
  } else if ((!state->prescan) && (options->sync_flag)) {

    scaled_start = ((double) options->newfirstms) + ((((double) dts_ms) - ((double) options->oldfirstms)) * ratio);

    // END (PTS) timestamp; preserve duration.
    if (state->pts_type == PTS_END) {

      dts_ms = (uint32_t) (scaled_start + (double) (sync->end.totalms - sync->start.totalms));

    // START or MIDDLE; scale normally.
    } else {

      dts_ms = (uint32_t) (((double) options->newfirstms) + ((((double) dts_ms) - ((double) options->oldfirstms)) * ratio));
    }

    dts_ticks = dts_ms * 90;
    record_u32_change (options, *index, dts_ticks);
  }

  head->dts.totalms = (int64_t) dts_ms;
  mstotime (&head->dts);
  (*index) += 4;

  // Segment Type (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment header in parse_header().\n");
    exit (EXIT_FAILURE);
  }
  head->segment_type = sup[(*index)];
  switch (head->segment_type) {

    case 20:  // 0x14 = PDS
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x14 = Palette Definition Segment (PDS)\n");
      break;

    case 21:  // 0x15 = ODS
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x15 = Object Definition Segment (ODS)\n");
      break;

    case 22:  // 0x16 = PCS
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x16 = Presentation Composition (\"Control\") Segment (PCS)\n");
      break;

    case 23:  // 0x17 = WDS
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x17 = Window Definition Segment (WDS)\n");
      break;

    case 128:  // 0x80 = END
      if (!state->prescan) fprintf (fo, "  Segment Type (1 byte): 0x80 = End of Display Segment (END)\n");
      break;

    default:
      fprintf (stderr, "Unknown Segment Type value: 0x%02x\n", head->segment_type);

    }
  (*index)++;

  // Segment Size (2 bytes)
  if (((*index) + 1) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment header in parse_header().\n");
    exit (EXIT_FAILURE);
  }
  head->segment_size =  ((uint16_t) sup[*index] << 8) | sup[*index + 1];
  if (!state->prescan) fprintf (fo, "  Segment Size (2 bytes): %zu bytes", head->segment_size);
  if ((!state->prescan) && (head->segment_type == 128)) fprintf (fo, "\n");

  (*index) += 2;

  return (EXIT_SUCCESS);
}
