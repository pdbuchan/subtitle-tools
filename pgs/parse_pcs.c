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

// Parse Presentation Composition Segment (PCS)
int
parse_pcs (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, OBJECT *object, PALETTE *palette, FILE *fo) {

  size_t i, offset;
  uint8_t frame_rate, palette_update_flag, palette_id, crop_flag;
  uint16_t video_width, video_height, composition_number, object_id, window_id, object_horizontal_position, object_vertical_position;
  uint16_t object_cropping_horizontal_position, object_cropping_vertical_position, object_cropping_width, object_cropping_height;

  if (!state->prescan) fprintf (fo, "\nPresentation Composition Segment (PCS):\n");

  offset = 0;  // Index of this segment

  // Video width (2 bytes)
  if (((*index) + 1) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  video_width = (sup[*index] << 8) | sup[(*index) + 1];
  if (!state->prescan) fprintf (fo, "  Video Width (2 bytes): %u px\n", video_width);
  (*index) += 2;
  offset += 2;

  // Video height (2 bytes)
  if (((*index) + 1) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  video_height = (sup[*index] << 8) | sup[(*index) + 1];
  if (!state->prescan) fprintf (fo, "  Video Height (2 bytes): %u px\n", video_height);
  (*index) += 2;
  offset += 2;

  // Frame Rate (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  frame_rate = sup[(*index)];
  if (!state->prescan) fprintf (fo, "  Framerate ID (1 byte): 0x%02x (indicates %.3lf fps)\n", frame_rate, framerates (frame_rate));
  (*index)++;
  offset++;

  // Composition Number (2 bytes)
  if (((*index) + 1) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  composition_number = (sup[*index] << 8) | sup[(*index) + 1];
  if (!state->prescan) fprintf (fo, "  Composition Number (2 bytes): %u (0x%04x)\n", composition_number, composition_number);
  (*index) += 2;
  offset += 2;

  // Composition State (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  state->composition_state = sup[(*index)];
  switch (state->composition_state) {

    case 0x00:  // Normal
      if (!state->prescan) fprintf (fo, "  Composition State (1 byte): 0x%02x = Normal\n", state->composition_state);
      break;

    case 0x40:  // Acquisition Point
      if (!state->prescan) fprintf (fo, "  Composition State (1 byte): 0x%02x = Acquisition\n", state->composition_state);

      // Clear palettes.
      clear_palettes (palette);
      break;

    case 0x80:  // Epoch Start
      if (!state->prescan) fprintf (fo, "  Composition State (1 byte): 0x%02x = Epoch Start\n", state->composition_state);

      // Clear palettes.
      clear_palettes (palette);

      // Clear objects.
      clear_objects (object);
      break;

    case 0xc0:  // Epoch Continue
      if (!state->prescan) fprintf (fo, "  Composition State (1 byte): 0x%02x = Epoch Continue\n", state->composition_state);
      break;

    default:
      fprintf (stderr, "Unknown Composition State value: 0x%02x\n", state->composition_state);
      exit (EXIT_FAILURE);

  }  // End switch
  (*index)++;
  offset++;

  // Palette Update Flag (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  palette_update_flag = sup[*index] & 0x80;
  state->palette_update_flag = palette_update_flag ? 1 : 0;
  if (!state->palette_update_flag) {
    if (!state->prescan) fprintf (fo, "  Palette Update Flag (1 byte): 0x%02x = False\n", palette_update_flag);   
  } else {
    if (!state->prescan) fprintf (fo, "  Palette Update Flag (1 byte): 0x%02x = True\n", palette_update_flag);
  }
  (*index)++;
  offset++;

  // Palette ID (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  palette_id = sup[(*index)];
  if (palette_id > MAX_PALETTES) {
    fprintf (stderr, "palette_id exceeds MAX_PALETTES in pcs().\n");
    exit (EXIT_FAILURE);
  }
  if (!state->prescan) fprintf (fo, "  Palette ID (1 byte): 0x%02x\n", palette_id);
  state->current_palette = sup[(*index)];
  (*index)++;
  offset++;

  // Number of Composition ("Window Information") Objects (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
    exit (EXIT_FAILURE);
  }
  state->num_objects = sup[(*index)];
  if (!state->prescan) fprintf (fo, "  Number of Composition (\"Window Information\") Objects (1 byte): %u\n", state->num_objects);
  (*index)++;
  offset++;

  // Composition ("Window Information") Objects
  if ((!state->prescan) && (state->num_objects > 0)) fprintf (fo, "  Composition (\"Window Information\") Objects:\n");
  for (i = 0; i < (size_t) state->num_objects; i++) {

    // Object ID (2 bytes)
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    object_id = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "    Object ID (2 bytes): 0x%04x\n", object_id);
    if (i == 0) state->object_id = object_id;  // Save first object_id.
    (*index) += 2;
    offset += 2;

    // Window ID (1 byte)
    if ((*index) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    window_id = sup[(*index)];
    if (!state->prescan) fprintf (fo, "      Window ID (1 byte): 0x%02x\n", window_id);
    (*index)++;
    offset++;

    // Object Cropped Flag (1 byte)
    if ((*index) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    crop_flag = sup[(*index)];
    switch (crop_flag) {

      case 0:
        if (!state->prescan) fprintf (fo, "      Object Cropped Flag (1 byte): 0x%02x = Off\n", crop_flag);
        break;

      case 64:  // 0x40
        if (!state->prescan) fprintf (fo, "      Object Cropped Flag (1 byte): 0x%02x = Force display of the cropped image object\n", crop_flag);
        break;

      default:
        fprintf (stderr, "      Unknown Object Cropped Flag value: 0x%02x\n", crop_flag);
        exit (EXIT_FAILURE);

    }
    (*index)++;
    offset++;

    // Object Horizontal Position (2 bytes)
    // X offset from the top-left pixel of the image on the screen
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    object_horizontal_position = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Object Horizontal Position (2 bytes): %u px\n", object_horizontal_position);
    (*index) += 2;
    offset += 2;

    // Object Vertical Position (2 bytes)
    // Y offset from the top-left pixel of the image on the screen
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    object_vertical_position = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Object Vertical Position (2 bytes): %u px\n", object_vertical_position);
    (*index) += 2;
    offset += 2;

    // No cropping for current object.
    if (crop_flag == 0) {
      continue;
    }

    if ((offset + 8) > head->segment_size) {
      fprintf (stderr, "Not enough bytes in segment for cropping data in pcs().\n");
      exit (EXIT_FAILURE);
    }

    // Object Cropping Horizontal Position (2 bytes)
    // X offset from the top left pixel of the cropped object in the
    // screen. Only used when the Object Cropped Flag is set to 0x40.
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    object_cropping_horizontal_position = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Object Cropping Horizontal Position (2 bytes): %u px\n", object_cropping_horizontal_position);
    (*index) += 2;
    offset += 2;

    // Object Cropping Vertical Position (2 bytes)
    // Y offset from the top left pixel of the cropped object in the
    // screen. Only used when the Object Cropped Flag is set to 0x40.
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    object_cropping_vertical_position = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Object Cropping Vertical Position (2 bytes): %u px\n", object_cropping_vertical_position);
    (*index) += 2;
    offset += 2;

    // Object Cropping Width (2 bytes)
    // Width of the cropped object in the screen. Only used when the
    // Object Cropped Flag is set to 0x40.
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    object_cropping_width = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Object Cropping Width (2 bytes): %u px\n", object_cropping_width);
    (*index) += 2;
    offset += 2;

    // Object Cropping Height (2 bytes)
    // Height of the cropped object in the screen. Only used when the
    // Object Cropped Flag is set to 0x40.
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_pcs().\n");
      exit (EXIT_FAILURE);
    }
    object_cropping_height = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "      Object Cropping Height (2 bytes): %u px\n", object_cropping_height);
    (*index) += 2;
    offset += 2;

  }  // Next Compositional Object

  return (EXIT_SUCCESS);
}
