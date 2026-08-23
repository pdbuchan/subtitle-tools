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

static int parse_integer_input (const char *, int *, const char *);
static void print_usage (void);

int
main (int argc, char **argv) {

  size_t i, j, subdatalen, n_idxlines, lang, *count;
  int fi_sub;
  char *temp, *idx_filename, *sub_filename, **idxdata, *timestamp;
  uint8_t *subdata;
  OPTIONS options;
  IDX idx;
  PES pes_info;
  TIME time;
  FILE *fi_idx, *fo;
  struct stat st;

  memset (&options, 0, sizeof (options));
  memset (&idx, 0, sizeof (idx));
  memset (&pes_info, 0, sizeof (pes_info));
  memset (&time, 0, sizeof (time));

  if (argc < 3) {
    print_usage ();
    return (EXIT_SUCCESS);
  }
  if (argc > 4) {
    print_usage ();
    return (EXIT_FAILURE);
  }

  if (argc == 4) {
    if (strcmp (argv[3], "bmp") == 0) {
      options.makebmp_flag = 1;
    } else if (strcmp (argv[3], "offset") == 0) {
      options.offset_flag = 1;
    } else if (strcmp (argv[3], "sync") == 0) {
      options.sync_flag = 1;
    } else {
      fprintf (stderr, "Unknown option: %s\n", argv[3]);
      print_usage ();
      return (EXIT_FAILURE);
    }
  }

  idx_filename = allocate_strmem (MAX_STRINGLEN);
  sub_filename = allocate_strmem (MAX_STRINGLEN);
  temp = allocate_strmem (MAX_STRINGLEN);
  timestamp = allocate_strmem (MAX_STRINGLEN);

  if (snprintf (idx_filename, MAX_STRINGLEN, "%s", argv[1]) >= MAX_STRINGLEN ||
      snprintf (sub_filename, MAX_STRINGLEN, "%s", argv[2]) >= MAX_STRINGLEN) {
    fprintf (stderr, "Input filename is too long.\n");
    return (EXIT_FAILURE);
  }

  fi_idx = fopen (idx_filename, "r");
  if (fi_idx == NULL) {
    fprintf (stderr, "\nUnable to open input file %s.\n", idx_filename);
    return (EXIT_FAILURE);
  }

  n_idxlines = 0;
  while (readline (fi_idx, temp, MAX_STRINGLEN) != -1) n_idxlines++;
  if (n_idxlines == 0) {
    fprintf (stderr, "IDX file is empty.\n");
    return (EXIT_FAILURE);
  }
  rewind (fi_idx);

  idxdata = allocate_strmemp (n_idxlines);
  for (i = 0; i < n_idxlines; i++) {
    idxdata[i] = allocate_strmem (MAX_STRINGLEN);
    if (readline (fi_idx, idxdata[i], MAX_STRINGLEN) == -1) {
      fprintf (stderr, "Cannot read line %zu from %s.\n", i + 1, idx_filename);
      return (EXIT_FAILURE);
    }
  }
  fclose (fi_idx);

  idx.n_id = 0;
  for (i = 0; i < n_idxlines; i++) {
    if (strncmp (idxdata[i], "id:", 3) == 0) idx.n_id++;
  }
  if (idx.n_id == 0) {
    fprintf (stderr, "No language ID sections found in IDX file.\n");
    return (EXIT_FAILURE);
  }

  idx.id = allocate_strmemp (idx.n_id);
  for (i = 0; i < idx.n_id; i++) idx.id[i] = allocate_strmem (MAX_STRINGLEN);
  idx.id_index = allocate_sizetmem (idx.n_id);
  idx.n_timestamps = allocate_sizetmem (idx.n_id);
  count = allocate_sizetmem (idx.n_id);

  lang = SIZE_MAX;
  for (i = 0; i < n_idxlines; i++) {
    if (strncmp (idxdata[i], "id:", 3) == 0) {
      if (lang == SIZE_MAX) lang = 0;
      else lang++;
      continue;
    }
    if (lang < idx.n_id && strncmp (idxdata[i], "timestamp:", 10) == 0) count[lang]++;
  }

  idx.offset = allocate_sizetmemp (idx.n_id);
  for (i = 0; i < idx.n_id; i++) {
    idx.offset[i] = allocate_sizetmem (count[i] == 0 ? 1 : count[i]);
  }

  fi_sub = open (sub_filename, O_RDONLY);
  if (fi_sub == -1) {
    fprintf (stderr, "\nUnable to open input file %s.\n", sub_filename);
    return (EXIT_FAILURE);
  }
  if (fstat (fi_sub, &st) == -1) {
    perror ("fstat() failed");
    close (fi_sub);
    return (EXIT_FAILURE);
  }
  if (st.st_size <= 0 || (uintmax_t) st.st_size > SIZE_MAX) {
    fprintf (stderr, "Invalid or unsupported .sub file size.\n");
    close (fi_sub);
    return (EXIT_FAILURE);
  }
  subdatalen = (size_t) st.st_size;

  subdata = mmap (NULL, subdatalen, PROT_READ, MAP_PRIVATE, fi_sub, 0);
  if (subdata == MAP_FAILED) {
    perror ("mmap() failed");
    close (fi_sub);
    return (EXIT_FAILURE);
  }

  fo = fopen ("sub.out", "r");
  if (fo != NULL) {
    fclose (fo);
    fprintf (stderr, "Output file sub.out already exists.\n");
    return (EXIT_FAILURE);
  }
  fo = fopen ("sub.out", "w");
  if (fo == NULL) {
    fprintf (stderr, "Cannot open output file sub.out.\n");
    return (EXIT_FAILURE);
  }

  if (options.offset_flag) {
    fprintf (stdout, "\nOffset values can be positive or negative integers.\n\n");

    fprintf (stdout, "What is desired offset hours? ");
    inputtext (temp);
    if (parse_integer_input (temp, &options.offset.h, "offset hours") != EXIT_SUCCESS) return (EXIT_FAILURE);

    fprintf (stdout, "What is desired offset minutes? ");
    inputtext (temp);
    if (parse_integer_input (temp, &options.offset.m, "offset minutes") != EXIT_SUCCESS) return (EXIT_FAILURE);

    fprintf (stdout, "What is desired offset seconds? ");
    inputtext (temp);
    if (parse_integer_input (temp, &options.offset.s, "offset seconds") != EXIT_SUCCESS) return (EXIT_FAILURE);

    fprintf (stdout, "What is desired offset milliseconds? ");
    inputtext (temp);
    if (parse_integer_input (temp, &options.offset.ms, "offset milliseconds") != EXIT_SUCCESS) return (EXIT_FAILURE);

    timetoms (&options.offset);
    fprintf (stdout, "\n");
  }

  if (options.sync_flag) {
    fprintf (stdout, "\nCurrent start timestamp for first subtitle (hh:mm:ss,mmm)? ");
    inputtext (timestamp);
    if (parse_timestamp (timestamp, &time) != EXIT_SUCCESS) return (EXIT_FAILURE);
    options.oldfirstms = time.totalms;

    fprintf (stdout, "Current start timestamp for last subtitle (hh:mm:ss,mmm)? ");
    inputtext (timestamp);
    if (parse_timestamp (timestamp, &time) != EXIT_SUCCESS) return (EXIT_FAILURE);
    options.oldlastms = time.totalms;
    if (options.oldlastms <= options.oldfirstms) {
      fprintf (stderr, "Current timestamps must have last > first.\n");
      return (EXIT_FAILURE);
    }

    fprintf (stdout, "New start timestamp for first subtitle (hh:mm:ss,mmm)? ");
    inputtext (timestamp);
    if (parse_timestamp (timestamp, &time) != EXIT_SUCCESS) return (EXIT_FAILURE);
    options.newfirstms = time.totalms;

    fprintf (stdout, "New start timestamp for last subtitle (hh:mm:ss,mmm)? ");
    inputtext (timestamp);
    if (parse_timestamp (timestamp, &time) != EXIT_SUCCESS) return (EXIT_FAILURE);
    options.newlastms = time.totalms;
    if (options.newlastms <= options.newfirstms) {
      fprintf (stderr, "New timestamps must have last > first.\n");
      return (EXIT_FAILURE);
    }
  }

  fprintf (fo, "IDX File: %s\n\n", idx_filename);
  if (parse_idx (&idx, idxdata, n_idxlines, fo) != EXIT_SUCCESS) return (EXIT_FAILURE);

  // Ensure all IDX offsets are inside the mapped .sub file before parsing packets.
  for (lang = 0; lang < idx.n_id; lang++) {
    for (i = 0; i < idx.n_timestamps[lang]; i++) {
      if (idx.offset[lang][i] >= subdatalen) {
        fprintf (stderr, "IDX filepos 0x%zx lies outside the .sub file.\n", idx.offset[lang][i]);
        return (EXIT_FAILURE);
      }
    }
  }

  fprintf (fo, "\nSUB File: %s (%zu bytes)\n", sub_filename, subdatalen);
  if (extract_subs (subdata, subdatalen, &options, &idx, &pes_info, fo) != EXIT_SUCCESS) {
    return (EXIT_FAILURE);
  }
  fclose (fo);

  if (options.offset_flag || options.sync_flag) {
    sort_and_compact_changes (&options);

    fo = fopen ("out.sub", "r");
    if (fo != NULL) {
      fclose (fo);
      fprintf (stderr, "Output file out.sub already exists.\n");
      return (EXIT_FAILURE);
    }
    fo = fopen ("out.sub", "wb");
    if (fo == NULL) {
      fprintf (stderr, "Cannot open output file out.sub.\n");
      return (EXIT_FAILURE);
    }

    j = 0;
    for (i = 0; i < subdatalen; i++) {
      if (j < options.nchanges && i == options.change[j].offset) {
        fputc (options.change[j].new_value, fo);
        j++;
      } else {
        fputc (subdata[i], fo);
      }
    }
    fclose (fo);

    if (j != options.nchanges) {
      fprintf (stderr, "A recorded timestamp change lies outside the .sub file.\n");
      return (EXIT_FAILURE);
    }

    if (write_idx_file (idx_filename, &options) != EXIT_SUCCESS) return (EXIT_FAILURE);
  }

  munmap (subdata, subdatalen);
  close (fi_sub);

  free (temp);
  free (idx_filename);
  free (sub_filename);
  free (timestamp);
  free (options.change);
  free (idx.n_timestamps);
  for (i = 0; i < n_idxlines; i++) free (idxdata[i]);
  free (idxdata);
  for (i = 0; i < idx.n_id; i++) {
    free (idx.id[i]);
    free (idx.offset[i]);
  }
  free (idx.id);
  free (idx.offset);
  free (idx.id_index);
  free (count);

  return (EXIT_SUCCESS);
}

