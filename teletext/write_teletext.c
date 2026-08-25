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

// Positions whose glyphs are replaced by the selected G0 national option.
static const uint8_t national_positions[13] = {
  0x23, 0x24, 0x40, 0x5b, 0x5c, 0x5d, 0x5e,
  0x5f, 0x60, 0x7b, 0x7c, 0x7d, 0x7e
};

// Unicode approximations for the principal Level 1 Latin national subsets.
// The language advertised in the PMT descriptor disambiguates subsets which
// share the same C12-C14 option value in different national repertoires.
typedef enum {
  SUBSET_ENGLISH,
  SUBSET_GERMAN,
  SUBSET_SFH,
  SUBSET_ITALIAN,
  SUBSET_FRENCH,
  SUBSET_SPANISH_PORTUGUESE,
  SUBSET_CZECH_SLOVAK,
  SUBSET_ESTONIAN,
  SUBSET_POLISH,
  SUBSET_LATVIAN_LITHUANIAN,
  SUBSET_ROMANIAN,
  SUBSET_SLOVENIAN_SERBIAN_CROATIAN,
  SUBSET_TURKISH
} NATIONAL_SUBSET;

// Replacement Unicode code points for the 13 national-option positions.
// Each row corresponds to one NATIONAL_SUBSET value above, and each column
// corresponds to the same index in national_positions[].
static const uint32_t national_table[][13] = {
  // English
  {0x00a3, '$', '@', 0x2190, 0x00bd, 0x2192, 0x2191, '#', '-', 0x00bc, 0x2016, 0x00be, 0x00f7},
  // German
  {'#', '$', 0x00a7, 0x00c4, 0x00d6, 0x00dc, '^', '_', 0x00b0, 0x00e4, 0x00f6, 0x00fc, 0x00df},
  // Swedish / Finnish / Hungarian
  {'#', 0x00a4, 0x00c9, 0x00c4, 0x00d6, 0x00c5, 0x00dc, '_', 0x00e9, 0x00e4, 0x00f6, 0x00e5, 0x00fc},
  // Italian
  {0x00a3, '$', 0x00e9, 0x00b0, 0x00e7, 0x2192, 0x2191, '#', 0x00f9, 0x00e0, 0x00f2, 0x00e8, 0x00ec},
  // French
  {0x00e9, 0x00ef, 0x00e0, 0x00eb, 0x00ea, 0x00f9, 0x00ee, '#', 0x00e8, 0x00e2, 0x00f4, 0x00fb, 0x00e7},
  // Spanish / Portuguese
  {0x00e7, '$', 0x00a1, 0x00e1, 0x00e9, 0x00ed, 0x00f3, 0x00fa, 0x00bf, 0x00fc, 0x00f1, 0x00e8, 0x00e0},
  // Czech / Slovak
  {'#', 0x016f, 0x010d, 0x0165, 0x017e, 0x00fd, 0x00ed, 0x0159, 0x00e9, 0x00e1, 0x011b, 0x00fa, 0x0161},
  // Estonian
  {'#', 0x00f5, 0x0160, 0x00c4, 0x00d6, 0x017d, 0x00dc, 0x00d5, 0x0161, 0x00e4, 0x00f6, 0x017e, 0x00fc},
  // Polish
  {'#', 0x0144, 0x0105, 0x017b, 0x015a, 0x0141, 0x0107, 0x00f3, 0x0119, 0x017a, 0x015b, 0x0142, 0x017c},
  // Latvian / Lithuanian
  {'#', '$', 0x0160, 0x0117, 0x0119, 0x017d, 0x010d, 0x016b, 0x0161, 0x0105, 0x0173, 0x017e, 0x012f},
  // Romanian (modern comma-below Unicode forms where appropriate)
  {'#', 0x00a4, 0x021a, 0x00c2, 0x0218, 0x0102, 0x00ce, 0x0131, 0x021b, 0x00e2, 0x0219, 0x0103, 0x00ee},
  // Slovenian / Serbian / Croatian
  {'#', 0x00cb, 0x010c, 0x0106, 0x017d, 0x0110, 0x0160, 0x00eb, 0x010d, 0x0107, 0x017e, 0x0111, 0x0161},
  // Turkish
  {0x20ba, 0x011f, 0x0130, 0x015e, 0x00d6, 0x00c7, 0x00dc, 0x011e, 0x0131, 0x015f, 0x00f6, 0x00e7, 0x00fc}
};

