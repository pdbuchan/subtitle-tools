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

// Parse Palette Definition Segment (PDS).
int
parse_pds (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, PALETTE *palette, FILE *fo) {

  int rgb[3];
  uint8_t palette_id, palette_version, entry_id, y, cr, cb;

  (void) suplen;

  if (!state->prescan) fprintf (fo, "\nPalette Definition Segment (PDS):\n");

  if (*index > head->segment_end || (head->segment_end - *index) < 2) {
    fprintf (stderr, "PDS is too short in parse_pds().\n");
    exit (EXIT_FAILURE);
  }
  if (((head->segment_end - *index) - 2) % 5 != 0) {
    fprintf (stderr, "PDS payload after its two-byte header is not a multiple of five bytes.\n");
    exit (EXIT_FAILURE);
  }

  // Palette ID (1 byte).
  palette_id = sup[*index];
  if (palette_id >= MAX_PALETTES) {
    fprintf (stderr, "Palette ID 0x%02x is outside the supported PGS epoch range 0-%u in parse_pds().\n",
             palette_id, MAX_PALETTES - 1);
    exit (EXIT_FAILURE);
  }
  state->current_palette = palette_id;
  if (!state->prescan) fprintf (fo, "  Palette ID (1 byte): 0x%02x\n", palette_id);
  (*index)++;

  // Palette Version Number (1 byte).
  palette_version = sup[*index];
  palette[palette_id].version = palette_version;
  if (!state->prescan) fprintf (fo, "  Palette Version Number (1 byte): 0x%02x\n", palette_version);
  (*index)++;

  while (*index < head->segment_end) {
    // Each palette entry is exactly five bytes: ID, Y, Cr, Cb, Alpha.
    entry_id = sup[*index];
    y = sup[*index + 1];
    cr = sup[*index + 2];
    cb = sup[*index + 3];

    if (!state->prescan) {
      fprintf (fo, "    Palette Entry ID (1 byte): 0x%02x\n", entry_id);
      fprintf (fo, "      Luminance (Y) (1 byte): %u\n", y);
      fprintf (fo, "      Color Difference Red (Cr) (1 byte): %u\n", cr);
      fprintf (fo, "      Color Difference Blue (Cb) (1 byte): %u\n", cb);
    }

    YCbCr2RGB_bt709 (y, cb, cr, rgb);
    palette[palette_id].entry[entry_id].r = (uint8_t) rgb[0];
    palette[palette_id].entry[entry_id].g = (uint8_t) rgb[1];
    palette[palette_id].entry[entry_id].b = (uint8_t) rgb[2];
    palette[palette_id].entry[entry_id].alpha = sup[*index + 4];

    if (!state->prescan) {
      fprintf (fo, "      Transparency (Alpha) (1 byte): %u\n", palette[palette_id].entry[entry_id].alpha);
    }
    *index += 5;
  }

  return (EXIT_SUCCESS);
}
