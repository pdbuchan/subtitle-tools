/*  Copyright (C) 2025-2026 P. David Buchan (pdbuchan@gmail.com)

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

#include "sub.h"

// Extract subtitles from .sub file.
int
extract_subs (uint8_t *subdata, size_t subdatalen, OPTIONS *options, IDX *idx, PES *pes_info, FILE *fo) {

  size_t lang, timestamp, spu_buffer_size;
  uint8_t *img_buffer, *spu_buffer;
  SPU_PARMS spu_info;
  SUB sub;

  // Allocate memory for various arrays.
  spu_buffer_size = (size_t) MAX_SPU_SIZE + 2;  // Add 2 padding bytes to prevent seg-fault in get_16bits().
  spu_buffer = allocate_u8mem ((int) spu_buffer_size);
  img_buffer = allocate_u8mem (IMG_BUFFER_SIZE);  // Will contain unpacked RGBA values.

  fprintf (stdout, "\n");

  // Loop through each language of subtitles.
  for (lang = 0; lang < idx->n_id; lang++) {

    fprintf (stdout, "Processing subtitles for language: %s%zu\n", idx->id[lang], idx->id_index[lang]);

    // Loop through each timestamp/offset (i.e., subtitle) for current language.
    for (timestamp = 0; timestamp < idx->n_timestamps[lang]; timestamp++) {

      fprintf (fo, "\nSUBTITLE %zu for Language ID: %zu (%s)\n\n", timestamp, idx->id_index[lang], idx->id[lang]);

      // Parse all MPEG-2 packets needed to compose one complete Subpicture Unit (SPU).
      // Store complete SPU in spu_buffer array.
      parse_packets (options, subdata, subdatalen, spu_buffer, timestamp, idx, lang, pes_info, &sub, fo);

      // Clear SPU parameters.
      memset (&spu_info, 0, sizeof (SPU_PARMS));

      // Parse Subpicture Unit (SPU).
      parse_spu (options, spu_buffer, idx, pes_info, &spu_info, &sub, fo);

      // Make bitmaps of subtitles if requested.
      if (options->makebmp_flag) {

        // Clear image buffer.
        memset (img_buffer, 0u, IMG_BUFFER_SIZE * sizeof (uint8_t));

        // Unpack RLE-encoded subpicture pixel data (PXD).
        unpack_pxd (spu_buffer, spu_buffer_size, &spu_info, idx, img_buffer, &sub);

        // Ensure end time is later than start time. This can occur for last subtitle
        // if Stop Display (STP_DSP) isn't included in last SPU. In this case, add a nominal 5 seconds.
        if (sub.end.totalms <= sub.start.totalms) {
          sub.end.totalms = sub.start.totalms + 5000;
          mstotime (&sub.end);
        }

        // Create bitmap file.
        write_bmp (img_buffer, idx, lang, &sub);
      }

    }  // Next filepos offset (i.e., next subtitle)
  }  // Next language of subtitles

  fprintf (stdout, "\n");

  // Free allocated memory.
  free (spu_buffer);
  free (img_buffer);

  return (EXIT_SUCCESS);
}