// Determine which national G0 character subset should be used for a page.
// Prefer the ISO 639 language code obtained from the PMT Teletext descriptor;
// if that information is absent or unrecognized, fall back to the three-bit
// national-option value carried in the Teletext page header.
static NATIONAL_SUBSET
select_subset (const TTX_PAGE *page) {

  const char *lang = page->language;

  // The PMT descriptor is the most useful discriminator because several
  // national repertoires can use the same three-bit Teletext option value.
  if (!strcmp (lang, "deu") || !strcmp (lang, "ger")) return (SUBSET_GERMAN);
  if (!strcmp (lang, "swe") || !strcmp (lang, "fin") || !strcmp (lang, "hun")) return (SUBSET_SFH);
  if (!strcmp (lang, "ita")) return (SUBSET_ITALIAN);
  if (!strcmp (lang, "fra") || !strcmp (lang, "fre")) return (SUBSET_FRENCH);
  if (!strcmp (lang, "spa") || !strcmp (lang, "por")) return (SUBSET_SPANISH_PORTUGUESE);
  if (!strcmp (lang, "ces") || !strcmp (lang, "cze") || !strcmp (lang, "slk") || !strcmp (lang, "slo")) return (SUBSET_CZECH_SLOVAK);
  if (!strcmp (lang, "est")) return (SUBSET_ESTONIAN);
  if (!strcmp (lang, "pol")) return (SUBSET_POLISH);
  if (!strcmp (lang, "lav") || !strcmp (lang, "lit")) return (SUBSET_LATVIAN_LITHUANIAN);
  if (!strcmp (lang, "ron") || !strcmp (lang, "rum")) return (SUBSET_ROMANIAN);
  if (!strcmp (lang, "slv") || !strcmp (lang, "srp") || !strcmp (lang, "hrv")) return (SUBSET_SLOVENIAN_SERBIAN_CROATIAN);
  if (!strcmp (lang, "tur")) return (SUBSET_TURKISH);

  // Basic EN 300 706 C12-C14 fallback when no useful language descriptor was
  // found. Option 7 is repertoire-dependent; English is the safest portable
  // fallback rather than inventing a country selection.
  switch (page->national_option) {
    case 1: return (SUBSET_GERMAN);
    case 2: return (SUBSET_SFH);
    case 3: return (SUBSET_ITALIAN);
    case 4: return (SUBSET_FRENCH);
    case 5: return (SUBSET_SPANISH_PORTUGUESE);
    case 6: return (SUBSET_CZECH_SLOVAK);
    case 0:
    case 7:
    default: return (SUBSET_ENGLISH);
  }
}

// Convert one Level 1 Teletext alphanumeric character to a Unicode code
// point. Most printable G0 characters map directly to ASCII; the 13 national
// option positions are replaced according to the subset selected for the page.
static uint32_t
map_character (const TTX_PAGE *page, uint8_t value) {

  size_t i;
  NATIONAL_SUBSET subset;

  // Control codes are spacing characters rather than printable glyphs.
  if (value < 0x20) return (' ');

  // 0x7f is the solid block character in the Level 1 alphanumeric G0 set.
  if (value == 0x7f) return (0x25a0);  // solid block in the alphanumeric set.

  subset = select_subset (page);
  for (i = 0; i < 13; i++) {
    if (value == national_positions[i]) return (national_table[subset][i]);
  }
  return (value);
}

// Encode one Unicode code point as UTF-8 and write it to the output stream.
// Return EXIT_FAILURE immediately if any byte cannot be written.
static int
write_utf8 (FILE *fo, uint32_t cp) {

  if (cp <= 0x7f) {
    return (fputc ((int) cp, fo) == EOF ? EXIT_FAILURE : EXIT_SUCCESS);
  }
  if (cp <= 0x7ff) {
    if (fputc ((int) (0xc0 | (cp >> 6)), fo) == EOF ||
        fputc ((int) (0x80 | (cp & 0x3f)), fo) == EOF) return (EXIT_FAILURE);
  } else if (cp <= 0xffff) {
    if (fputc ((int) (0xe0 | (cp >> 12)), fo) == EOF ||
        fputc ((int) (0x80 | ((cp >> 6) & 0x3f)), fo) == EOF ||
        fputc ((int) (0x80 | (cp & 0x3f)), fo) == EOF) return (EXIT_FAILURE);
  } else {
    if (fputc ((int) (0xf0 | (cp >> 18)), fo) == EOF ||
        fputc ((int) (0x80 | ((cp >> 12) & 0x3f)), fo) == EOF ||
        fputc ((int) (0x80 | ((cp >> 6) & 0x3f)), fo) == EOF ||
        fputc ((int) (0x80 | (cp & 0x3f)), fo) == EOF) return (EXIT_FAILURE);
  }
  return (EXIT_SUCCESS);
}

