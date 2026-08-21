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

static void
require_pcs_bytes (size_t index, size_t segment_end, size_t nbytes) {

  if (index > segment_end || nbytes > (segment_end - index)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
}

// Parse Presentation Composition Segment (PCS).
int
parse_pcs (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, OBJECT *object, PALETTE *palette, FILE *fo) {

  size_t i;
  uint8_t frame_rate, palette_id;
  uint16_t composition_number;
  double fps;

  (void) suplen;  // parse_header() has already validated head->segment_end against the file size.

  if (!state->prescan) fprintf (fo, "\nPresentation Composition Segment (PCS):\n");

  require_pcs_bytes (*index, head->segment_end, 11);

  // Video width (2 bytes).
  state->video_width = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
  if (!state->prescan) fprintf (fo, "  Video Width (2 bytes): %u px\n", state->video_width);
  *index += 2;

  // Video height (2 bytes).
  state->video_height = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
  if (!state->prescan) fprintf (fo, "  Video Height (2 bytes): %u px\n", state->video_height);
  *index += 2;
  if (state->video_width == 0 || state->video_height == 0) {
    fprintf (stderr, "Invalid zero video dimension in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }

  // Frame Rate (1 byte).
  frame_rate = sup[*index];
  fps = framerates (frame_rate);
  if (!state->prescan) {
    if (fps > 0.0) {
      fprintf (fo, "  Framerate ID (1 byte): 0x%02x (indicates %.3f fps)\n", frame_rate, fps);
    } else {
      fprintf (fo, "  Framerate ID (1 byte): 0x%02x (unknown/reserved)\n", frame_rate);
    }
  }
  (*index)++;

  // Composition Number (2 bytes).
  composition_number = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
  state->composition_number = composition_number;
  if (!state->prescan) fprintf (fo, "  Composition Number (2 bytes): %u (0x%04x)\n", composition_number, composition_number);
  *index += 2;

  // Composition State (1 byte).
  state->composition_state = sup[*index];
  switch (state->composition_state) {
    case 0x00:
      if (!state->prescan) fprintf (fo, "  Composition State (1 byte): 0x00 = Normal\n");
      break;
    case 0x40:
      if (!state->prescan) fprintf (fo, "  Composition State (1 byte): 0x40 = Acquisition\n");
      clear_palettes (palette);
      clear_objects (object);
      break;
    case 0x80:
      if (!state->prescan) fprintf (fo, "  Composition State (1 byte): 0x80 = Epoch Start\n");
      clear_palettes (palette);
      clear_objects (object);
      break;
    case 0xc0:
      if (!state->prescan) fprintf (fo, "  Composition State (1 byte): 0xc0 = Epoch Continue\n");
      clear_palettes (palette);
      clear_objects (object);
      break;
    default:
      fprintf (stderr, "Unknown Composition State value: 0x%02x\n", state->composition_state);
      exit (EXIT_FAILURE);
  }
  (*index)++;

  // Palette Update Flag (1 byte). Bit 7 is defined; remaining bits are reserved.
  if ((sup[*index] & 0x7f) != 0) {
    fprintf (stderr, "Reserved bits are set in PCS Palette Update Flag byte: 0x%02x\n", sup[*index]);
    exit (EXIT_FAILURE);
  }
  state->palette_update_flag = (sup[*index] & 0x80) != 0;
  if (!state->prescan) {
    fprintf (fo, "  Palette Update Flag (1 byte): 0x%02x = %s\n",
             sup[*index] & 0x80, state->palette_update_flag ? "True" : "False");
  }
  (*index)++;

  // Palette ID (1 byte).
  palette_id = sup[*index];
  if (palette_id >= MAX_PALETTES) {
    fprintf (stderr, "Palette ID 0x%02x is outside the supported PGS epoch range 0-%u in parse_pcs().\n",
             palette_id, MAX_PALETTES - 1);
    exit (EXIT_FAILURE);
  }
  state->current_palette = palette_id;
  if (!state->prescan) fprintf (fo, "  Palette ID (1 byte): 0x%02x\n", palette_id);
  (*index)++;

  // Number of Composition Objects (1 byte).
  state->num_objects = sup[*index];
  if (state->num_objects > MAX_COMPOSITION_OBJECTS) {
    fprintf (stderr, "PCS references %u composition objects; PGS supports at most %u.\n",
             state->num_objects, MAX_COMPOSITION_OBJECTS);
    exit (EXIT_FAILURE);
  }
  if (!state->prescan) {
    fprintf (fo, "  Number of Composition (\"Window Information\") Objects (1 byte): %u\n", state->num_objects);
  }
  (*index)++;

  memset (state->composition_object, 0, sizeof (state->composition_object));
  if ((!state->prescan) && state->num_objects > 0) fprintf (fo, "  Composition (\"Window Information\") Objects:\n");

  for (i = 0; i < (size_t) state->num_objects; i++) {
    COMPOSITION_OBJECT *ref = &state->composition_object[i];

    require_pcs_bytes (*index, head->segment_end, 8);

    ref->object_id = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
    if (!state->prescan) fprintf (fo, "    Object ID (2 bytes): 0x%04x\n", ref->object_id);
    *index += 2;

    ref->window_id = sup[*index];
    if (!state->prescan) fprintf (fo, "      Window ID (1 byte): 0x%02x\n", ref->window_id);
    (*index)++;

    ref->composition_flag = sup[*index];
    if ((ref->composition_flag & 0x3f) != 0) {
      fprintf (stderr, "Reserved bits are set in PCS composition flag: 0x%02x\n", ref->composition_flag);
      exit (EXIT_FAILURE);
    }
    if (!state->prescan) {
      fprintf (fo, "      Composition Flag (1 byte): 0x%02x", ref->composition_flag);
      if ((ref->composition_flag & 0xc0) == 0) fprintf (fo, " = Normal");
      if (ref->composition_flag & 0x80) fprintf (fo, " = Cropped");
      if (ref->composition_flag & 0x40) fprintf (fo, "%sForced", (ref->composition_flag & 0x80) ? ", " : " = ");
      fprintf (fo, "\n");
    }
    (*index)++;

    ref->x = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
    if (!state->prescan) fprintf (fo, "      Object Horizontal Position (2 bytes): %u px\n", ref->x);
    *index += 2;

    ref->y = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
    if (!state->prescan) fprintf (fo, "      Object Vertical Position (2 bytes): %u px\n", ref->y);
    *index += 2;

    if (ref->x > state->video_width || ref->y > state->video_height) {
      fprintf (stderr, "PCS object 0x%04x position (%u,%u) is outside video dimensions %ux%u.\n",
               ref->object_id, ref->x, ref->y, state->video_width, state->video_height);
      exit (EXIT_FAILURE);
    }

    // Cropping information follows when bit 7 is set. Bit 6 independently means forced display.
    if (ref->composition_flag & 0x80) {
      require_pcs_bytes (*index, head->segment_end, 8);

      ref->crop_x = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
      if (!state->prescan) fprintf (fo, "      Object Cropping Horizontal Position (2 bytes): %u px\n", ref->crop_x);
      *index += 2;

      ref->crop_y = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
      if (!state->prescan) fprintf (fo, "      Object Cropping Vertical Position (2 bytes): %u px\n", ref->crop_y);
      *index += 2;

      ref->crop_width = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
      if (!state->prescan) fprintf (fo, "      Object Cropping Width (2 bytes): %u px\n", ref->crop_width);
      *index += 2;

      ref->crop_height = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
      if (!state->prescan) fprintf (fo, "      Object Cropping Height (2 bytes): %u px\n", ref->crop_height);
      *index += 2;

      if (ref->crop_width == 0 || ref->crop_height == 0) {
        fprintf (stderr, "PCS object 0x%04x has a zero-sized crop rectangle.\n", ref->object_id);
        exit (EXIT_FAILURE);
      }
    }
  }

  if (*index != head->segment_end) {
    fprintf (stderr, "PCS parser consumed %zu bytes but segment ends at file offset %zu.\n",
             *index, head->segment_end);
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
