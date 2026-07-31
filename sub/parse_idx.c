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

// Parse .idx file.
// N.B. The "custom colors" keyword, if present, is ignored because it is not in the DVD standard. Reported here for interest.
//      The "time offset" and "delay" keywords, if present, are not implemented. Reported here for interest.
//      The "alpha" keyword, if present, is not applied.
//      The "smooth" keyword, if present, is not applied.
//      Many other keywords are also ignored.
int
parse_idx (IDX *idx, char **idxdata, size_t n_idxlines, FILE *fo) {

  size_t i, id, line, lang;
  uint64_t offset;
  int offset_format, align, r, g, b;
  char *p, *token, tmp[MAX_STRINGLEN];

  // Scan through lines of .idx file and process all keywords except timestamp.
  id = 0;
  for (line = 0; line < n_idxlines; line++) {

    // Palette
    if (strncmp (idxdata[line], "palette:", 8) == 0) {
      idx->n_palette = 0;
      memset (tmp, 0, MAX_STRINGLEN * sizeof (char));
      strncpy (tmp, idxdata[line], sizeof (tmp));
      tmp[sizeof (tmp) - 1] = '\0';
      token = strtok (tmp + 8, ", \n");
      while (token && (idx->n_palette < MAX_PALETTE)) {
        sscanf (token, "%02x%02x%02x", &r, &g, &b);
        idx->palette[idx->n_palette] = (RGB){r, g, b};
        idx->n_palette++;
        token = strtok (NULL, ", \n");
      }

      // Report palette colors.
      fprintf (fo, "  Palette: ");
      for (i = 0; i < idx->n_palette; i++) {
        fprintf (fo, "%02x%02x%02x ", idx->palette[i].r, idx->palette[i].g, idx->palette[i].b);
      }
      fprintf (fo, "\n");

    // Default language index
    } else if (strncmp (idxdata[line], "langidx:", 8) == 0) {
      sscanf (idxdata[line] + 9, "%zu", &idx->langidx);
      fprintf (fo, "  Default language index: %zu\n", idx->langidx);

    // Language ID(s)
    } else if (strncmp (idxdata[line], "id:", 3) == 0) {
      p = strcasestr (idxdata[line], "index:");
      sscanf (p + 7, "%zu", &idx->id_index[id]);
      memset (tmp, 0, MAX_STRINGLEN * sizeof (char));
      strncpy (tmp, idxdata[line], sizeof (tmp));
      tmp[sizeof (tmp) - 1] = '\0';
      token = strtok (tmp + 4, ", \n");
      sprintf (idx->id[id], "%s", token);
      fprintf (fo, "  Language: %s, Index: %zu\n", idx->id[id], idx->id_index[id]);
      id++;

    // Forced / Non-forced subtitles
    } else if (strncmp (idxdata[line], "forced subs:", 12) == 0) {
      fprintf (fo, "  Forced subtitles: ");
      i = 13;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n");

    // Smoothing filter for very blocky images (e.g., anti-aliasing)
    } else if (strncmp (idxdata[line], "smooth:", 7) == 0) {
      fprintf (fo, "  Smoothing: ");
      i = 8;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n");

    // Frame size; Typically 720x576 px for PAL DVD, 720x480 px for NTSC DVD, 1920x1080 px for BD, 3840x2160 px for UHD BD
    } else if (strncmp (idxdata[line], "size:", 5) == 0) {
      fprintf (fo, "  Original frame size (px): ");
      i = 6;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n");

    // Origin
    } else if (strncmp (idxdata[line], "org:", 4) == 0) {
      fprintf (fo, "  Origin (px) (relative to upper-left corner; can be overloaded by alignment): ");
      i = 5;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n");

    // Image scaling
    } else if (strncmp (idxdata[line], "scale:", 6) == 0) {
      fprintf (fo, "  Image scaling (hor, ver); Origin is at upper-left corner or at alignment coord: ");
      i = 7;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n"); 

    // Fade in/out times
    } else if (strncmp (idxdata[line], "fadein/out:", 11) == 0) {
      fprintf (fo, "  Fade-in, Fade-out times (ms): ");
      i = 12;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n");

    // Alpha blending
    } else if (strncmp (idxdata[line], "alpha:", 6) == 0) {
      fprintf (fo, "  Alpha: ");
      i = 7;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n");

    // Alignment relative to origin
    // Numerical index format:
    //   0 = bottom-center,    1 = bottom-left,
    //   2 = bottom-right,     3 = middle-center,
    //   4 = middle-left,      5 = middle-right,
    //   6 = top-center,       7 = top-left,
    //   8 = top-right
    // Text format:
    //   ON|OFF at LEFT|CENTER|RIGHT BOTTOM|MIDDLE|TOP
    //   e.g., align: OFF at LEFT TOP
    } else if (strncmp (idxdata[line], "align:", 6) == 0) {
      fprintf (fo, "  Alignment (relative to origin): ");

      // Text format
      if ((idxdata[line][8] < '0') || (idxdata[line][8] > '8')) {
        i = 7;
        while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
          if ((idxdata[line][i] != '\r') && (idxdata[line][i] != '\n')) {
            fprintf (fo, "%c", idxdata[line][i]);
            i++;
          } else {
            break;
          }
        }
        fprintf (fo, "\n");

      // Numerical index format
      } else {
        sscanf (idxdata[line] + 7, "%d", &align);
        switch (align) {
          case 0:
            fprintf (fo, "0 (bottom-center)\n");
            break;
          case 1:
            fprintf (fo, "1 (bottom-left)\n");
            break;
          case 2:
            fprintf (fo, "2 (bottom-right)\n");
            break;
          case 3:
            fprintf (fo, "3 (middle-center)\n");
            break;
          case 4:
            fprintf (fo, "4 (middle-left)\n");
            break;
          case 5:
            fprintf (fo, "5 (middle-right)\n");
            break;
          case 6:
            fprintf (fo, "6 (top-center)\n");
            break;
          case 7:
            fprintf (fo, "7 (top-left)\n");
            break;
          case 8:
            fprintf (fo, "8 (top-right)\n");
            break;
          default:
            fprintf (stderr, "Unknown alignment in IDX file: %s\n", idxdata[line]);
            exit (EXIT_FAILURE);
        }
      }

    // Time offset - Time offset from start of video stream; applies to all subtitles equally
    } else if (strncmp (idxdata[line], "time offset:", 12) == 0) {
      fprintf (fo, "  Time offset: ");

      // Determine offset format.
      offset_format = 0;  // Default to single value (ms)
      i = 13;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\r') && (idxdata[line][i] != '\n')) {
          if (idxdata[line][i] == ':') {
            offset_format = 1;  // Format is hh:mm:ss:ms, where ms is 3 digits and hh may be preceded by a - sign.
          }
          i++;
        } else {
          break;
        }
      }

      // hh:mm:ss:ms format
      if (offset_format) {
        i = 13;
        while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
          if ((idxdata[line][i] != '\r') && (idxdata[line][i] != '\n')) {
            fprintf (fo, "%c", idxdata[line][i]);
            i++;
          } else {
            break;
          }
        }
        fprintf (fo, "\n");

      // Milliseconds format
      } else {
        sscanf (idxdata[line] + 12, "%li", &idx->time_offset.totalms);
        mstotime (&idx->time_offset);
        fprintf (fo, "%02i:%02i:%02i,%03i (total ms: %" PRIu64 ")\n", idx->time_offset.h, idx->time_offset.m, idx->time_offset.s, idx->time_offset.ms, idx->time_offset.totalms);
      }

    // Delay (ms)
    } else if (strncmp (idxdata[line], "delay:", 6) == 0) {
      fprintf (fo, "  Delay (ms): ");
      i = 7;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n");

    // Custom colors
    } else if (strncmp (idxdata[line], "custom colors:", 14) == 0) {
      fprintf (fo, "  Custom colors: ");
      i = 15;
      while (i < strnlen (idxdata[line], MAX_STRINGLEN)) {
        if ((idxdata[line][i] != '\n') && (idxdata[line][i] != '\r')) {
          fprintf (fo, "%c", idxdata[line][i]);
          i++;
        } else {
          break;
        }
      }
      fprintf (fo, "\n");

    }  // End pattern matching to keywords
  }  // Next line from file

  idx->n_id = id;

  fprintf (fo, "  %zu Language IDs found\n", idx->n_id);

  // Scan through lines of .idx file and extract all subtitle offsets for each language.
  lang = 0;  // Array index of array offset
  line = 0;  // Line of .idx file
  while ((line < n_idxlines) && (lang < idx->n_id)) {

    // Search for a language ID.
    if (strncmp (idxdata[line], "id:", 3) == 0) {

      // Move ahead to timestamps (there's often comment lines).
      while (line < n_idxlines) {
        if (strncmp (idxdata[line], "timestamp:", 10) == 0) break;
        line++;
      }

      // Extract the offsets for each timestamp (subtitle).
      idx->n_timestamps[lang] = 0;
      while (line < n_idxlines) {

        if (strncmp (idxdata[line], "timestamp:", 10) != 0) break;

        p = strstr (idxdata[line], "filepos:");
        if (p) {
          sscanf(p + 8, "%" SCNx64, &offset);
          idx->offset[lang][idx->n_timestamps[lang]] = offset;
          idx->n_timestamps[lang]++;  // Keep track of how many timestamps for this language.
        }

        line++;  // Next line in .idx file

      }  // Next timestamp

      fprintf (fo, "  Found %zu offsets to subtitles for Language ID: %zu (%s)\n", idx->n_timestamps[lang], idx->id_index[lang], idx->id[lang]);
      lang++;  // Next language

    }  // End if found "id:"

    line++;

  }  // Continue through lines of .idx file.

  return (EXIT_SUCCESS);
}
