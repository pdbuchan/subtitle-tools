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

// Parse Object Definition Segment (ODS)
int
parse_ods (STATE *state, uint8_t *sup, size_t suplen, size_t *index, HEAD *head, PALETTE *palette, OBJECT *object, SUB *sub, FILE *fo) {

  size_t i, segment_end_index;
  uint8_t object_version_number, seq_flag, seq;
  uint32_t object_length;
  void *tmp;

  if (!state->prescan) fprintf (fo, "\nObject Definition Segment (ODS):\n");

  segment_end_index = (*index) + head->segment_size;

  // Object ID (2 bytes)
  if (((*index) + 1) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
  state->object_id = (sup[*index] << 8) | sup[(*index) + 1];
  if (!state->prescan) fprintf (fo, "  Object ID (2 bytes): 0x%04x\n", state->object_id);
  (*index) += 2;

  // Allocate memory for RLE-encoded data buffer if not currently allocated for this object ID.
  if (object[state->object_id].buffer == NULL) {
    tmp = (uint8_t *) malloc (MAX_OBJECT_BUFFER_LEN * sizeof (uint8_t));
    if (tmp != NULL) {
      memset (tmp, 0, MAX_OBJECT_BUFFER_LEN * sizeof (uint8_t));
      object[state->object_id].buffer = tmp;
    } else {
      fprintf (stderr, "ERROR: Cannot allocate memory for object[state->object_id].buffer in ods().\n");
      exit (EXIT_FAILURE);
    }
    object[state->object_id].length = 0;
  }

  // Object Version Number (1 byte)
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
  object_version_number = sup[(*index)];
  if (!state->prescan) fprintf (fo, "  Object Version Number (1 byte): 0x%02x\n", object_version_number);
  (*index)++;

  // Last in Sequence Flag (1 byte)
  // US20090185789A1: In some cases, it isn't possible to store the decompressed graphics that
  // constitutes a subtitle into one ODS due to payload contraints of a PES packet.
  // In such cases, the graphics is split into a series of consecutive fragments.
  if ((*index) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
  seq_flag = sup[(*index)];
  seq = seq_flag & 0xc0;
  switch (seq) {

    case 64:  // 0x40 = Last in sequence
      state->seq_flag = 0;  // This is the last in a series of ODSs for one subtitle.
      if (!state->prescan) fprintf (fo, "  Last in Sequence Flag (1 byte): 0x40 = Last in sequence\n");
      break;

    case 128:  // 0x80 = First in sequence
      state->seq_flag = 1;  // We're now in a sequence of ODSs for one subtitle.
      if (!state->prescan) fprintf (fo, "  Last in Sequence Flag (1 byte): 0x80 = First in sequence\n");

      // Clear buffer for this new object.
      object[state->object_id].length = 0;
      memset (object[state->object_id].buffer, 0, MAX_OBJECT_BUFFER_LEN * sizeof (uint8_t));
      break;

    case 192:  // 0xc0 = First and last in sequence (0x40 | 0x80)
      state->seq_flag = 0;  // This is the only ODS for this subtitle.
      if (!state->prescan) fprintf (fo, "  Last in Sequence Flag (1 byte): 0xc0 = First and last in sequence\n");

      // Clear buffer for this new object.
      object[state->object_id].length = 0;
      memset (object[state->object_id].buffer, 0, MAX_OBJECT_BUFFER_LEN * sizeof (uint8_t));
      break;

    case 0x00:  // Continuation of sequence; more ODS segments will follow.
      if (!state->prescan) fprintf (fo, "  Last in Sequence Flag (1 byte): 0x%02x = Continuation of sequence\n", seq_flag);
      break;

    default:
      fprintf(stderr, "Invalid ODS sequence flag: 0x%02x in ods().\n", seq_flag);
      exit (EXIT_FAILURE);
      break;

  }
  (*index)++;

  // Object Data Length (3 bytes)
  // The length of the run-length encoding (RLE) data in the current segment.
  // Include width and height fields (4 bytes) if (seq_flag == 0x80) || (seq_flag == 0xc0).
  if (((*index) + 2) >= suplen) {
    fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
    exit (EXIT_FAILURE);
  }
  object_length = (sup[*index] << 16) | (sup[(*index) + 1] << 8) | sup[(*index) + 2];
  (*index) += 3;
  if (object_length < 4) {
    fprintf (stderr, "Invalid ODS object length: %u in ods().\n", object_length);
    exit (EXIT_FAILURE);
  }
  if (((*index) + object_length) > segment_end_index) {
    fprintf (stderr, "ODS object length exceeds segment size in ods().\n");
    exit (EXIT_FAILURE);
  }
  if (!state->prescan) fprintf (fo, "  Object Data Length for this segment (3 bytes): %u bytes\n", object_length);

  // Width and Height fields appear only in the first ODS of a sequence.
  if ((seq_flag == 0x80) || (seq_flag == 0xc0)) {

    // Width of the image (2 bytes)
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
      exit (EXIT_FAILURE);
    }
    sub->width = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "  Width of the image (2 bytes): %zu px\n", sub->width);
    (*index) += 2;

    // Height of the image (2 bytes)
    if (((*index) + 1) >= suplen) {
      fprintf (stderr, "Unexpectedly reached end of segment in parse_ods().\n");
      exit (EXIT_FAILURE);
    }
    sub->height = (sup[*index] << 8) | sup[(*index) + 1];
    if (!state->prescan) fprintf (fo, "  Height of the image (2 bytes): %zu px\n", sub->height);
    (*index) += 2;

    object_length -= 4;  // Subtract Width and Height field lengths (2 bytes each)
  }

  // Append RLE-compressed object data to object buffer.
  if (object[state->object_id].length + object_length > MAX_OBJECT_BUFFER_LEN) {
    fprintf (stderr, "RLE object buffer overflow for object 0x%04x in ods().\n", state->object_id);
    exit (EXIT_FAILURE);
  }
  for (i = 0; i < (size_t) object_length; i++) {
    object[state->object_id].buffer[object[state->object_id].length] = sup[(*index) + i];
    object[state->object_id].length++;
  }

  // Decode RLE-compressed object data if: last in sequence, or first and last in sequence.
  // Decode image by uncompressing to red, green, blue, and alpha (RGBA) per pixel, and saving in current subtitle's image buffer.
  if ((seq_flag == 0x40) || (seq_flag == 0xc0)) {
    decode_rle (state, palette, object[state->object_id].buffer, (size_t) object[state->object_id].length, sub->buffer);
  }

  (*index) += object_length;

  return (EXIT_SUCCESS);
}
