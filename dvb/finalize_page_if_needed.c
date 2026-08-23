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

// Determine the end timestamp of a completed Display Set, optionally render
// it to a bitmap, and then release the per-Display-Set data for this page.
//
// The page index is passed explicitly. A transport stream may carry several
// subtitle page_ids, so relying on state->page_id here could finalize the
// wrong page when EOF causes all outstanding pages to be flushed.
void
finalize_page_if_needed (STATE *s, PAGE *page, size_t p, PES *pes) {

  int64_t start, to = 0, pts, end;

  // An END segment marks the Display Set complete. Leave an in-progress page
  // untouched until that condition has been seen.
  if (!page[p].complete) return;

  // A PCS page_time_out supplies one possible end time. Guard the conversion
  // from seconds to milliseconds against signed overflow.
  start = page[p].start.totalms;
  if (page[p].time_out && start <= INT64_MAX - (int64_t) page[p].time_out * 1000) to = start + (int64_t) page[p].time_out * 1000;

  // The PTS of the next Display Set is normally the other candidate end time.
  // Use whichever valid candidate occurs first.
  pts = pes->pts.totalms;
  if (to > start && pts > start) end = pts < to ? pts : to;
  else if (pts > start) end = pts;
  else if (to > start) end = to;

  // The final subtitle in a stream may have no following PES/PTS. If neither
  // normal end time is usable, give it a conservative five-second duration.
  else end = start <= INT64_MAX - 5000 ? start + 5000 : INT64_MAX;

  page[p].end.totalms = end;
  mstotime (&page[p].end);
  s->nsubs++;

  // assemble_composition() also writes the BMP when bitmap output is enabled.
  if (s->makebmp_flag && assemble_composition (s, &page, p) != EXIT_SUCCESS) fprintf (stderr, "Could not assemble page_id 0x%04x.\n", page[p].page_id);

  // The PAGE slot remains available for the next Display Set of this page_id.
  clear_page (page, p);
}
