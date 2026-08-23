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

static const char *
find_nocase (const char *haystack, const char *needle) {

  size_t i, j, haylen, needlelen;

  if (haystack == NULL || needle == NULL) return NULL;
  haylen = strlen (haystack);
  needlelen = strlen (needle);
  if (needlelen == 0) return haystack;
  if (needlelen > haylen) return NULL;

  for (i = 0; i <= haylen - needlelen; i++) {
    for (j = 0; j < needlelen; j++) {
      if (tolower ((unsigned char) haystack[i + j]) !=
          tolower ((unsigned char) needle[j])) break;
    }
    if (j == needlelen) return haystack + i;
  }

  return NULL;
}

static const char *
skip_space (const char *p) {
  while (*p == ' ' || *p == '\t') p++;
  return p;
}

static void
print_value (FILE *fo, const char *label, const char *line, size_t prefix_len) {

  size_t len;
  const char *p;

  p = skip_space (line + prefix_len);
  len = strcspn (p, "\r\n");
  fprintf (fo, "  %s%.*s\n", label, (int) len, p);
}

static int
parse_size_t_decimal (const char *text, size_t *value) {

  unsigned long long v;
  char *end;

  errno = 0;
  text = skip_space (text);
  if (*text == '\0') return EXIT_FAILURE;

  v = strtoull (text, &end, 10);
  if (errno == ERANGE || end == text || v > SIZE_MAX) return EXIT_FAILURE;
  end = (char *) skip_space (end);
  if (*end != '\0' && *end != '\r' && *end != '\n' && *end != ',') return EXIT_FAILURE;

  *value = (size_t) v;
  return EXIT_SUCCESS;
}

static int
parse_hex_offset (const char *text, size_t *value) {

  unsigned long long v;
  char *end;

  errno = 0;
  text = skip_space (text);
  if (*text == '\0') return EXIT_FAILURE;

  v = strtoull (text, &end, 16);
  if (errno == ERANGE || end == text || v > SIZE_MAX) return EXIT_FAILURE;
  end = (char *) skip_space (end);
  if (*end != '\0' && *end != '\r' && *end != '\n') return EXIT_FAILURE;

  *value = (size_t) v;
  return EXIT_SUCCESS;
}

static int
parse_language_line (IDX *idx, const char *line, size_t id, FILE *fo) {

  const char *p, *q, *indexp;
  size_t len;

  if (id >= idx->n_id) {
    fprintf (stderr, "More language IDs were found while parsing than during the initial IDX scan.\n");
    return EXIT_FAILURE;
  }

  p = skip_space (line + 3);
  q = strchr (p, ',');
  if (q == NULL) {
    fprintf (stderr, "Malformed IDX language line: %s", line);
    return EXIT_FAILURE;
  }

  while (q > p && (q[-1] == ' ' || q[-1] == '\t')) q--;
  len = (size_t) (q - p);
  if (len == 0 || len >= MAX_STRINGLEN) {
    fprintf (stderr, "Invalid or overlong IDX language identifier.\n");
    return EXIT_FAILURE;
  }
  memcpy (idx->id[id], p, len);
  idx->id[id][len] = '\0';

  indexp = find_nocase (strchr (p, ','), "index:");
  if (indexp == NULL || parse_size_t_decimal (indexp + 6, &idx->id_index[id]) != EXIT_SUCCESS) {
    fprintf (stderr, "Missing or invalid language index in IDX line: %s", line);
    return EXIT_FAILURE;
  }

  fprintf (fo, "  Language: %s, Index: %zu\n", idx->id[id], idx->id_index[id]);
  return EXIT_SUCCESS;
}

static int
parse_palette (IDX *idx, const char *line, FILE *fo) {

  char tmp[MAX_STRINGLEN];
  char *token;
  unsigned int r, g, b;
  int consumed;
  size_t i, len;

  len = strlen (line + 8);
  if (len >= sizeof (tmp)) {
    fprintf (stderr, "IDX palette line is too long.\n");
    return EXIT_FAILURE;
  }
  memcpy (tmp, line + 8, len + 1);

  idx->n_palette = 0;
  token = strtok (tmp, ", \t\r\n");
  while (token != NULL) {
    if (idx->n_palette >= MAX_PALETTE) {
      fprintf (stderr, "IDX palette contains more than %d entries.\n", MAX_PALETTE);
      return EXIT_FAILURE;
    }

    consumed = 0;
    if (strlen (token) != 6 ||
        sscanf (token, "%2x%2x%2x%n", &r, &g, &b, &consumed) != 3 ||
        consumed != 6 || r > 255 || g > 255 || b > 255) {
      fprintf (stderr, "Invalid RGB palette entry in IDX file: %s\n", token);
      return EXIT_FAILURE;
    }

    idx->palette[idx->n_palette++] = (RGB){(uint8_t) r, (uint8_t) g, (uint8_t) b};
    token = strtok (NULL, ", \t\r\n");
  }

  if (idx->n_palette == 0) {
    fprintf (stderr, "IDX palette line contains no colors.\n");
    return EXIT_FAILURE;
  }

  fprintf (fo, "  Palette: ");
  for (i = 0; i < idx->n_palette; i++) {
    fprintf (fo, "%02x%02x%02x%s", idx->palette[i].r, idx->palette[i].g,
             idx->palette[i].b, i + 1 == idx->n_palette ? "" : " ");
  }
  fprintf (fo, "\n");
  return EXIT_SUCCESS;
}

