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
require_ods_bytes (size_t index, size_t segment_end, size_t nbytes) {

  if (index > segment_end || nbytes > (segment_end - index)) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
}

// Parse Object Definition Segment (ODS).
int
parse_ods (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, OBJECT *object, FILE *fo) {

  OBJECT *obj;
  size_t fragment_len, pixel_count;
  uint16_t object_id;
  uint8_t version, sequence_desc, sequence;

  (void) suplen;

  if (!state->prescan) fprintf (fo, "\nObject Definition Segment (ODS):\n");

  // Every fragment has Object ID, version, and sequence descriptor.
  require_ods_bytes (*index, head->segment_end, 4);

  object_id = (uint16_t) (((uint16_t) sup[*index] << 8) | (uint16_t) sup[*index + 1]);
  if (!state->prescan) fprintf (fo, "  Object ID (2 bytes): 0x%04x\n", object_id);
  *index += 2;

  version = sup[*index];
  if (!state->prescan) fprintf (fo, "  Object Version Number (1 byte): 0x%02x\n", version);
  (*index)++;

  sequence_desc = sup[*index];
  if ((sequence_desc & 0x3f) != 0) {
    fprintf (stderr, "Reserved bits are set in ODS sequence descriptor 0x%02x.\n", sequence_desc);
    exit (EXIT_FAILURE);
  }
  sequence = sequence_desc & 0xc0;
  if (!state->prescan) {
    switch (sequence) {
      case 0x00: fprintf (fo, "  Sequence Descriptor (1 byte): 0x00 = Continuation\n"); break;
      case 0x40: fprintf (fo, "  Sequence Descriptor (1 byte): 0x40 = Last in sequence\n"); break;
      case 0x80: fprintf (fo, "  Sequence Descriptor (1 byte): 0x80 = First in sequence\n"); break;
      case 0xc0: fprintf (fo, "  Sequence Descriptor (1 byte): 0xc0 = First and last in sequence\n"); break;
      default: break;
    }
  }
  (*index)++;

  obj = &object[object_id];

  if (sequence & 0x80) {
    uint32_t declared_length;
    void *tmp;

    // A first fragment additionally contains Object Data Length and object dimensions.
    require_ods_bytes (*index, head->segment_end, 7);

    declared_length = ((uint32_t) sup[*index] << 16) |
                      ((uint32_t) sup[*index + 1] << 8) |
                      (uint32_t) sup[*index + 2];
    *index += 3;
    if (declared_length < 4) {
      fprintf (stderr, "Invalid ODS Object Data Length %u; it must include four dimension bytes.\n", declared_length);
      exit (EXIT_FAILURE);
    }

    obj->width = ((size_t) sup[*index] << 8) | sup[*index + 1];
    *index += 2;
    obj->height = ((size_t) sup[*index] << 8) | sup[*index + 1];
    *index += 2;
    obj->version = version;
    obj->expected_length = (size_t) declared_length - 4u;
    obj->remaining_length = obj->expected_length;
    obj->length = 0;
    obj->complete = 0;

    if (!state->prescan) {
      fprintf (fo, "  Object Data Length (3 bytes): %u bytes (includes width and height)\n", declared_length);
      fprintf (fo, "  Width of the image (2 bytes): %zu px\n", obj->width);
      fprintf (fo, "  Height of the image (2 bytes): %zu px\n", obj->height);
    }

    if (obj->width == 0 || obj->height == 0 || obj->width > SIZE_MAX / obj->height) {
      fprintf (stderr, "Invalid ODS object dimensions %zux%zu.\n", obj->width, obj->height);
      exit (EXIT_FAILURE);
    }
    if (state->video_width != 0 && state->video_height != 0 &&
        (obj->width > state->video_width || obj->height > state->video_height)) {
      fprintf (stderr, "ODS object dimensions %zux%zu exceed video dimensions %ux%u.\n",
               obj->width, obj->height, state->video_width, state->video_height);
      exit (EXIT_FAILURE);
    }

    free (obj->buffer);
    obj->buffer = NULL;
    free (obj->pixels);
    obj->pixels = NULL;

    if (obj->expected_length > 0) {
      tmp = malloc (obj->expected_length);
      if (tmp == NULL) {
        fprintf (stderr, "Cannot allocate %zu bytes for ODS object 0x%04x.\n", obj->expected_length, object_id);
        exit (EXIT_FAILURE);
      }
      obj->buffer = tmp;
    }

  } else {
    // Continuation fragments do not repeat Object Data Length or dimensions.
    if (obj->buffer == NULL || obj->expected_length == 0 || obj->remaining_length == 0) {
      fprintf (stderr, "ODS continuation for object 0x%04x has no incomplete first fragment.\n", object_id);
      exit (EXIT_FAILURE);
    }
    if (obj->version != version) {
      fprintf (stderr, "ODS continuation version 0x%02x does not match first-fragment version 0x%02x for object 0x%04x.\n",
               version, obj->version, object_id);
      exit (EXIT_FAILURE);
    }
  }

  fragment_len = head->segment_end - *index;
  if (fragment_len > obj->remaining_length) {
    fprintf (stderr, "ODS fragment contains %zu RLE bytes but object 0x%04x has only %zu bytes remaining.\n",
             fragment_len, object_id, obj->remaining_length);
    exit (EXIT_FAILURE);
  }

  if (fragment_len > 0) memcpy (obj->buffer + obj->length, sup + *index, fragment_len);
  obj->length += fragment_len;
  obj->remaining_length -= fragment_len;
  *index = head->segment_end;

  if ((sequence & 0x40) != 0) {
    if (obj->remaining_length != 0) {
      fprintf (stderr, "Last ODS fragment for object 0x%04x is %zu RLE bytes shorter than declared.\n",
               object_id, obj->remaining_length);
      exit (EXIT_FAILURE);
    }

    pixel_count = obj->width * obj->height;
    obj->pixels = malloc (pixel_count);
    if (obj->pixels == NULL) {
      fprintf (stderr, "Cannot allocate %zu decoded pixels for object 0x%04x.\n", pixel_count, object_id);
      exit (EXIT_FAILURE);
    }
    decode_rle (obj->buffer, obj->length, obj->width, obj->height, obj->pixels);
    obj->complete = 1;
  } else if (obj->remaining_length == 0) {
    fprintf (stderr, "ODS object 0x%04x is complete, but the current fragment is not marked last.\n", object_id);
    exit (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
