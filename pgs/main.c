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

static int parse_integer_input (const char *, const char *);
static void copy_filename (char *, const char *);
static int presentation_changed (uint8_t, uint16_t, size_t, const COMPOSITION_OBJECT *, const STATE *);
static void record_sub_start (SYNC *, size_t, SUB *, HEAD *);
static void record_sub_end (SYNC *, size_t, SUB *, HEAD *);

int
main (int argc, char **argv) {

  size_t i, suplen, index, nsubs = 0;
  int fi, pass, npasses;
  uint8_t *sup;
  TIME time;
  OPTIONS options = {0};
  HEAD head = {0};
  PALETTE *palette;
  OBJECT *object;
  SUB sub = {0};
  STATE state = {0};
  SYNC *sync;
  char *temp, *filename, *timestamp;
  FILE *fo;

  filename = allocate_strmem (MAX_STRINGLEN);

  // Process command-line arguments.
  if (argc == 2) {
    copy_filename (filename, argv[1]);
  } else if (argc == 3 && strcmp (argv[2], "bmp") == 0) {
    copy_filename (filename, argv[1]);
    options.makebmp_flag = 1;
  } else if (argc == 3 && strcmp (argv[2], "offset") == 0) {
    copy_filename (filename, argv[1]);
    options.offset_flag = 1;
  } else if (argc == 3 && strcmp (argv[2], "sync") == 0) {
    copy_filename (filename, argv[1]);
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
    fprintf (stdout, "  Subtitle durations are preserved when a PCS provides a distinct ending timestamp.\n");
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

  temp = allocate_strmem (MAX_STRINGLEN);
  timestamp = allocate_strmem (MAX_STRINGLEN);
  options.change = allocate_changemem (MAX_CHANGES);
  palette = allocate_pdsmem (MAX_PALETTES);
  for (i = 0; i < MAX_PALETTES; i++) palette[i].entry = allocate_palentrymem (MAX_PALETTE_ENTRIES);
  object = allocate_objmem (MAX_OBJECTS);
  sync = allocate_syncmem (MAX_SUBS);

  if (options.offset_flag) {
    fprintf (stdout, "\nOffset values can be positive or negative integers.\n\n");

    fprintf (stdout, "What is desired offset hours? ");
    inputtext (temp);
    options.offset.h = parse_integer_input (temp, "offset hours");

    fprintf (stdout, "What is desired offset minutes? ");
    inputtext (temp);
    options.offset.m = parse_integer_input (temp, "offset minutes");

    fprintf (stdout, "What is desired offset seconds? ");
    inputtext (temp);
    options.offset.s = parse_integer_input (temp, "offset seconds");

    fprintf (stdout, "What is desired offset milliseconds? ");
    inputtext (temp);
    options.offset.ms = parse_integer_input (temp, "offset milliseconds");
    fprintf (stdout, "\n");

    timetoms (&options.offset);
    if (options.offset.totalms > INT64_MAX / 90 || options.offset.totalms < INT64_MIN / 90) {
      fprintf (stderr, "Requested offset is too large to represent in 90-kHz ticks.\n");
      exit (EXIT_FAILURE);
    }
    options.offset_ticks = options.offset.totalms * 90;
  }

  // Open and memory-map .sup input file.
  fi = open (filename, O_RDONLY);
  if (fi == -1) {
    fprintf (stderr, "Unable to open input file %s.\n", filename);
    exit (EXIT_FAILURE);
  }

  {
    struct stat st;
    if (fstat (fi, &st) == -1) {
      perror ("fstat() failed");
      close (fi);
      exit (EXIT_FAILURE);
    }
    if (st.st_size <= 0) {
      fprintf (stderr, "Input file %s is empty.\n", filename);
      close (fi);
      exit (EXIT_FAILURE);
    }
    if ((uintmax_t) st.st_size > SIZE_MAX) {
      fprintf (stderr, "Input file is too large for this platform.\n");
      close (fi);
      exit (EXIT_FAILURE);
    }
    suplen = (size_t) st.st_size;
  }

  sup = mmap (NULL, suplen, PROT_READ, MAP_PRIVATE, fi, 0);
  if (sup == MAP_FAILED) {
    perror ("mmap() failed");
    close (fi);
    exit (EXIT_FAILURE);
  }

  // Open report output.
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

  if (options.sync_flag) {
    fprintf (stdout, "\nCurrent start timestamp for first PGS subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.oldfirstms = time.totalms;

    fprintf (stdout, "Current start timestamp for last PGS subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.oldlastms = time.totalms;

    fprintf (stdout, "New start timestamp for first PGS subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.newfirstms = time.totalms;

    fprintf (stdout, "New start timestamp for last PGS subtitle (hh:mm:ss,ms)? ");
    inputtext (timestamp);
    parse_timestamp (timestamp, &time);
    options.newlastms = time.totalms;

    if (options.oldlastms <= options.oldfirstms) {
      fprintf (stderr, "Current timestamps: last must be greater than first.\n");
      exit (EXIT_FAILURE);
    }
    if (options.newlastms <= options.newfirstms) {
      fprintf (stderr, "New timestamps: last must be greater than first.\n");
      exit (EXIT_FAILURE);
    }
  }

  fprintf (fo, "File: %s (%zu bytes)\n\n", filename, suplen);

  // Only synchronization needs a prescan. Report, bitmap, and offset modes use one pass.
  npasses = options.sync_flag ? 2 : 1;
  for (pass = 0; pass < npasses; pass++) {
    state.prescan = options.sync_flag && pass == 0;
    index = 0;
    nsubs = 0;
    options.nchanges = state.prescan ? options.nchanges : 0;

    clear_objects (object);
    clear_palettes (palette);
    memset (&state, 0, sizeof (state));
    state.prescan = options.sync_flag && pass == 0;
    memset (&head, 0, sizeof (head));
    sub.width = 0;
    sub.height = 0;

    PTS_TYPE display_pts_type = PTS_MIDDLE;
    SYNC *display_sync = NULL;

    while (index < suplen) {
      uint8_t old_num_objects = state.num_objects;
      uint16_t old_composition_number = state.composition_number;
      size_t old_palette = state.current_palette;
      COMPOSITION_OBJECT old_objects[MAX_COMPOSITION_OBJECTS];
      SYNC *adjust_sync = NULL;

      memcpy (old_objects, state.composition_object, sizeof (old_objects));
      state.pts_type = PTS_MIDDLE;

      parse_header (&state, sup, suplen, &index, &head, fo);

      switch (head.segment_type) {
        case 0x14:  // PDS
          parse_pds (&state, sup, suplen, &index, &head, palette, fo);
          break;

        case 0x15:  // ODS
          parse_ods (&state, sup, suplen, &index, &head, object, fo);
          break;

        case 0x16: {  // PCS
          int changed;

          parse_pcs (&state, sup, suplen, &index, &head, object, palette, fo);
          changed = presentation_changed (old_num_objects, old_composition_number,
                                          old_palette, old_objects, &state);

          // Acquisition/Epoch states are explicit presentation boundaries. Use an else-if
          // chain so such a PCS cannot also be processed a second time as a normal object change.
          if (state.composition_state != 0) {
            if (state.subtitle_active) {
              adjust_sync = &sync[nsubs];
              record_sub_end (sync, nsubs, &sub, &head);
              if (!state.prescan && options.makebmp_flag) write_bmp (&sub);
              nsubs++;

              if (state.num_objects > 0) {
                state.pts_type = PTS_END_START;
                record_sub_start (sync, nsubs, &sub, &head);
                state.subtitle_active = 1;
              } else {
                state.pts_type = PTS_END;
                state.subtitle_active = 0;
              }
            } else if (state.num_objects > 0) {
              state.pts_type = PTS_START;
              record_sub_start (sync, nsubs, &sub, &head);
              state.subtitle_active = 1;
            }

          } else if (!state.subtitle_active && state.num_objects > 0) {
            state.pts_type = PTS_START;
            record_sub_start (sync, nsubs, &sub, &head);
            state.subtitle_active = 1;

          } else if (state.subtitle_active && state.num_objects == 0) {
            state.pts_type = PTS_END;
            adjust_sync = &sync[nsubs];
            record_sub_end (sync, nsubs, &sub, &head);
            if (!state.prescan && options.makebmp_flag) write_bmp (&sub);
            nsubs++;
            state.subtitle_active = 0;

          } else if (state.subtitle_active && state.num_objects > 0 && changed) {
            state.pts_type = PTS_END_START;
            adjust_sync = &sync[nsubs];
            record_sub_end (sync, nsubs, &sub, &head);
            if (!state.prescan && options.makebmp_flag) write_bmp (&sub);
            nsubs++;
            record_sub_start (sync, nsubs, &sub, &head);
          }

          // All following functional segments through END belong to this PCS display set.
          display_pts_type = state.pts_type;
          display_sync = adjust_sync;
          break;
        }

        case 0x17:  // WDS
          parse_wds (&state, sup, suplen, &index, &head, fo);
          break;

        case 0x80:  // END of display set
          if (head.segment_size != 0) {
            fprintf (stderr, "END segment has non-zero payload size %zu.\n", head.segment_size);
            exit (EXIT_FAILURE);
          }
          if (!state.prescan && options.makebmp_flag && state.subtitle_active) {
            render_subtitle (&state, palette, object, &sub);
          }
          break;

        default:
          fprintf (stderr, "Unknown Segment Type value: 0x%02x\n", head.segment_type);
          exit (EXIT_FAILURE);
      }

      if (head.segment_type != 0x16) {
        state.pts_type = display_pts_type;
        adjust_sync = display_sync;
      }

      if (!state.prescan && (options.offset_flag || options.sync_flag)) {
        adjust_timestamps (&state, &head, &options, adjust_sync);
      }

      if (head.segment_type == 0x80) {
        display_pts_type = PTS_MIDDLE;
        display_sync = NULL;
      }

      if (index != head.segment_end) {
        fprintf (stderr, "Parser ended at file offset %zu, but current segment ends at %zu.\n",
                 index, head.segment_end);
        exit (EXIT_FAILURE);
      }

      if (!state.prescan) fprintf (fo, "\n");
    }
  }

  fclose (fo);

  if (options.offset_flag || options.sync_flag) {
    uint8_t *revised;

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

    revised = malloc (suplen);
    if (revised == NULL) {
      fprintf (stderr, "Cannot allocate revised .sup output buffer.\n");
      exit (EXIT_FAILURE);
    }
    memcpy (revised, sup, suplen);

    for (i = 0; i < options.nchanges; i++) {
      if (options.change[i].offset >= suplen) {
        fprintf (stderr, "Timestamp change offset %zu lies outside the input file.\n", options.change[i].offset);
        exit (EXIT_FAILURE);
      }
      revised[options.change[i].offset] = options.change[i].new_value;
    }

    if (fwrite (revised, 1, suplen, fo) != suplen) {
      fprintf (stderr, "Failed while writing out.sup.\n");
      exit (EXIT_FAILURE);
    }
    free (revised);
    if (fclose (fo) != 0) {
      fprintf (stderr, "Failed to close out.sup cleanly.\n");
      exit (EXIT_FAILURE);
    }
  }

  fprintf (stdout, "\n%zu subtitles found in %s.\n\n", nsubs, filename);

  munmap (sup, suplen);
  close (fi);

  free (temp);
  free (filename);
  free (timestamp);
  free (options.change);
  for (i = 0; i < MAX_PALETTES; i++) free (palette[i].entry);
  free (palette);
  clear_objects (object);
  free (object);
  free (sub.buffer);
  free (sync);

  return (EXIT_SUCCESS);
}

static int
parse_integer_input (const char *text, const char *label) {

  char *endptr;
  long value;

  errno = 0;
  value = strtol (text, &endptr, 10);
  if (errno == ERANGE || endptr == text || *endptr != '\0' || value < INT_MIN || value > INT_MAX) {
    fprintf (stderr, "Cannot make integer of %s: %s\n", label, text);
    exit (EXIT_FAILURE);
  }

  return (int) value;
}

static void
copy_filename (char *dest, const char *src) {
  
  int n = snprintf (dest, MAX_STRINGLEN, "%s", src);
  if (n < 0 || n >= MAX_STRINGLEN) {
    fprintf (stderr, "Input filename is too long; maximum is %d characters.\n", MAX_STRINGLEN - 1);
    exit (EXIT_FAILURE);
  } 
}   
    
static int
presentation_changed (uint8_t old_num_objects, uint16_t old_composition_number, size_t old_palette, const COMPOSITION_OBJECT *old_objects, const STATE *state) {  
    
  size_t nbytes;

  if (old_num_objects != state->num_objects) return 1;
  if (old_composition_number != state->composition_number) return 1;
  if (old_palette != state->current_palette) return 1;
  if (state->palette_update_flag) return 1;
    
  nbytes = (size_t) state->num_objects * sizeof (COMPOSITION_OBJECT);
  if (nbytes > 0 && memcmp (old_objects, state->composition_object, nbytes) != 0) return 1;
    
  return 0; 
}   
    
static void 
record_sub_start (SYNC *sync, size_t nsubs, SUB *sub, HEAD *head) {
    
  if (nsubs >= MAX_SUBS) {
    fprintf (stderr, "Exceeded MAX_SUBS while recording subtitle start times.\n");
    exit (EXIT_FAILURE);
  } 
    
  sub->start = head->pts;
  sync[nsubs].start = head->pts;
  sync[nsubs].start_ticks = head->pts_ticks;
} 
  
static void 
record_sub_end (SYNC *sync, size_t nsubs, SUB *sub, HEAD *head) {
  
  if (nsubs >= MAX_SUBS) {
    fprintf (stderr, "Exceeded MAX_SUBS while recording subtitle end times.\n");
    exit (EXIT_FAILURE);
  } 
    
  sub->end = head->pts;
  sync[nsubs].end = head->pts;
  sync[nsubs].end_ticks = head->pts_ticks; 
    
  if (sync[nsubs].end_ticks < sync[nsubs].start_ticks) {
    fprintf (stderr, "Subtitle %zu has an end timestamp before its start timestamp.\n", nsubs + 1u);
    exit (EXIT_FAILURE);
  }
}
