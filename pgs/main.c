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

int
main (int argc, char **argv) {

  size_t i, j, suplen, index, nsubs;
  int fi;
  uint8_t *sup;
  TIME time;
  OPTIONS options;
  HEAD head;
  PALETTE *palette;
  OBJECT *object;
  SUB sub;
  STATE state;
  SYNC *sync;
  char *temp, *filename, *timestamp, *endptr;
  FILE *fo;

  // Allocate memory for various arrays.
  filename = allocate_strmem (MAX_STRINGLEN);

  // Process the command line arguments, if any.
  options.makebmp_flag = 0;  // Default to not make bitmap files.
  options.offset_flag = 0;  // Default to not applying PTS and DTS timestamp offsets.
  options.sync_flag = 0;  // Default to not resynchronizing PTS and DTS timestamps.
  if (argc == 2) {
    strncpy (filename, argv[1], MAX_STRINGLEN);

  } else if ((argc == 3) && (strncmp (argv[2], "bmp", 3) == 0)) {
    strncpy (filename, argv[1], MAX_STRINGLEN);
    options.makebmp_flag = 1;

  } else if ((argc == 3) && (strncmp (argv[2], "offset", 6) == 0)) {
    strncpy (filename, argv[1], MAX_STRINGLEN);
    options.offset_flag = 1;

  } else if ((argc == 3) && (strncmp (argv[2], "sync", 6) == 0)) {
    strncpy (filename, argv[1], MAX_STRINGLEN);
    options.sync_flag = 1;

  } else {
    fprintf (stdout, "\npgs - A tool to analyze a PGS (.sup) file and produce a report file.\n");
    fprintf (stdout, "      The .sup file can be extracted from BluRay or UHD BluRay (i.e., 1080p versus 2160p BDs).\n");
    fprintf (stdout, "      Optional functions include: - Producing a bitmap file for each subtitle, and\n");
    fprintf (stdout, "                                  - Synchronizing/offsetting all timestamps to new anchor-points.\n");
    fprintf (stdout, "\nBitmap notes:\n");
    fprintf (stdout, "  If the PGS (.sup) file contains subtitle fades (changing alpha) or scrolling (moving cropping windows), each step\n");
    fprintf (stdout, "  will produce a separate bitmap. i.e., this program will not attempt to find only the brightest step (in the case of fades),\n");
    fprintf (stdout, "  and it will not find the non-cropped step. All steps will have a bitmap produced.\n");
    fprintf (stdout, "  Bitmap filenames are: start and end times (hh_mm_ss_ms__hh_mm_ss_ms), and then .bmp.\n");
    fprintf (stdout, "  SubtitleEdit can read the timestamps from the filenames.\n");
    fprintf (stdout, "\nOffset notes:\n");
    fprintf (stdout, "  The user can supply positive or negative hours, minutes, seconds, and millisecond offsets to be applied to all PTS and DTS timestamps in out.sup file.\n");
    fprintf (stdout, "\nSynchronization notes:\n");
    fprintf (stdout, "  Subtitle durations are preserved.\n");
    fprintf (stdout, "  Synchronization of all PGS timestamps in a PGS (.sup) file is accomplished using \"first\" and \"last\" timestamps as anchor-points.\n");
    fprintf (stdout, "  Choose \"first\" and \"last\" subtitles that are near or at beginning and end of the feature in order to maximize scaling accuracy.\n");
    fprintf (stdout, "  Use pgs to produce bitmaps (which have timestamp filenames) or use SubtitleEdit to obtain the existing subtitle timestamps.\n");
    fprintf (stdout, "\nBuild: make\n");
    fprintf (stdout, "\nUsage: ./pgs inputfilename [option]\n");
    fprintf (stdout, "\nOptions:\n");
    fprintf (stdout, "  bmp      Produce a bitmap file for each subtitle.\n");
    fprintf (stdout, "  offset   Apply user-input offsets to all subtitle timestamps.\n");
    fprintf (stdout, "           Produce PGS file out.sup with offset subtitle timestamps.\n");
    fprintf (stdout, "  sync     Synchronize all subtitle timestamps to user-input anchor points.\n");
    fprintf (stdout, "           Produce PGS file out.sup with resynchronized subtitle timestamps.\n");
    fprintf (stdout, "\nOutput: pgs.out, and optionally: bitmap file for each subtitle, or resynchronized/offset out.sup file.\n");
    fprintf (stdout, "\nReferences: U.S. Patents US20090185789A1, US7912305B1\n\n");
    free (filename);
    return (EXIT_SUCCESS);
  }

  // Allocate memory for various arrays.
  temp = allocate_strmem (MAX_STRINGLEN);
  timestamp = allocate_strmem (MAX_STRINGLEN);
  options.change = allocate_changemem (MAX_CHANGES);
  palette = allocate_pdsmem (MAX_PALETTES);
  for (i = 0; i < MAX_PALETTES; i++) {
    palette[i].entry = allocate_palentrymem (MAX_PALETTE_ENTRIES);
  }
  object = allocate_objmem (MAX_OBJECTS);
  for (i = 0; i < MAX_OBJECTS; i++) {
    object->buffer = NULL;  // We will allocate dynamically in parse_ods().
  }
  sub.buffer = allocate_u8mem (3840 * 2160 * 4);  // 2160p UHD BluRay video resolution * RGBA
  sync = allocate_syncmem (MAX_SUBS);

  // Initializations.
  options.nchanges = 0;
  for (i = 0; i < MAX_OBJECTS; i++) {
    object[i].buffer = NULL;
    object[i].length = 0;
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

  // Open .sup input file.
  // Memory-map the file rather than loading it into an array.
  fi = open (filename, O_RDONLY);
  if (fi == -1) {
    fprintf (stderr, "Unable to open input file %s.\n", filename);
    exit (EXIT_FAILURE);
  }
  struct stat st;

  // Get file size.
  if (fstat (fi, &st) == -1) {
    perror ("fstat() failed.\n");
    close (fi);
    exit (EXIT_FAILURE);
  }
  suplen = st.st_size;

  // Memory-map the .sup file.
  sup = mmap (NULL, suplen, PROT_READ, MAP_PRIVATE, fi, 0);
  if (sup == MAP_FAILED) {
    perror ("mmap() failed.\n");
    close (fi);
    exit (EXIT_FAILURE);
  }

  // Open output (report) file pgs.out.
  fo = fopen ("pgs.out", "r");
  if (fo != NULL) {
    fprintf (stderr, "Output file pgs.out already exists.\n");
    fclose (fo);
    exit (EXIT_FAILURE);
  }
  fo = fopen ("pgs.out", "w");
  if (fo == NULL) {
    fprintf (stderr, "Can't open output file pgs.out.\n");
    exit (EXIT_FAILURE);
  }

  // Ask for current and new start timestamps for anchor points, if resynchronization is requested.
  if (options.sync_flag) {

    // Ask for current timestamp of first PGS subtitle.
    fprintf (stdout, "\nCurrent start timestamp for first PGS subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.oldfirstms = time.totalms;

    // Ask for current timestamp of last PGS subtitle.
    fprintf (stdout, "Current start timestamp for last PGS subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.oldlastms = time.totalms;

    // Ask for new timestamp of first PGS subtitle.
    fprintf (stdout, "New start timestamp for first PGS subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.newfirstms = time.totalms;

    // Ask for new timestamp of last PGS subtitle.
    fprintf (stdout, "New start timestamp for last PGS subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.newlastms = time.totalms;

    if ((options.newlastms - options.newfirstms) <= 0) {
      fprintf (stderr, "New timestamps: last - first <= 0.\n");
      exit (EXIT_FAILURE);
    }

  }  // End if options.sync

  // Report PGS filename.
  fprintf (fo, "File: %s (%zu bytes)\n\n", filename, suplen);

  // Prescan PGS file to determine subtitle durations, needed for syncing.
  for (state.prescan = 1; state.prescan >= 0; state.prescan--) {

    index = 0;  // Index of sup file
    nsubs = 0;  // Count of subtitles
    state.seq_flag = 0;  // Initially assume not in a sequence of ODSs.
    head.segment_type = 0;  // Initially set to invalid segment type.
    state.num_objects = 0;
    state.object_id = 0;
    state.prev_object_id = 0;
    state.subtitle_active = 0;

    // Loop through all segments in sup file.
    while (index < suplen) {

      state.pts_type = PTS_MIDDLE;

      // Segment Header
      parse_header (&state, sup, suplen, &index, &options, &head, &sync[nsubs], fo);

      switch (head.segment_type) {

        case 0x14:  // Palette Definition Segment (PDS)
          parse_pds (&state, sup, suplen, &index, &head, palette, fo);
          break;

        case 0x15:  // Object Definition Segment (ODS)
          parse_ods (&state, sup, suplen, &index, &head, palette, object, &sub, fo);
          break;

        case 0x16:  // Presentation Composition Segment (PCS); States: 0x00 = Normal, 0x40 = Acquisition, 0x80 = Epoch Start, 0xc0 = Epoch Continue
          parse_pcs (&state, sup, suplen, &index, &head, object, palette, fo);

          // Acquisition or Epoch Start
          if (state.composition_state == 0x80 || state.composition_state == 0x40) {

            // If a subtitle is currently active, record active subtitle's end timestamp.
            if (state.subtitle_active) {
              sub.end = state.pts;
              state.pts_type = PTS_END;
              sync[nsubs].end.totalms = sub.end.totalms;  // Record end timestamp for sync option, if requested.
              nsubs++;

              // Create bitmap, if requested.
              if (!state.prescan && options.makebmp_flag) write_bmp (&sub);
            }

            // Record new subtitle's start timestamp.
            sub.start = state.pts;
            state.pts_type = PTS_START;
            sync[nsubs].start.totalms = sub.start.totalms;  // Record start timestamp for sync option, if requested.

            // Set subtitle_active state to 1 if number of active objects is > 0.
            state.subtitle_active = (state.num_objects > 0);
          }

          // Start new subtitle.
          if (!state.subtitle_active && (state.num_objects > 0)) {

            // Record new subtitle's start timestamp.
            sub.start = state.pts;
            state.pts_type = PTS_START;
            sync[nsubs].start.totalms = sub.start.totalms;  // Record start timestamp for sync option, if requested.

            state.subtitle_active = 1;

          // Object changed; start new subtitle.
          } else if (state.subtitle_active && (state.num_objects > 0) && (state.object_id != state.prev_object_id)) {

            // Record active subtitle's end timestamp.
            sub.end = state.pts;
            state.pts_type = PTS_END;
            sync[nsubs].end.totalms = sub.end.totalms;  // Record end timestamp for sync option, if requested.
            nsubs++;

            // Create bitmap, if requested.
            if (!state.prescan && options.makebmp_flag) write_bmp (&sub);

            // Record new subtitle's start timestamp.
            sub.start = state.pts;
            state.pts_type = PTS_START;
            sync[nsubs].start.totalms = sub.start.totalms;  // Record start timestamp for sync option, if requested.

          // Clear screen.
          } else if (state.subtitle_active && (state.num_objects == 0)) {

            // Record active subtitle's end timestamp.
            sub.end = state.pts;
            state.pts_type = PTS_END;
            sync[nsubs].end.totalms = sub.end.totalms;  // Record end timestamp for sync option, if requested.
            nsubs++;

            // Create bitmap, if requested.
            if (!state.prescan && options.makebmp_flag) write_bmp (&sub);

            state.subtitle_active = 0;
          }

          // Save object_id.
          state.prev_object_id = state.object_id;
          break;

        case 0x17:  // Window Definition Segment (WDS)
          parse_wds (&state, sup, suplen, &index, &head, fo);
          break;

        case 0x80:  // End of Display Segment (DS)
          break;

        default:
          fprintf (stderr, "Unknown Segment Type value: 0x%02x\n", head.segment_type);
          exit (EXIT_FAILURE);
      }

      if (!state.prescan) fprintf (fo, "\n");

    }  // Next segment
  }  // Next state.prescan

  // Close output file pgs.out.
  fclose (fo);

  if ((options.offset_flag) || (options.sync_flag)) {

    // Open output file for revised .sup file.
    fo = fopen ("out.sup", "r");
    if (fo != NULL) {
      fprintf (stderr, "Output file out.sup already exists.\n");
      fclose (fo);
      exit (EXIT_FAILURE);
    }
    fo = fopen ("out.sup", "wb");
    if (fo == NULL) {
      fprintf (stderr, "Can't open output file out.sup.\n");
      exit (EXIT_FAILURE);
    }

    // Write revised .sup file.
    j = 0;  // Index of change array
    for (i = 0; i < suplen; i++) {
      if (i == options.change[j].offset) {
        fputc (options.change[j].new_value, fo);
        j++;
      } else {
        fputc (sup[i], fo);
      }
    }

    // Close output file.
    fclose (fo);

  }  // End if options.offset_flag || options.sync_flag

  fprintf (stdout, "\n%zu subtitles found in %s.\n\n", nsubs, filename);

  // Unmap and close .sup input file.
  munmap (sup, suplen);
  close (fi);

  // Free allocated memory.
  free (temp);
  free (filename);
  free (timestamp);
  free (options.change);
  for (i = 0; i < (size_t) MAX_PALETTES; i++) {
    free (palette[i].entry);
  }
  free (palette);
  for (i = 0; i < MAX_OBJECTS; i++) {
    if (object[i].buffer != NULL) free (object[i].buffer);
  }
  free (object);
  free (sub.buffer);
  free (sync);

  return (EXIT_SUCCESS);
}