// Parse .idx file.  VobSub presentation-only keywords that do not affect the
// packet/subpicture decoder are reported but deliberately not applied.
int
parse_idx (IDX *idx, char **idxdata, size_t n_idxlines, FILE *fo) {

  size_t line, id, lang, off, expected_ids;
  const char *p;

  expected_ids = idx->n_id;
  id = 0;

  // First pass: validate/report general properties and language declarations.
  for (line = 0; line < n_idxlines; line++) {
    if (strncmp (idxdata[line], "palette:", 8) == 0) {
      if (parse_palette (idx, idxdata[line], fo) != EXIT_SUCCESS) return EXIT_FAILURE;

    } else if (strncmp (idxdata[line], "langidx:", 8) == 0) {
      if (parse_size_t_decimal (idxdata[line] + 8, &idx->langidx) != EXIT_SUCCESS) {
        fprintf (stderr, "Invalid langidx in IDX file: %s", idxdata[line]);
        return EXIT_FAILURE;
      }
      fprintf (fo, "  Default language index: %zu\n", idx->langidx);

    } else if (strncmp (idxdata[line], "id:", 3) == 0) {
      if (parse_language_line (idx, idxdata[line], id, fo) != EXIT_SUCCESS) return EXIT_FAILURE;
      id++;

    } else if (strncmp (idxdata[line], "forced subs:", 12) == 0) {
      print_value (fo, "Forced subtitles: ", idxdata[line], 12);
    } else if (strncmp (idxdata[line], "smooth:", 7) == 0) {
      print_value (fo, "Smoothing: ", idxdata[line], 7);
    } else if (strncmp (idxdata[line], "size:", 5) == 0) {
      print_value (fo, "Original frame size (px): ", idxdata[line], 5);
    } else if (strncmp (idxdata[line], "org:", 4) == 0) {
      print_value (fo, "Origin (px): ", idxdata[line], 4);
    } else if (strncmp (idxdata[line], "scale:", 6) == 0) {
      print_value (fo, "Image scaling (hor, ver): ", idxdata[line], 6);
    } else if (strncmp (idxdata[line], "fadein/out:", 11) == 0) {
      print_value (fo, "Fade-in, Fade-out times (ms): ", idxdata[line], 11);
    } else if (strncmp (idxdata[line], "alpha:", 6) == 0) {
      print_value (fo, "Alpha: ", idxdata[line], 6);
    } else if (strncmp (idxdata[line], "align:", 6) == 0) {
      print_value (fo, "Alignment (relative to origin): ", idxdata[line], 6);
    } else if (strncmp (idxdata[line], "time offset:", 12) == 0) {
      print_value (fo, "Time offset: ", idxdata[line], 12);
    } else if (strncmp (idxdata[line], "delay:", 6) == 0) {
      print_value (fo, "Delay (ms): ", idxdata[line], 6);
    } else if (strncmp (idxdata[line], "custom colors:", 14) == 0) {
      print_value (fo, "Custom colors: ", idxdata[line], 14);
    }
  }

  if (id != expected_ids) {
    fprintf (stderr, "IDX language count changed between scans (%zu vs %zu).\n", expected_ids, id);
    return EXIT_FAILURE;
  }
  fprintf (fo, "  %zu Language IDs found\n", id);

  // Second pass: associate every timestamp/filepos line with the most recently
  // declared language.  Comments or other keywords between timestamps are okay.
  lang = SIZE_MAX;
  for (line = 0; line < n_idxlines; line++) {
    if (strncmp (idxdata[line], "id:", 3) == 0) {
      if (lang == SIZE_MAX) lang = 0;
      else lang++;
      if (lang >= expected_ids) {
        fprintf (stderr, "Too many language sections in IDX file.\n");
        return EXIT_FAILURE;
      }
      idx->n_timestamps[lang] = 0;
      continue;
    }

    if (strncmp (idxdata[line], "timestamp:", 10) != 0) continue;
    if (lang == SIZE_MAX) {
      fprintf (stderr, "Timestamp appears before the first language declaration in IDX file.\n");
      return EXIT_FAILURE;
    }

    p = find_nocase (idxdata[line], "filepos:");
    if (p == NULL || parse_hex_offset (p + 8, &off) != EXIT_SUCCESS) {
      fprintf (stderr, "Missing or invalid filepos in IDX timestamp line: %s", idxdata[line]);
      return EXIT_FAILURE;
    }

    idx->offset[lang][idx->n_timestamps[lang]++] = off;
  }

  for (lang = 0; lang < expected_ids; lang++) {
    fprintf (fo, "  Found %zu offsets to subtitles for Language ID: %zu (%s)\n",
             idx->n_timestamps[lang], idx->id_index[lang], idx->id[lang]);
  }

  return EXIT_SUCCESS;
}
