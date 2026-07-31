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

// Parse Palette Definition Segment (PDS)
int
parse_pds (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, PALETTE *palette, FILE *fo) {

  size_t offset;
  int rgb[3];
  uint8_t palette_id, palette_version, entry_id, y, cr, cb;

  if (!state->prescan) fprintf (fo, "\nPalette Definition Segment (PDS):\n");

  offset = 0;  // Index of this segment

  // Palette ID (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pds().\n");
    exit (EXIT_FAILURE);
  }
  palette_id = sup[(*index)];
  if (palette_id >= MAX_PALETTES) {
    fprintf (stderr, "Palette ID (1 byte): 0x%02x is >= MAX_PALETTES in pds().\n", palette_id);
    exit (EXIT_FAILURE);
  }
  state->current_palette = palette_id;
  if (!state->prescan) fprintf (fo, "  Palette ID (1 byte): 0x%02x\n", palette_id);
  (*index)++;
  offset++;

  // Palette Version Number (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pds().\n");
    exit (EXIT_FAILURE);
  }
  palette_version = sup[(*index)];
  palette[palette_id].version = palette_version;
  if (!state->prescan) fprintf (fo, "  Palette Version Number (1 byte): 0x%02x\n", palette_version);
  (*index)++;
  offset++;

  // Loop through all Palette entries
  while ((offset + 5) <= head->segment_size) {

    // Palette Entry ID (1 byte)
    if ((*index) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pds().\n");
      exit (EXIT_FAILURE);
    }
    entry_id = sup[(*index)];
    if (entry_id >= MAX_PALETTE_ENTRIES) {
      fprintf (stderr, "Palette Entry ID (1 byte): 0x%02x >= MAX_PALETTE_ENTRIES in pds().\n", entry_id);
      exit (EXIT_FAILURE);
    }
    if (!state->prescan) fprintf (fo, "    Palette Entry ID (1 byte): 0x%02x\n", entry_id);
    (*index)++;
    offset++;

    // Luminance (Y) (1 byte)
    // 16 <= Y <= 235
    if ((*index) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pds().\n");
      exit (EXIT_FAILURE);
    }
    y = sup[(*index)];
    if (!state->prescan) fprintf (fo, "      Luminance (Y) (1 byte): %u\n", y);
    (*index)++;
    offset++;

    // Color Difference Red (Cr) (1 byte)
    // 16 <= Cr <= 240
    if ((*index) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pds().\n");
      exit (EXIT_FAILURE);
    }
    cr = sup[(*index)];
    if (!state->prescan) fprintf (fo, "      Color Difference Red (Cr) (1 byte): %u\n", cr);
    (*index)++;
    offset++;

    // Color Difference Blue (Cb) (1 byte)
    // 16 <= Cb <= 240
    if ((*index) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pds().\n");
      exit (EXIT_FAILURE);
    }
    cb = sup[(*index)];
    if (!state->prescan) fprintf (fo, "      Color Difference Blue (Cb) (1 byte): %u\n", cb);
    (*index)++;
    offset++;

    // Convert YCbCr to RGB and save to palette struct.
    YCbCr2RGB_bt709 (y, cb, cr, rgb);
    palette[palette_id].entry[entry_id].r = rgb[0];
    palette[palette_id].entry[entry_id].g = rgb[1];
    palette[palette_id].entry[entry_id].b = rgb[2];

    // Transparency (Alpha) (1 byte)
    if ((*index) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pds().\n");
      exit (EXIT_FAILURE);
    }
    palette[palette_id].entry[entry_id].alpha = sup[(*index)];
    if (!state->prescan) fprintf (fo, "      Transparency (Alpha) (1 byte): %u\n", palette[palette_id].entry[entry_id].alpha);
    (*index)++;
    offset++;

  }  // Next Palette entry

  return (EXIT_SUCCESS);
}
