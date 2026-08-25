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

#include "teletext.h"

// Release every dynamically allocated parser object. Keeping cleanup in one
// helper lets error paths return directly without duplicating long free loops.
static void
free_parser_state (PAT *pat, TTX_CONTEXT *ttx, SECTION *section, SEGMENT *segment, PES *pes) {

  size_t i;

  if (pat) {
    for (i = 0; i < pat->nprograms; i++) free (pat->program[i].pmt.stream);
    free (pat->program);
  }

  if (ttx) {
    free (ttx->active_page);
    free (ttx->page);
  }

  if (section) {
    for (i = 0; i < MAX_PIDS; i++) free (section[i].buffer);
    free (section);
  }

  if (segment) {
    for (i = 0; i < MAX_PIDS; i++) free (segment[i].buffer);
    free (segment);
  }

  free (pes);
}

int
main (int argc, char **argv) {

  size_t i, tslen, packet;
  int fi, status;
  char filename[MAX_STRINGLEN];
  uint8_t *tsdata;
  STATE state;
  PAT pat;
  TTX_CONTEXT ttx;
  SECTION *section;
  SEGMENT *segment;
  PES *pes;
  FILE *fo;
  struct stat st;

  memset (&state, 0, sizeof (state));
  memset (&pat, 0, sizeof (pat));
  memset (&ttx, 0, sizeof (ttx));

  if (argc == 2) {
    state.write_text_flag = 0;
  } else if (argc == 3 && strcmp (argv[2], "text") == 0) {
    state.write_text_flag = 1;
  } else {
    fprintf (stdout, "\nteletext - Analyze DVB Teletext carried in an MPEG-2 transport stream.\n");
    fprintf (stdout, "\nBuild: make\n");
    fprintf (stdout, "\nUsage: ./teletext file.ts [option]\n");
    fprintf (stdout, "\nOptions:\n");
    fprintf (stdout, "  text    Extract Level 1 page text as UTF-8.\n");
    fprintf (stdout, "\nOutput:\n");
    fprintf (stdout, "  teletext.out      Detailed TS/PSI/PES/Teletext analyzer report.\n");
    fprintf (stdout, "  teletext_pages/   With 'text': one file per PID/page/subpage,\n");
    fprintf (stdout, "                    containing its chronological transmissions.\n");
    fprintf (stdout, "\nReferences: ISO/IEC 13818-1, ETSI EN 300 472, ETSI EN 300 468, ETSI EN 300 706.\n\n");
    return (EXIT_SUCCESS);
  }

  if (snprintf (filename, sizeof (filename), "%s", argv[1]) >= (int) sizeof (filename)) {
    fprintf (stderr, "Input filename is too long.\n");
    return (EXIT_FAILURE);
  }

  memset (state.pid_type, PID_UNKNOWN, sizeof (state.pid_type));

  section = allocate_sectionmem (MAX_PIDS);
  segment = allocate_segmentmem (MAX_PIDS);
  pes = allocate_pesmem (MAX_PIDS);
  if (!section || !segment || !pes) {
    fprintf (stderr, "Unable to allocate parser state.\n");
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }
  for (i = 0; i < MAX_PIDS; i++) pes[i].total_length = PES_LEN_UNBOUNDED;

  if (MAX_PIDS > SIZE_MAX / (8 * sizeof (*ttx.active_page))) {
    fprintf (stderr, "Teletext active-page array size overflow.\n");
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }
  ttx.active_page = malloc (MAX_PIDS * 8 * sizeof (*ttx.active_page));
  if (!ttx.active_page) {
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }
  for (i = 0; i < MAX_PIDS * 8; i++) ttx.active_page[i] = -1;

  // Open TeleText MPEG-2 stream file.
  fi = open (filename, O_RDONLY);
  if (fi == -1) {
    fprintf (stderr, "Unable to open input file %s.\n", filename);
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }

  if (fstat (fi, &st) == -1) {
    status = errno;
    fprintf (stderr, "fstat() failed in main.c.\nError Message: %s\n", strerror (status));
    close (fi);
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }
  if (st.st_size <= 0 || (uintmax_t) st.st_size > SIZE_MAX) {
    fprintf (stderr, "Input file has an invalid size.\n");
    close (fi);
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }
  tslen = (size_t) st.st_size;
  if ((tslen % 188) != 0) {
    fprintf (stderr, "Input length (%zu) is not an exact multiple of 188-byte TS packets.\n", tslen);
    close (fi);
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }

  // Memory-map input file.
  tsdata = mmap (NULL, tslen, PROT_READ, MAP_PRIVATE, fi, 0);
  if (tsdata == MAP_FAILED) {
    status = errno;
    fprintf (stderr, "mmap() failed in main.c.\nError Message: %s\n", strerror (status));
    close (fi);
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }

  // Open output file for reporting analysis of TeleText MPEG-2 stream file.
  fo = fopen ("teletext.out", "wx");
  if (!fo) {
    fprintf (stderr, "Unable to create teletext.out: %s\n", strerror (errno));
    munmap (tsdata, tslen);
    close (fi);
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }
  fprintf (fo, "File: %s (%zu bytes)\n", filename, tslen);

  state.ts_index = 0;
  packet = 0;
  while (state.ts_index < tslen) {
    if (parse_ts_packet (&state, &ttx, &pat, tsdata, tslen, &packet, pes, section, segment, fo) != EXIT_SUCCESS) {
      fprintf (stderr, "Failed while parsing TS packet %zu.\n", packet);
      fclose (fo);
      munmap (tsdata, tslen);
      close (fi);
      free_parser_state (&pat, &ttx, section, segment, pes);
      return (EXIT_FAILURE);
    }
    packet++;
  }

  // A zero-length PES_packet_length is terminated by the next PUSI. If the
  // input file ends first, flush that final unbounded packet explicitly.
  for (i = 0; i < MAX_PIDS; i++) {
    if (pes[i].collecting && pes[i].total_length == PES_LEN_UNBOUNDED && segment[i].length > 0) {
      state.pid = (uint16_t) i;
      if (parse_pes_segment (&state, &ttx, &pat, segment, &pes[i], fo) != EXIT_SUCCESS) {
        fprintf (stderr, "Failed while flushing final PES on PID 0x%04zx.\n", i);
        fclose (fo);
        munmap (tsdata, tslen);
        close (fi);
        free_parser_state (&pat, &ttx, section, segment, pes);
        return (EXIT_FAILURE);
      }
    }
  }

  // Close output file.
  if (fclose (fo) == EOF) {
    munmap (tsdata, tslen);
    close (fi);
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }

  // Unmap input file from memory.
  if (munmap (tsdata, tslen) == -1) {
    status = errno;
    fprintf (stderr, "munmap() failed in main.c.\nError Message: %s\n", strerror (status));
    close (fi);
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }
  close (fi);

  if (state.write_text_flag && write_teletext_pages (&ttx, "teletext_pages") != EXIT_SUCCESS) {
    fprintf (stderr, "Failed while writing Teletext text files.\n");
    free_parser_state (&pat, &ttx, section, segment, pes);
    return (EXIT_FAILURE);
  }

  fprintf (stdout, "Found %zu Teletext page transmissions in %zu Teletext data units.\n", ttx.npages, ttx.nteletext_units);
  fprintf (stdout, "Hamming 8/4: %zu one-bit corrections, %zu uncorrectable fields.\n", ttx.nhamming_corrected, ttx.nhamming_errors);
  fprintf (stdout, "Character parity errors: %zu.\n", ttx.nparity_errors);
  if (state.write_text_flag) fprintf (stdout, "Extracted text is in teletext_pages/.\n");

  free_parser_state (&pat, &ttx, section, segment, pes);

  return (EXIT_SUCCESS);
}
