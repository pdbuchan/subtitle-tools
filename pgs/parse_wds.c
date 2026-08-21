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

// Parse Window Definition Segment (WDS).
int
parse_wds (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, FILE *fo) {

  size_t i, expected_size;
  uint8_t nwin;
  uint16_t x, y, width, height;

  (void) suplen;

  if (!state->prescan) fprintf (fo, "\nWindow Definition Segment (WDS):\n");

  if (*index >= head->segment_end) {
    fprintf (stderr, "WDS is too short in parse_wds().\n");
    exit (EXIT_FAILURE);
  }

  nwin = sup[*index];
  if (!state->prescan) fprintf (fo, "  Number of windows defined in this segment (1 byte): %u\n", nwin);
  (*index)++;

  expected_size = 1u + ((size_t) nwin * 9u);
  if (expected_size != head->segment_size) {
    fprintf (stderr, "WDS declares %u windows but its segment size is %zu bytes; expected %zu.\n",
             nwin, head->segment_size, expected_size);
    exit (EXIT_FAILURE);
  }

  for (i = 0; i < (size_t) nwin; i++) {
    uint8_t window_id;

    if ((head->segment_end - *index) < 9) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_wds().\n");
      exit (EXIT_FAILURE);
    }

    window_id = sup[*index];
    if (!state->prescan) fprintf (fo, "    Window ID (1 byte): 0x%02x\n", window_id);
    (*index)++;

    x = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
    if (!state->prescan) fprintf (fo, "      Window Horizontal Position (2 bytes): %u px\n", x);
    *index += 2;

    y = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
    if (!state->prescan) fprintf (fo, "      Window Vertical Position (2 bytes): %u px\n", y);
    *index += 2;

    width = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
    if (!state->prescan) fprintf (fo, "      Window Width (2 bytes): %u px\n", width);
    *index += 2;

    height = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
    if (!state->prescan) fprintf (fo, "      Window Height (2 bytes): %u px\n", height);
    *index += 2;

    if (width == 0 || height == 0) {
      fprintf (stderr, "Window 0x%02x has a zero dimension in parse_wds().\n", window_id);
      exit (EXIT_FAILURE);
    }
  }

  if (*index != head->segment_end) {
    fprintf (stderr, "WDS parser did not finish at the end of the segment.\n");
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