static int
parse_integer_input (const char *text, int *value, const char *description) {
  
  long v;
  char *endptr;
  
  errno = 0;
  endptr = NULL;
  v = strtol (text, &endptr, 10);
  if (errno == ERANGE || endptr == text || endptr == NULL || *endptr != '\0' ||
      v < INT_MIN || v > INT_MAX) {
    fprintf (stderr, "Cannot make integer of %s: %s\n", description, text);
    return (EXIT_FAILURE);
  }
    
  *value = (int) v;
  return (EXIT_SUCCESS);
}
    
static void
print_usage (void) {
  fprintf (stdout, "\nsub - Analyze VobSub IDX/SUB (.idx/.sub) files and produce a report.\n");
  fprintf (stdout, "      Supports classic DVD SPUs and extended HD VobSub SPUs, including\n");
  fprintf (stdout, "      256-color/alpha commands, 32-bit offsets and 8-bit RLE pixel data.\n");
  fprintf (stdout, "\nUsage: ./sub file.idx file.sub [option]\n");
  fprintf (stdout, "\nOptions:\n");
  fprintf (stdout, "  bmp      Produce a bitmap file for each subtitle.\n");
  fprintf (stdout, "  offset   Apply an offset to all PTS/DTS and IDX timestamps; write out.idx/out.sub.\n");
  fprintf (stdout, "  sync     Synchronize timestamps between two anchor points; write out.idx/out.sub.\n");
  fprintf (stdout, "\nOutput: sub.out and, depending on the option, bitmap files or out.idx/out.sub.\n\n");
}

