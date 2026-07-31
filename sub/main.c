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
main (int argc, char **argv) {

  size_t i, j, subdatalen, n_idxlines, lang, *count, max;
  int fi_sub;
  char *temp, *idx_filename, *sub_filename, **idxdata, *timestamp, *endptr;
  uint8_t *subdata;
  OPTIONS options;
  IDX idx;
  PES pes_info;
  TIME time;
  FILE *fi_idx, *fo;

  // Allocate memory for various arrays.
  idx_filename = allocate_strmem (MAX_STRINGLEN);
  sub_filename = allocate_strmem (MAX_STRINGLEN);

  // Process the command line arguments, if any.
  options.makebmp_flag = 0;  // Default to not make bitmap files.
  options.offset_flag = 0;  // Default to not applying PTS and DTS timestamp offsets.
  options.sync_flag = 0;  // Default to not resynchronizing PTS and DTS timestamps.
  if ((argc == 4) && (strncmp (argv[3], "bmp", 3) == 0)) {
    options.makebmp_flag = 1;

  } else if ((argc == 4) && (strncmp (argv[3], "offset", 6) == 0)) {
    options.offset_flag = 1;

  } else if ((argc == 4) && (strncmp (argv[3], "sync", 6) == 0)) {
    options.sync_flag = 1;

  } else if (argc < 3) {
    fprintf (stdout, "\nsub - A tool to analyze VobSub IDX/SUB (.idx/.sub) files and produce a report file.\n");
    fprintf (stdout, "      The idx/sub input files could be derived from an NTSC or PAL DVD, or HD (1080p) or UHD (4K) BluRay.\n");
    fprintf (stdout, "      Optional functions include: - Producing a bitmap file for each subtitle, and\n");
    fprintf (stdout, "                                  - Synchronizing/offsetting all timestamps to new anchor-points.\n");
    fprintf (stdout, "      Note that some idx/sub files have multiple languages. All subtitles from all languages will be extracted to bitmap when bitmaps are requested.\n");
    fprintf (stdout, "\nBitmap notes:\n");
    fprintf (stdout, "  Filenames are: start and end times (hh_mm_ss_ms__hh_mm_ss_ms), the language ID and language index, and then .bmp.\n");
    fprintf (stdout, "  The language index is included because you can have multiple subtitle streams for the same language and\n");
    fprintf (stdout, "  they need to be differentiated. SubtitleEdit can read the timestamps from the filenames if they don't include the language ID\n");
    fprintf (stdout, "  and index. Modify the write_bmp() function to suit your needs.\n");
    fprintf (stdout, "\nOffset notes:\n");
    fprintf (stdout, "  The user can supply positive or negative hours, minutes, seconds, and millisecond offsets to be applied to all PTS and DTS timestamps\n");
    fprintf (stdout, "  in out.sub output file. The timestamps in output file out.idx file will also have the revised timestamps.\n");

    fprintf (stdout, "\nSynchronization notes:\n");
    fprintf (stdout, "  Subtitle durations are preserved.\n");
    fprintf (stdout, "  Synchronization of all timestamps in a .sub file is accomplished using \"first\" and \"last\" timestamps as anchor-points.\n");
    fprintf (stdout, "  Choose \"first\" and \"last\" subtitles that are near or at beginning and end of the feature in order to maximize scaling accuracy.\n");
    fprintf (stdout, "  Use sub to produce bitmaps (which have timestamp filenames) or use SubtitleEdit to obtain the existing subtitle timestamps.\n");
    fprintf (stdout, "  The timestamps in output file out.idx will also be the resynchronized timestamps.\n");
    fprintf (stdout, "\nBuild: make\n");
    fprintf (stdout, "\nUsage: ./sub file.idx file.sub [option]\n");
    fprintf (stdout, "\nOptions:\n");
    fprintf (stdout, "  bmp      Produce a bitmap file for each subtitle.\n");
    fprintf (stdout, "  offset   Apply user-input offsets to all subtitle timestamps.\n");
    fprintf (stdout, "           Produce files out.idx and out.sub with offset subtitle timestamps.\n");
    fprintf (stdout, "  sync     Synchronize all subtitle timestamps to user-input anchor points.\n");
    fprintf (stdout, "           Produce files out.idx and out.sub with resynchronized subtitle timestamps.\n");
    fprintf (stdout, "\nOutput: sub.out, and optionally: bitmap file for each subtitle, or resynchronized/offset out.idx and out.sub pair of files.\n");
    fprintf (stdout, "\nReferences: U.S. Patent US006871008B1, ITU-R H.222.0, http://www.mpucoder.com/DVD/index.html\n\n");
    return (EXIT_SUCCESS);
  }
  strncpy (idx_filename, argv[1], MAX_STRINGLEN);
  strncpy (sub_filename, argv[2], MAX_STRINGLEN);

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAX_STRINGLEN);
  timestamp = allocate_strmem (MAX_STRINGLEN);
  options.change = allocate_changemem (MAX_CHANGES);

  // Initialize to no changes due to offset or sync.
  options.nchanges = 0;

  // Open .idx file.
  fi_idx = fopen (idx_filename, "r");
  if (fi_idx == NULL) {
    fprintf (stderr, "\nUnable to open input file %s.\n", idx_filename);
    exit (EXIT_FAILURE);
  }

  // Count number of lines in .idx file.
  n_idxlines = 0;
  while (readline (fi_idx, temp, MAX_STRINGLEN) != -1) {
    n_idxlines++;
  }
  rewind (fi_idx);

  // Allocate memory for various arrays.
  idxdata = allocate_strmemp (n_idxlines);
  for (i = 0; i < n_idxlines; i++) {
    idxdata[i] = allocate_strmem (MAX_STRINGLEN);
  }

  // Read entire .idx file into an array.
  for (i = 0; i < n_idxlines; i++) {
    if (readline (fi_idx, idxdata[i], MAX_STRINGLEN) == -1) {
      fprintf (stderr, "\nCannot read line %zu from input file %s.\n", i + 1, idx_filename);
      exit (EXIT_FAILURE);
    }
  }

  // Close .idx file.
  fclose (fi_idx);
  
  // Count number of language IDs.
  idx.n_id = 0;
  for (i = 0; i < n_idxlines; i++) {
    if (strncmp (idxdata[i], "id:", 3) == 0) idx.n_id++;
  }

  // Allocate memory for various arrays.
  idx.id = allocate_strmemp (idx.n_id);
  for (i = 0; i < idx.n_id; i++) {
    idx.id[i] = allocate_strmem (5);  // e.g., "en", "nl", etc.
  }
  idx.id_index = allocate_sizetmem ((int) idx.n_id);
  count = allocate_sizetmem ((int) idx.n_id);

  // Find maximum number of timestamps out of any language.
  max = 0;
  lang = (size_t) -1;
  memset (count, 0, idx.n_id * sizeof (int));
  for (i = 0; i < n_idxlines; i++) {

    if (strncmp (idxdata[i], "id:", 3) == 0) {
      lang++;
      continue;
    }

    if ((lang < idx.n_id) && (strncmp (idxdata[i], "timestamp:", 10) == 0)) {
      count[lang]++;
    }

  }  // Next line
  for (lang = 0; lang < idx.n_id; lang++) {
    if (count[lang] > max) max = count[lang];
  }

  // Allocate memory for various arrays.
  idx.n_timestamps = allocate_sizetmem ((int) max);
  idx.offset = allocate_sizetmemp ((int) idx.n_id);
  for (i = 0; i < idx.n_id; i++) {
    idx.offset[i] = allocate_sizetmem ((int) max);
  }

  // Open .sub input file.
  // Memory-map the file rather than loading it into an array.
  fi_sub = open (sub_filename, O_RDONLY);
  if (fi_sub == -1) {
    fprintf (stderr, "\nUnable to open input file %s.\n", sub_filename);
    exit (EXIT_FAILURE);
  }
  struct stat st;

  // Get file size.
  if (fstat (fi_sub, &st) == -1) {
    perror ("fstat() failed.\n");
    close (fi_sub);
    exit (EXIT_FAILURE);
  }
  subdatalen = st.st_size;

  // Memory-map the .sub file.
  subdata = mmap (NULL, subdatalen, PROT_READ, MAP_PRIVATE, fi_sub, 0);
  if (subdata == MAP_FAILED) {
    perror ("mmap() failed.\n");
    close (fi_sub);
    exit (EXIT_FAILURE);
  }

  // Open output (report) file sub.out.
  fo = fopen ("sub.out", "r");
  if (fo != NULL) {
    fprintf (stderr, "Output file sub.out already exists.\n");
    fclose (fo);
    exit (EXIT_FAILURE);
  }
  fo = fopen ("sub.out", "w");
  if (fo == NULL) {
    fprintf (stderr, "Can't open output file sub.out.\n");
    exit (EXIT_FAILURE);
  }

  // Ask for desired timestep offsets, if requested.
  // All can be zero.
  if (options.offset_flag) {
    fprintf (stdout, "\nOffset values can be positive or negative integers.\n\n");
    fprintf (stdout, "What is desired offset hours? ");
    memset (temp, 0, MAX_STRINGLEN * sizeof (char));
    inputtext (temp);
    errno = 0;
    options.offset.h = (int) strtol (temp, &endptr, 10);
    if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
      fprintf (stderr, "Cannot make integer of offset hours: %s\n", temp);
      exit (EXIT_FAILURE);
    }

    fprintf (stdout, "What is desired offset minutes? ");
    memset (temp, 0, MAX_STRINGLEN * sizeof (char));
    inputtext (temp);
    errno = 0;
    options.offset.m = (int) strtol (temp, &endptr, 10);
    if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
      fprintf (stderr, "Cannot make integer of offset minutes: %s\n", temp);
      exit (EXIT_FAILURE);
    }

    fprintf (stdout, "What is desired offset seconds? ");
    memset (temp, 0, MAX_STRINGLEN * sizeof (char));
    inputtext (temp);
    errno = 0;
    options.offset.s = (int) strtol (temp, &endptr, 10);
    if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
      fprintf (stderr, "Cannot make integer of offset seconds: %s\n", temp);
      exit (EXIT_FAILURE);
    }

    fprintf (stdout, "What is desired offset milliseconds? ");
    memset (temp, 0, MAX_STRINGLEN * sizeof (char));
    inputtext (temp);
    errno = 0;
    options.offset.ms = (int) strtol (temp, &endptr, 10);
    if ((errno == ERANGE) || (errno == EINVAL) || (endptr == temp)) {
      fprintf (stderr, "Cannot make integer of offset milliseconds: %s\n", temp);
      exit (EXIT_FAILURE);
    }
    fprintf (stdout, "\n");

    // Calculate total milliseconds of offset.
    timetoms (&options.offset);

  }  // End if options.offset

  // Ask for current and new start timestamps for anchor points, if resynchronization is requested.
  if (options.sync_flag) {

    // Ask for current timestamp of first subtitle.
    fprintf (stdout, "\nCurrent start timestamp for first subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.oldfirstms = time.totalms;
  
    // Ask for current timestamp of last subtitle.
    fprintf (stdout, "Current start timestamp for last subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.oldlastms = time.totalms;

    if ((options.oldlastms - options.oldfirstms) <= 0) {
      fprintf (stderr, "Current timestamps: last - first <= 0.\n");
      exit (EXIT_FAILURE);
    }
  
    // Ask for new timestamp of first subtitle.
    fprintf (stdout, "New start timestamp for first subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.newfirstms = time.totalms;
  
    // Ask for new timestamp of last subtitle.
    fprintf (stdout, "New start timestamp for last subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.newlastms = time.totalms;

    if ((options.newlastms - options.newfirstms) <= 0) {
      fprintf (stderr, "New timestamps: last - first <= 0.\n");
      exit (EXIT_FAILURE);
    }
  }  // End if options.sync

  // Parse .idx file.
  fprintf (fo, "IDX File: %s\n\n", idx_filename);
  parse_idx (&idx, idxdata, n_idxlines, fo);

  // Parse .sub file.
  fprintf (fo, "\nSUB File: %s (%zu bytes)\n", sub_filename, subdatalen);
  extract_subs (subdata, subdatalen, &options, &idx, &pes_info, fo);

  // Close sub.out (report) output file.
  fclose (fo);

  // Apply offset or resynchronize, if requested.
  if ((options.offset_flag) || (options.sync_flag)) {

    // Open output file for revised .sub file.
    fo = fopen ("out.sub", "r");
    if (fo != NULL) {
      fprintf (stderr, "Output file out.sub already exists.\n");
      fclose (fo);
      exit (EXIT_FAILURE);
    }
    fo = fopen ("out.sub", "wb");
    if (fo == NULL) {
      fprintf (stderr, "Can't open output file out.sub.\n");
      exit (EXIT_FAILURE);
    }

    // Write out new .sub file.
    j = 0;  // Index of change array
    for (i = 0; i < subdatalen; i++) {
      if (i == options.change[j].offset) {
        fputc (options.change[j].new_value, fo);
        j++;
      } else {
        fputc (subdata[i], fo);
      }
    }

    // Close out.sub output file.
    fclose (fo);

    // Create a new .idx file with updated timestamps.
    write_idx_file (idx_filename, &options);

  }  // End if options.offset || options.sync

  // Unmap and close .sub input file.
  munmap (subdata, subdatalen);
  close (fi_sub);

  // Free allocated memory.
  free (temp);
  free (idx_filename);
  free (sub_filename);
  free (timestamp);
  free (options.change);
  free (idx.n_timestamps);
  for (i = 0; i < n_idxlines; i++) {
    free (idxdata[i]);
  }
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