// Return a human-readable description of the Teletext page type advertised
// by the DVB Teletext descriptor in the PMT.
static const char *
teletext_type_name (uint8_t type) {

  switch (type) {
    case 0x01: return ("initial Teletext page");
    case 0x02: return ("Teletext subtitle page");
    case 0x03: return ("additional information page");
    case 0x04: return ("programme schedule page");
    case 0x05: return ("Teletext subtitle page for hearing impaired people");
    default: return ("not identified by PMT descriptor");
  }
}

// Test whether two saved transmissions belong to the same logical Teletext
// page. PID is part of the key because identical magazine/page/subpage numbers
// may legitimately occur on different elementary streams.
static int
same_page_key (const TTX_PAGE *a, const TTX_PAGE *b) {

  return (a->pid == b->pid && a->magazine == b->magazine && a->page_number == b->page_number && a->subcode == b->subcode);
}

// Compare the final Level 1 screen state and display-affecting metadata. Exact
// consecutive retransmissions can then be collapsed in the text output while
// their first/last transmission numbers and PTS range remain documented.
static int
same_page_state (const TTX_PAGE *a, const TTX_PAGE *b) {

  return (same_page_key (a, b) &&
          a->subtitle == b->subtitle &&
          a->suppress_header == b->suppress_header &&
          a->inhibit_display == b->inhibit_display &&
          a->national_option == b->national_option &&
          !memcmp (a->row_present, b->row_present, sizeof (a->row_present)) &&
          !memcmp (a->row, b->row, sizeof (a->row)));
}

