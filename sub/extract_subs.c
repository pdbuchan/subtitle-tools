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

int
extract_subs (uint8_t *subdata, size_t subdatalen, OPTIONS *options,
              IDX *idx, PES *pes_info, FILE *fo) {

  size_t lang, timestamp, spu_buffer_size;
  uint8_t *img_buffer, *spu_buffer;
  SPU_PARMS spu_info;
  SUB sub;

  img_buffer = NULL;
  spu_buffer = NULL;
  memset (&spu_info, 0, sizeof (spu_info));
  memset (&sub, 0, sizeof (sub));

  fprintf (stdout, "\n");

  for (lang = 0; lang < idx->n_id; lang++) {
    fprintf (stdout, "Processing subtitles for language: %s%zu\n",
             idx->id[lang], idx->id_index[lang]);

    for (timestamp = 0; timestamp < idx->n_timestamps[lang]; timestamp++) {
      fprintf (fo, "\nSUBTITLE %zu for Language ID: %zu (%s)\n\n",
               timestamp, idx->id_index[lang], idx->id[lang]);

      free (spu_buffer);
      spu_buffer = NULL;
      spu_buffer_size = 0;
      memset (&sub, 0, sizeof (sub));

      if (parse_packets (options, subdata, subdatalen, &spu_buffer,
                         &spu_buffer_size, timestamp, idx, lang,
                         pes_info, &sub, fo) != EXIT_SUCCESS) {
        free_spu_parms (&spu_info);
        free (spu_buffer);
        free (img_buffer);
        return (EXIT_FAILURE);
      }

      if (parse_spu (spu_buffer, spu_buffer_size, idx, pes_info,
                     &spu_info, &sub, fo) != EXIT_SUCCESS) {
        free_spu_parms (&spu_info);
        free (spu_buffer);
        free (img_buffer);
        return (EXIT_FAILURE);
      }

      if (options->makebmp_flag) {
        if (unpack_pxd (spu_buffer, spu_buffer_size, &spu_info, idx,
                        &img_buffer, &sub) != EXIT_SUCCESS) {
          free_spu_parms (&spu_info);
          free (spu_buffer);
          free (img_buffer);
          return (EXIT_FAILURE);
        }

        // Last subtitles occasionally omit STP_DSP. Use a nominal five seconds.
        if (sub.end.totalms <= sub.start.totalms) {
          if (sub.start.totalms > INT64_MAX - 5000) {
            fprintf (stderr, "Subtitle fallback end timestamp overflow.\n");
            free_spu_parms (&spu_info);
            free (spu_buffer);
            free (img_buffer);
            return (EXIT_FAILURE);
          }
          sub.end.totalms = sub.start.totalms + 5000;
          if (mstotime (&sub.end) != EXIT_SUCCESS) {
            free_spu_parms (&spu_info);
            free (spu_buffer);
            free (img_buffer);
            return (EXIT_FAILURE);
          }
        }

        if (write_bmp (img_buffer, idx, lang, &sub) != EXIT_SUCCESS) {
          free_spu_parms (&spu_info);
          free (spu_buffer);
          free (img_buffer);
          return (EXIT_FAILURE);
        }
      }

      free_spu_parms (&spu_info);
      memset (&spu_info, 0, sizeof (spu_info));
    }
  }

  fprintf (stdout, "\n");
  free (spu_buffer);
  free (img_buffer);
  free_spu_parms (&spu_info);

  return (EXIT_SUCCESS);
}
