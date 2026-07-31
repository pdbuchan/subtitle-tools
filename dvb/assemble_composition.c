/*  Copyright (C) 2026 P. David Buchan (pdbuchan@gmail.com)

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

#include "dvb.h"

// Assemble Display Set composition from page_id's regions, objects, and cluts.
int
assemble_composition (STATE *state, PAGE **page) {

  int temp, top, bottom, left, right, region_x, region_y, object_x, object_y, object_width, object_height, width, height;
  size_t old_size, page_idx, region_idx, region_pos_idx, object_idx, page_object_idx, clut_idx, y, row_bytes, source_idx, object_buffer_idx, w, h;
  size_t composition_x, composition_y, destination_y;
  uint8_t region_id, depth, clut_id, entry, *final_composition, *source, *destination;
  uint16_t object_id;
  REGION *reg;
  RGBA rgba;
  void *tmp;

  // Find page index for state->page_id.
  temp = find_page_index (state, *page, state->page_id);
  if (temp < 0) {
    fprintf (stderr, "Cannot find index for state->page_id: 0x%04x in assemble_composition().\n", state->page_id);
    exit (EXIT_FAILURE);
  } else {
    page_idx = (size_t) temp;
  }

  // Pixel position values increase as you move down the composition, and as you move left to right.
  // PCS regions are the (region_id, region_horizontal_address, region_vertical_address) data. These are the regions to be displayed.
  // RCS regions are the (region_id, object list) data. None, some, or all of these may be displayed.
  top = INT_MAX;  // Position on page of highest object out of all regions. Start at unrealistically low position.
  bottom = INT_MIN;  // Position on page of lowest edge of lowest object of all regions. Start at highest possible position.
  left = INT_MAX;  // Position on page of leftmost object out of all regions. Start at unrealistically right position.
  right = INT_MIN;  // Position on page of rightmost object out of all regions. Start at leftmost possible position.

  // To build composition, we loop through all PCS regions on this page, adding each region's objects.
  // We will do this twice: Once first to determine Display Set dimensions in order to allocate memory, then again to copy objects to final composition.
  for (region_pos_idx = 0; region_pos_idx < (*page)[page_idx].nregion_pos; region_pos_idx++) {  // Index of page[page_idx].region_pos array (visible regions)

    // We can't guarantee region_pos has same index order as region array.
    // We'll look for region_id in region array and find its index there.
    region_id = (*page)[page_idx].region_pos[region_pos_idx].region_id;
    temp = find_region_index (state, *page, region_id);
    if (temp < 0) {
      fprintf (stderr, "Cannot find region_id: 0x%04x in page[page_id].region array in assemble_composition().\n", region_id);
      exit (EXIT_FAILURE);
    } else {
      region_idx = (size_t) temp;
    }

    // Region positions within page.
    region_x = (*page)[page_idx].region_pos[region_pos_idx].region_horizontal_address;
    region_y = (*page)[page_idx].region_pos[region_pos_idx].region_vertical_address;

    reg = &(*page)[page_idx].region[region_idx];

    // Loop through all objects in current region.
    for (object_idx = 0; object_idx < reg->nobjects; object_idx++) {  // Index of page[page_id].region[region_id].object_pos array

      // We can't guarantee page[page_id].region[region_id].object_pos array has same index order as page[page_id].object array.
      // We'll look up object_id in page[page_id].object array and find its index.
      object_id = reg->object_pos[object_idx].object_id;
      temp = find_object_index (state, *page, object_id);
      if (temp < 0) {
        fprintf (stderr, "Cannot find object_id:0x%04x in page[page_id].object array in assemble_composition().\n", object_id);
        exit (EXIT_FAILURE);
      } else {
        page_object_idx = (size_t) temp;
      }

      // Object width and height.
      object_width  = (*page)[page_idx].object[page_object_idx].width;
      object_height = (*page)[page_idx].object[page_object_idx].height;

      // Object positions within page.
      object_x = region_x + reg->object_pos[object_idx].horizontal_position;
      object_y = region_y + reg->object_pos[object_idx].vertical_position;

      // Guard against zero-width or height objects.
      if ((object_width <= 0) || (object_height <= 0))  continue;

      if (object_x < left) left = object_x;
      if (object_y < top)  top  = object_y;
      if ((object_x + object_width)  > right)  right  = object_x + object_width;
      if ((object_y + object_height) > bottom) bottom = object_y + object_height;

    }  // Next object
  }  // Next region

  // Nothing to render.
  if (left == INT_MAX) {
    return (EXIT_FAILURE);
  }

  // Save dimensions.
  (*page)[page_idx].width = (size_t) (right - left);
  width = (*page)[page_idx].width;
  (*page)[page_idx].height = (size_t) (bottom - top);
  height = (*page)[page_idx].height;

  // Allocate memory for RGBA buffer of final composition.
  final_composition = allocate_u8mem ((right - left) * (bottom - top) * 4);  // width * height * 4

  // Allocate memory for array 'source'.
  source = allocate_u8mem (IMG_BUFFER_SIZE);

  // Add all regions and objects to final composition.
  for (region_pos_idx = 0; region_pos_idx < (*page)[page_idx].nregion_pos; region_pos_idx++) {  // Index of page[page_idx].region_pos array (visible regions)
  
    // We can't guarantee region_pos has same index order as region array.
    // We'll look for region_id in region array and find its index there.
    region_id = (*page)[page_idx].region_pos[region_pos_idx].region_id;
    temp = find_region_index (state, *page, region_id);
    if (temp < 0) {
      fprintf (stderr, "Cannot find region_id: 0x%04x in page[page_id].region array.\n", region_id);
      exit (EXIT_FAILURE);
    } else {
      region_idx = (size_t) temp;
    }

    // Retrieve region's pixel depth.
    depth = (*page)[page_idx].region[region_idx].depth;

    // Retrieve clut_id for this region.
    clut_id = (*page)[page_idx].region[region_idx].clut_id;

    // Find clut index for clut_id.
    temp = find_clut_index (state, *page, clut_id);

    // If we fail to find clut_id, we'll create a default clut with that clut_id.
    if (temp < 0) {
      old_size = (*page)[page_idx].ncluts;
      clut_idx = (*page)[page_idx].ncluts;  // Note it's a 0-based array.
      tmp = (CLUT_FAMILY *) realloc ((*page)[page_idx].clut, (old_size + 1) * sizeof (CLUT_FAMILY));
      if (tmp != NULL) {
        (*page)[page_idx].clut = tmp;
      } else {
        fprintf (stderr, "Cannot allocate memory for page[%zu].clut[%zu] in assemble_composition().\n", page_idx, clut_idx);
        fprintf (stderr, "page_id: 0x%04x, clut_id: 0x%02x\n", state->page_id, clut_id);
        exit (EXIT_FAILURE);
      }
      memset (&(*page)[page_idx].clut[old_size], 0, sizeof (CLUT_FAMILY));  // Clear only new elements.
      (*page)[page_idx].clut[clut_idx].clut_id = clut_id;
      (*page)[page_idx].ncluts++;

      // Initialize CLUT family, assigned clut_id, with default CLUTs.
      // Incoming CLUT Definition Sections can overwrite none, some, or all of the default CLUT entries.
      initialize_clut_family (state, *page, clut_idx);

    // Found existing CLUT family.
    } else {
      clut_idx = temp;
    }

    // Region positions within page.
    region_x = (*page)[page_idx].region_pos[region_pos_idx].region_horizontal_address;
    region_y = (*page)[page_idx].region_pos[region_pos_idx].region_vertical_address;

    reg = &(*page)[page_idx].region[region_idx];

    // Loop through all objects in current region.
    for (object_idx = 0; object_idx < reg->nobjects; object_idx++) {  // Index of page[page_id].region[region_id].object_pos array

      // We can't guarantee page[page_id].region[region_id].object_pos array has same index order as page[page_id].object array.
      // We'll look up object_id in page[page_id].object array and find its index.
      object_id = reg->object_pos[object_idx].object_id;
      temp = find_object_index (state, *page, object_id);
      if (temp < 0) {
        fprintf (stderr, "Cannot find object_id:0x%04x in page[page_id].object array.\n", object_id);
        exit (EXIT_FAILURE);
      } else {
        page_object_idx = (size_t) temp;
      }

      // Object width and height.
      object_width  = (*page)[page_idx].object[page_object_idx].width;
      object_height = (*page)[page_idx].object[page_object_idx].height;

      // Render object to image buffer using appropriate RGBA colors based upon clut_id for this region.
      memset (source, 0, IMG_BUFFER_SIZE);
      source_idx = 0;  // Index of buffer 'source' where we will put RGBA data for object.
      object_buffer_idx = 0;  // Index of object buffer containing CLUT entry values; prepared by parse_ods().
      for (h = 0; h < object_height; h++) {
        for (w = 0; w < object_width; w++) {

          // Retrieve pixel CLUT entry index.
          entry = mask_entry ((*page)[page_idx].object[page_object_idx].buffer[object_buffer_idx], depth);

          // Handle non-modifying color flag: If set, CLUT Entry 1 is non-modifying.
          // Since it applies only to Entry 1, this must be evaluated on a per-pixel basis.
          // Also, color index 0 is background. i.e., transparent
          if (!((*page)[page_idx].object[page_object_idx].non_modifying_colour_flag && (entry == 1))) {

            // If 2, 4, or 8-bit CLUT is customized but CLUT with required depth is default,
            // use map table or reduction formula to obtain same customized palette with required depth.
            rgba = resolve_clut_color (&(*page)[page_idx].clut[clut_idx], depth, entry);

            // Add the pixel to the object's buffer.
            source[source_idx + 0] = rgba.r;  // R
            source[source_idx + 1] = rgba.g;  // G
            source[source_idx + 2] = rgba.b;  // B
            source[source_idx + 3] = rgba.a;  // Alpha

          }  // End if !non_modifying_colour_flag

          object_buffer_idx++;
          source_idx += 4;

        }  // Next w
      }  // Next h

      // Object positions within page.
      object_x = region_x + reg->object_pos[object_idx].horizontal_position;
      object_y = region_y + reg->object_pos[object_idx].vertical_position;

      composition_x = object_x - left;
      composition_y = object_y - top;

      for (y = 0; y < object_height; y++) {

        destination_y = composition_y + y;
        if ((destination_y < 0) || (destination_y >= height)) continue;

        destination = final_composition + (((size_t) destination_y * width) + composition_x) * 4;

        row_bytes = (size_t) object_width * 4;

        // Copy row of object to final composition.
        memcpy (destination, source + ((size_t) y * object_width * 4), row_bytes * sizeof (uint8_t));

      }  // Next object y
    }  // Next object
  }  // Next region

  // Render bitmap of final composition.
  write_bmp (state, *page, final_composition);

  // Free allocated memory.
  free (source);
  free (final_composition);

  return (EXIT_SUCCESS);
}
