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
blend_pixel (uint8_t *dst, const PALETTE_ENTRY *src) {

  uint32_t sa = src->alpha;
  uint32_t da = dst[3];
  uint32_t inv_sa, oa, denom;

  if (sa == 0) return;
  if (sa == 255 || da == 0) {
    dst[0] = src->r;
    dst[1] = src->g;
    dst[2] = src->b;
    dst[3] = (uint8_t) sa;
    return;
  }

  inv_sa = 255u - sa;
  oa = sa + ((da * inv_sa + 127u) / 255u);
  if (oa == 0) return;

  denom = 255u * oa;
  dst[0] = (uint8_t) ((((uint32_t) src->r * sa * 255u) + ((uint32_t) dst[0] * da * inv_sa) + denom / 2u) / denom);
  dst[1] = (uint8_t) ((((uint32_t) src->g * sa * 255u) + ((uint32_t) dst[1] * da * inv_sa) + denom / 2u) / denom);
  dst[2] = (uint8_t) ((((uint32_t) src->b * sa * 255u) + ((uint32_t) dst[2] * da * inv_sa) + denom / 2u) / denom);
  dst[3] = (uint8_t) oa;
}

// Render the current PCS presentation using the currently selected palette.
// The resulting bitmap is the bounding box of all active composition objects.
int
render_subtitle (STATE *state, PALETTE *palette, OBJECT *object, SUB *sub) {

  size_t i, min_x = SIZE_MAX, min_y = SIZE_MAX, max_x = 0, max_y = 0;
  size_t out_width, out_height, required;

  if (state->num_objects == 0) return (EXIT_SUCCESS);
  if (state->current_palette >= MAX_PALETTES) {
    fprintf (stderr, "Invalid current palette in render_subtitle().\n");
    exit (EXIT_FAILURE);
  }

  // Determine and validate the displayed rectangle for each object.
  for (i = 0; i < state->num_objects; i++) {
    COMPOSITION_OBJECT *ref = &state->composition_object[i];
    OBJECT *obj = &object[ref->object_id];
    size_t src_x, src_y, width, height;

    if (!obj->complete || obj->pixels == NULL) {
      fprintf (stderr, "PCS references incomplete or undefined object 0x%04x in render_subtitle().\n", ref->object_id);
      exit (EXIT_FAILURE);
    }

    if (ref->composition_flag & 0x80) {
      src_x = ref->crop_x;
      src_y = ref->crop_y;
      width = ref->crop_width;
      height = ref->crop_height;
      if (src_x > obj->width || src_y > obj->height ||
          width > obj->width - src_x || height > obj->height - src_y) {
        fprintf (stderr, "PCS crop rectangle lies outside object 0x%04x (%zux%zu).\n",
                 ref->object_id, obj->width, obj->height);
        exit (EXIT_FAILURE);
      }
    } else {
      width = obj->width;
      height = obj->height;
    }

    if ((size_t) ref->x + width > state->video_width || (size_t) ref->y + height > state->video_height) {
      fprintf (stderr, "Displayed object 0x%04x exceeds the %ux%u video frame.\n",
               ref->object_id, state->video_width, state->video_height);
      exit (EXIT_FAILURE);
    }

    if (ref->x < min_x) min_x = ref->x;
    if (ref->y < min_y) min_y = ref->y;
    if ((size_t) ref->x + width > max_x) max_x = (size_t) ref->x + width;
    if ((size_t) ref->y + height > max_y) max_y = (size_t) ref->y + height;
  }

  if (min_x == SIZE_MAX || max_x <= min_x || max_y <= min_y) {
    fprintf (stderr, "Unable to determine subtitle bitmap bounds in render_subtitle().\n");
    exit (EXIT_FAILURE);
  }

  out_width = max_x - min_x;
  out_height = max_y - min_y;
  if (out_width > SIZE_MAX / out_height || out_width * out_height > SIZE_MAX / 4u) {
    fprintf (stderr, "Subtitle bitmap dimensions overflow size_t in render_subtitle().\n");
    exit (EXIT_FAILURE);
  }
  required = out_width * out_height * 4u;

  if (required > sub->buffer_size) {
    void *tmp = realloc (sub->buffer, required);
    if (tmp == NULL) {
      fprintf (stderr, "Cannot allocate %zu-byte subtitle bitmap in render_subtitle().\n", required);
      exit (EXIT_FAILURE);
    }
    sub->buffer = tmp;
    sub->buffer_size = required;
  }
  memset (sub->buffer, 0, required);
  sub->width = out_width;
  sub->height = out_height;

  // Composite the objects in PCS order.
  for (i = 0; i < state->num_objects; i++) {
    COMPOSITION_OBJECT *ref = &state->composition_object[i];
    OBJECT *obj = &object[ref->object_id];
    size_t src_x = 0, src_y = 0, width = obj->width, height = obj->height;
    size_t x, y;

    if (ref->composition_flag & 0x80) {
      src_x = ref->crop_x;
      src_y = ref->crop_y;
      width = ref->crop_width;
      height = ref->crop_height;
    }

    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        size_t src_index = (src_y + y) * obj->width + (src_x + x);
        size_t dst_x = (size_t) ref->x - min_x + x;
        size_t dst_y = (size_t) ref->y - min_y + y;
        size_t dst_index = (dst_y * out_width + dst_x) * 4u;
        uint8_t palette_index = obj->pixels[src_index];

        blend_pixel (&sub->buffer[dst_index], &palette[state->current_palette].entry[palette_index]);
      }
    }
  }

  return (EXIT_SUCCESS);
}
