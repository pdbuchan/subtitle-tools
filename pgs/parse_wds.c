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

// Parse Window Definition Segment (WDS)
int
parse_wds (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, FILE *fo) {

  size_t i;
  uint8_t nwin;
  uint16_t window_horizontal_position, window_vertical_position, window_width, window_height;

  if (!state->prescan) fprintf (fo, "\nWindow Definition Segment (WDS):\n");

  // Number of Windows (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_wds().\n");
    exit (EXIT_FAILURE);
  }
  nwin = sup[(*index)];
  if (!state->prescan) fprintf (fo, "  Number of windows defined in this segment (1 byte): %u\n", nwin);
  (*index)++;

  // Loop through window definitions.
  for (i = 0; i < (size_t) nwin; i++) {

    // Window ID (1 byte)
    if ((*index) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_wds().\n");
      exit (EXIT_FAILURE);
    }
    if (!state->prescan) fprintf (fo, "    Window ID (1 byte): 0x%02x\n", sup[(*index)]);
    (*index)++;

    // Window Horizontal Position (2 bytes)
    // X offset from the top left pixel of the window in the screen.
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_wds().\n");
      exit (EXIT_FAILURE);
    }
    window_horizontal_position = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Window Horizontal Position (2 bytes): %u px\n", window_horizontal_position);
    (*index) += 2;

    // Window Vertical Position (2 bytes)
    // Y offset from the top left pixel of the window in the screen.
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_wds().\n");
      exit (EXIT_FAILURE);
    }
    window_vertical_position = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Window Vertical Position (2 bytes): %u px\n", window_vertical_position);
    (*index) += 2;

    // Window Width (2 bytes)
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_wds().\n");
      exit (EXIT_FAILURE);
    }
    window_width = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Window Width (2 bytes): %u px\n", window_width);
    (*index) += 2;

    // Window Height (2 bytes)
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_wds().\n");
      exit (EXIT_FAILURE);
    }
    window_height = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Window Height (2 bytes): %u px\n", window_height);
    (*index) += 2;

  }  // Next window definition

  return (EXIT_SUCCESS);
}
