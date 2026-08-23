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

int
main (int argc, char **argv) {

  size_t i, j, tslen, packet, page_idx;
  int fi;
  char temp[MAX_STRINGLEN], filename[MAX_STRINGLEN], timestamp[MAX_STRINGLEN];
  uint8_t *tsdata;
  STATE state;
  PAT pat;
  SECTION *section;
  SEGMENT *segment;
  PAGE *page;
  PES pes;
  FILE *fo;

  // Initialize top-level state before processing command-line options or input.
  // This also ensures pointer/count fields begin in a known state before their
  // more specific initialization below.
  memset (&state, 0, sizeof (state));
  memset (&pat, 0, sizeof (pat));
  memset (&pes, 0, sizeof (pes));

  // Process the command line arguments, if any.
  if (argc == 2) {
    state.makebmp_flag = 0;  // No options selected.

  } else if ((argc == 3) && (strcmp (argv[2], "bmp") == 0)) {
    state.makebmp_flag = 1;

  } else {
    fprintf (stdout, "\ndvb - A tool to analyze a transport stream (.ts) file containing DVB subtitles and produce a report file.\n");
    fprintf (stdout, "      Optional function: Produce a bitmap file for each DVB subtitle\n");
    fprintf (stdout, "\nBitmap notes:\n");
    fprintf (stdout, "  Bitmap filenames are: start and end times (hh_mm_ss_ms__hh_mm_ss_ms), and then .bmp.\n");
    fprintf (stdout, "  SubtitleEdit can read the timestamps from the filenames.\n");
    fprintf (stdout, "\nBuild: make\n");
    fprintf (stdout, "\nUsage: ./dvb file.ts [option]\n");
    fprintf (stdout, "\nOptions:\n");
    fprintf (stdout, "  bmp      Produce a bitmap file for each subtitle.\n");
    fprintf (stdout, "\nOutput: dvb.out, and optionally, a bitmap file for each subtitle.\n");
    fprintf (stdout, "\nReferences: ISO/IEC 13818-1, ETSI EN 300 743, ETSI EN 300 468, ETSI 301 192\n\n");
    return (EXIT_SUCCESS);
  }

  // Copy the input filename while guaranteeing NUL termination. snprintf() also
  // tells us if the supplied name was too long for the fixed-size buffer.
  if (snprintf (filename, sizeof (filename), "%s", argv[1]) >= (int) sizeof (filename)) {
    fprintf (stderr, "Input filename is too long.\n");
    return (EXIT_FAILURE);
  }

  memset (temp, 0, MAX_STRINGLEN * sizeof (char));
  memset (timestamp, 0, MAX_STRINGLEN * sizeof (char));

  // State
  state.npages = 0;
  state.have_pat = 0;  // We haven't processed a PAT yet. Once we process one, we ignore the rest unless version number changes.
  state.display_width = 0;  // Display width field from the most recently parsed DDS.
  state.display_height = 0;  // Display height field from the most recently parsed DDS.
  memset (state.pid_type, PID_UNKNOWN, MAX_PIDS * sizeof (PID_TYPE));
  memset (state.section_bytecount, 0, MAX_PIDS * sizeof (size_t));
  memset (state.previous_section_length, 0, MAX_PIDS * sizeof (size_t));  // Total length of previous PSI section
  memset (state.previous_section_bytecount, 0, MAX_PIDS * sizeof (size_t));

  // PAT
  // There can only be one PAT, but it can be updated if version changes.
  pat.version = 0;
  pat.nprograms = 0;
  pat.program = NULL;  // We will allocate them dynamically as needed. See parse_pat().

  // PSI section
  section = allocate_sectionmem (MAX_PIDS);
  for (i = 0; i < MAX_PIDS; i++) {
    section[i].buffer = NULL;  // We will allocate them dynamically as needed. See build_psi_section().
  }

  // Some PES parameters.
  pes.collecting = 0;  // Initially set to indicate not collecting PES packets to form a segment.
  pes.total_length = -1;  // Set to unknown.

  // PES segment
  segment = allocate_segmentmem (MAX_PIDS);
  for (i = 0; i < MAX_PIDS; i++) {
    segment[i].buffer = NULL;  // We will allocate them dynamically as needed. See build_pes_segment().
  }

  // Pages
  page = NULL;  // We will allocate them dynamically as needed. See parse_pcs().

  // Open transport stream (.ts) file.
  // Memory-map the file rather than loading it into an array.
  fi = open (filename, O_RDONLY);
  if (fi == -1) {
    fprintf (stderr, "\nUnable to open input file %s.\n", filename);
    exit (EXIT_FAILURE);
  }
  struct stat st;

  // Get file size.
  if (fstat (fi, &st) == -1) {
    perror ("fstat() failed.\n");
    close (fi);
    exit (EXIT_FAILURE);
  }
  // mmap() and all parser offsets use size_t, so reject an empty file or a file
  // whose size cannot be represented safely by size_t.
  if (st.st_size <= 0 || (uintmax_t) st.st_size > SIZE_MAX) {
    fprintf (stderr, "Input file has an invalid size.\n");
    close (fi);
    return (EXIT_FAILURE);
  }
  tslen = (size_t) st.st_size;

  // Memory-map the .ts file.
  tsdata = mmap (NULL, tslen, PROT_READ, MAP_PRIVATE, fi, 0);
  if (tsdata == MAP_FAILED) {
    perror ("mmap() failed.\n");
    close (fi);
    exit (EXIT_FAILURE);
  }

  // Open output (report) file dvb.out.
  fo = fopen ("dvb.out", "r");
  if (fo != NULL) {
    fprintf (stderr, "Output file dvb.out already exists.\n");
    fclose (fo);
    exit (EXIT_FAILURE);
  }
  fo = fopen ("dvb.out", "w");
  if (fo == NULL) {
    fprintf (stderr, "Can't open output file dvb.out.\n");
    exit (EXIT_FAILURE);
  }

  // Report Transport Stream (.ts) filename to output file.
  fprintf (fo, "File: %s (%zu bytes)\n", filename, tslen);

  // Parse all Transport Stream packets in .ts file.
  state.ts_index = 0;  // Start at beginning of .ts file.
  packet = 0;  // Initialize Transport Stream packet counter.
  state.nsubs = 0;  // Count of subtitles
  while (state.ts_index < tslen) {
    // Propagate parser failures instead of continuing with partially decoded
    // state, which could otherwise make later packet diagnostics misleading.
    if (parse_ts_packet (&state, &page, &pat, tsdata, tslen, &packet, &pes,
                         section, segment, fo) != EXIT_SUCCESS) {
      fprintf (stderr, "Failed while parsing TS packet %zu.\n", packet);
      fclose (fo);
      munmap (tsdata, tslen);
      close (fi);
      return (EXIT_FAILURE);
    }
    packet++;
  }

  // Create bitmaps for any complete Display Sets that are not rendered yet.
  for (page_idx = 0; page_idx < state.npages; page_idx++) {
    finalize_page_if_needed (&state, page, page_idx, &pes);
  }

  // Close dvb.out (report) output file.
  fclose (fo);

  fprintf (stdout, "Found %zu subtitles.\n", state.nsubs);

  // Unmap and close .ts input file.
  if (munmap (tsdata, tslen) == -1) {
    perror ("munmap() failed.\n");
    exit (EXIT_FAILURE);
  }
  close (fi);

  // Free allocated memory.
  // PAT
  for (i = 0; i < pat.nprograms; i++) {
    if (pat.program[i].pmt.stream != NULL) {
      free (pat.program[i].pmt.stream);
    }
  }
  free (pat.program);

  // PSI section
  for (i = 0; i < MAX_PIDS; i++) {
    if (section[i].buffer != NULL) {
      free (section[i].buffer);
    }
  }
  free (section);

  // PES segment
  for (i = 0; i < MAX_PIDS; i++) {
    if (segment[i].buffer != NULL) {
      free (segment[i].buffer);
    }
  }
  free (segment);

  // Page
  for (i = 0; i < state.npages; i++) {
    for (j = 0; j < page[i].nobjects; j++) {
      // Each decoded object owns both its CLUT-index buffer and its coded-pixel
      // mask used to preserve ragged-right uncoded pixels during composition.
      free (page[i].object[j].buffer);
      free (page[i].object[j].coded);
    }
    free (page[i].object);
    if (page[i].clut) free (page[i].clut);
    if (page[i].buffer) free (page[i].buffer);
    if (page[i].region_pos) free (page[i].region_pos);
    if (page[i].region) free (page[i].region);
  }
  free (page);

  return (EXIT_SUCCESS);
}