// Write a decoded PTS value in both clock form and total milliseconds. If the
// PES packet carried no usable timestamp, explicitly record that fact instead.
static int
write_time (FILE *fo, const char *label, const TIME *time, uint8_t have_time) {

  if (!have_time) return (fprintf (fo, "%s: not present\n", label) < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
  return (fprintf (fo, "%s: %02d:%02d:%02d,%03d (%" PRId64 " ms)\n", label, time->h, time->m, time->s, time->ms, time->totalms) < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}

// Write one screen state as UTF-8. Teletext control codes are spacing
// characters. Alpha colour controls (0x00..0x07) select alphanumeric mode;
// mosaic colour controls (0x10..0x17) select graphics mode. Mosaic cells are
// represented by spaces because plain text cannot preserve their dot pattern.
static int
write_page_rows (FILE *fo, const TTX_PAGE *page) {

  size_t row, col, last_col, last_row;
  uint8_t value, alpha_mode, boxed, boxed_page;
  uint32_t cp, rendered[TELETEXT_COLUMNS];

  // C5 (newsflash) and C6 (subtitle) pages use Start Box/End Box to identify
  // the characters that are actually superimposed on the television picture.
  // Suppress characters outside those boxes in the plain-text representation.
  boxed_page = (uint8_t) (page->newsflash || page->subtitle);

  // Locate the final row containing non-space data so the text file does not
  // acquire a block of trailing blank lines for every 25-row Teletext screen.
  last_row = TELETEXT_ROWS;
  while (last_row > 0) {
    row = last_row - 1;
    if (page->row_present[row]) {
      for (col = 0; col < TELETEXT_COLUMNS; col++) {
        if (page->row[row][col] != ' ') break;
      }
      if (col < TELETEXT_COLUMNS) break;
    }
    last_row--;
  }

  // Preserve blank rows within the displayed area, but omit row 0 when the
  // page header explicitly requests suppression.
  for (row = 0; row < last_row; row++) {
    if (row == 0 && page->suppress_header) continue;
    if (!page->row_present[row]) {
      if (fputc ('\n', fo) == EOF) return (EXIT_FAILURE);
      continue;
    }

    // Each row begins in alphanumeric mode. Ordinary pages are visible from
    // the start of the row; subtitle/newsflash pages become visible only after
    // a Start Box control code.
    alpha_mode = 1;
    boxed = (uint8_t) !boxed_page;

    // Teletext control codes occupy a spacing cell. Display attributes are
    // processed before deciding how subsequent cells are represented.
    for (col = 0; col < TELETEXT_COLUMNS; col++) {
      value = page->row[row][col];
      if (value < 0x20) {
        if (value <= 0x07) alpha_mode = 1;
        else if (value >= 0x10 && value <= 0x17) alpha_mode = 0;

        // Start Box (0x0b) and End Box (0x0a) are set-after controls: the
        // control-code cell itself is a blank, and the state applies after it.
        rendered[col] = ' ';
        if (boxed_page && value == 0x0a) boxed = 0;
        else if (boxed_page && value == 0x0b) boxed = 1;
        continue;
      }

      // Plain text can represent only alphanumeric cells inside the displayed
      // subtitle/newsflash box. Graphics-mode mosaics and hidden cells become
      // spaces while retaining their column positions.
      if (!boxed || !alpha_mode) rendered[col] = ' ';
      else rendered[col] = map_character (page, value);
    }

    // Remove trailing spaces from each rendered row without disturbing
    // leading or embedded spaces, which are significant to the page layout.
    last_col = TELETEXT_COLUMNS;
    while (last_col > 0 && rendered[last_col - 1] == ' ') last_col--;
    for (col = 0; col < last_col; col++) {
      cp = rendered[col];
      if (write_utf8 (fo, cp) != EXIT_SUCCESS) return (EXIT_FAILURE);
    }
    if (fputc ('\n', fo) == EOF) return (EXIT_FAILURE);
  }
  return (EXIT_SUCCESS);
}

// Write one distinct page state to its page file. first identifies the state
// to render; last marks the final identical retransmission so the output can
// document the complete transmission-number and timestamp span.
static int
write_snapshot (FILE *fo, const TTX_PAGE *first, const TTX_PAGE *last,
                size_t repeats) {

  if (fprintf (fo, "\n============================================================\n") < 0 ||
      fprintf (fo, "Transmission: %zu", first->transmissions) < 0) return (EXIT_FAILURE);
  if (repeats > 1 && fprintf (fo, "-%zu (%zu identical transmissions collapsed)", last->transmissions, repeats) < 0) return (EXIT_FAILURE);
  if (fputc ('\n', fo) == EOF) return (EXIT_FAILURE);

  if (write_time (fo, "First PTS", &first->first_pts, first->have_first_pts) != EXIT_SUCCESS ||
      write_time (fo, "Last PTS", &last->last_pts, last->have_last_pts) != EXIT_SUCCESS) return (EXIT_FAILURE);

  if (fprintf (fo, "Flags: erase=%u newsflash=%u subtitle=%u suppress_header=%u update=%u interrupted=%u inhibit=%u serial=%u\n", first->erase_page, first->newsflash, first->subtitle, first->suppress_header, first->update_indicator, first->interrupted_sequence, first->inhibit_display, first->magazine_serial) < 0) return (EXIT_FAILURE);
  if (first->parity_errors > 0 && fprintf (fo, "Character parity errors in first transmission: %zu\n", first->parity_errors) < 0) return (EXIT_FAILURE);
  if (fputc ('\n', fo) == EOF) return (EXIT_FAILURE);

  if (write_page_rows (fo, first) != EXIT_SUCCESS) return (EXIT_FAILURE);

  return (EXIT_SUCCESS);
}

// Count all captured transmissions belonging to one PID/page/subpage key.
// This count is reported in index.txt even though identical consecutive states
// may later be collapsed in the per-page output file.
static size_t
count_page_key (const TTX_CONTEXT *ttx, const TTX_PAGE *key) {

  size_t i, n;

  n = 0;
  for (i = 0; i < ttx->npages; i++) {
    if (same_page_key (key, &ttx->page[i])) n++;
  }
  return (n);
}

// Produce one UTF-8 text file for each unique PID/page/subpage. Each file is a
// chronological record of that page's changing screen state. This preserves
// Teletext subtitles while avoiding one tiny file for every repeated header.
int
write_teletext_pages (TTX_CONTEXT *ttx, const char *directory) {

  int status;
  size_t i, j, k, repeats;
  uint8_t *done;
  char filename[MAX_STRINGLEN];
  FILE *fo, *index;
  TTX_PAGE *first, *last, *candidate;

  // Create the extraction directory if necessary. An existing directory is
  // acceptable, but individual output files are opened with "x" below so an
  // earlier extraction cannot be overwritten accidentally.
  if (mkdir (directory, 0755) == -1 && errno != EEXIST) {
    status = errno;
    fprintf (stderr, "mkdir() failed in write_teletext.c.\nError Message: %s\n", strerror (status));
    return (EXIT_FAILURE);
  }

  // done[] marks transmissions that have already been assigned to a page
  // file. Allocate one byte even for an empty capture so calloc (0, ...) is not
  // relied upon to return a usable pointer.
  done = calloc (ttx->npages ? ttx->npages : 1, 1);
  if (!done) return (EXIT_FAILURE);

  // index.txt provides a compact inventory of all unique extracted pages and
  // the file containing the chronological history for each one.
  if (snprintf (filename, sizeof (filename), "%s/index.txt", directory) >= (int) sizeof (filename)) {
    free (done);
    return (EXIT_FAILURE);
  }
  index = fopen (filename, "wx");
  if (!index) {
    fprintf (stderr, "Unable to create %s: %s\n", filename, strerror (errno));
    free (done);
    return (EXIT_FAILURE);
  }
  fprintf (index, "Extracted Teletext pages\n\n");
  fprintf (index, "Page  Subpage  PID     Lang  Type  Transmissions  File\n");
  fprintf (index, "----  -------  ------  ----  ----  -------------  ----\n");

  // The first unprocessed transmission of each unique key creates one output
  // file. Subsequent matching transmissions are collected into that same file.
  for (i = 0; i < ttx->npages; i++) {
    if (done[i]) continue;
    first = &ttx->page[i];

    // Include page, subpage, and PID in the filename so the name itself is
    // unambiguous even when different Teletext streams reuse a page number.
    if (snprintf (filename, sizeof (filename), "%s/P%u%02X_S%04X_PID%04X.txt", directory, first->magazine, first->page_number, first->subcode, first->pid) >= (int) sizeof (filename)) {
      free (done);
      return (EXIT_FAILURE);
    }

    fo = fopen (filename, "wx");
    if (!fo) {
      fprintf (stderr, "Unable to create %s: %s\n", filename, strerror (errno));
      fclose (index);
      free (done);
      return (EXIT_FAILURE);
    }

    fprintf (index, "%u%02X   %04X     0x%04X  %-4s  0x%02X  %-13zu  %s\n", first->magazine, first->page_number, first->subcode, first->pid, first->language[0] ? first->language : "und", first->teletext_type, count_page_key (ttx, first), filename + strlen (directory) + 1);

    if (fprintf (fo, "Teletext page %u%02X, subpage %04X\nPID: 0x%04X\nLanguage: %s\nType: %s\n", first->magazine, first->page_number, first->subcode, first->pid, first->language[0] ? first->language : "und", teletext_type_name (first->teletext_type)) < 0) {
      fclose (fo);
      fclose (index);
      free (done);
      return (EXIT_FAILURE);
    }

    // Visit all transmissions of this key in original stream order. Exact
    // consecutive states for this page are coalesced into one output block.
    j = i;
    while (j < ttx->npages) {
      while (j < ttx->npages && (done[j] || !same_page_key (first, &ttx->page[j]))) j++;
      if (j >= ttx->npages) break;

      first = &ttx->page[j];
      last = first;
      repeats = 1;
      done[j] = 1;

      // Look forward through the capture for the next transmissions of this
      // page key. Other page keys are skipped; coalescing stops at the first
      // later transmission of this page whose displayed state has changed.
      k = j + 1;
      while (k < ttx->npages) {
        if (!same_page_key (first, &ttx->page[k])) {
          k++;
          continue;
        }
        candidate = &ttx->page[k];
        if (!same_page_state (first, candidate)) break;
        last = candidate;
        repeats++;
        done[k] = 1;
        k++;
      }

      // Emit one block for this distinct screen state, including the range of
      // retransmissions represented when one or more duplicates were collapsed.
      if (write_snapshot (fo, first, last, repeats) != EXIT_SUCCESS) {
        fclose (fo);
        fclose (index);
        free (done);
        return (EXIT_FAILURE);
      }
      j = k;
    }

    // Close output file.
    if (fclose (fo) == EOF) {
      fclose (index);
      free (done);
      return (EXIT_FAILURE);
    }
  }

  if (fclose (index) == EOF) {
    free (done);
    return (EXIT_FAILURE);
  }
  free (done);

  return (EXIT_SUCCESS);
}
