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

// Determine Display Set end time, call assemble_composition(), free/clear all data for this page_id/Display Set.
void
finalize_page_if_needed (STATE *state, PAGE *page, size_t page_idx, PES *pes) {

  uint64_t timeout_end, pts_end, final_end;

  // Return if current Display Set (page) is not complete.
  if (!page[page_idx].complete) {
    return;
  }

  // Calculate end time based on timeout.
  if (page[page_idx].time_out > 0) {
    timeout_end = page[page_idx].start.totalms + ((uint64_t) page[page_idx].time_out * 1000);
  } else {
    timeout_end = 0;
  }

  // Calculate end time based on PTS.
  // Note:  For the very last subtitle in a stream, there is often no final PES segment, and thus no new PTS.
  //        In that case, pes->pts will be the last PTS to arrive, and it's likely to be the same as the start timestamp.
  //        We therefore guard against end < start below.
  pts_end = pes->pts.totalms;

  // Choose the earlier of timeout_end and pts_end.
  if (pts_end < timeout_end) {
    final_end = pts_end;
  } else {
    final_end = timeout_end;
  }

  // Ensure end > start.
  if (final_end <= page[page_idx].start.totalms) {
    final_end = page[page_idx].start.totalms + 5000;  // Small safe fallback of 5 seconds
  }

  page[page_idx].end.totalms = final_end;
  mstotime (&page[page_idx].end);

  // Increment subtitle counter.
  state->nsubs++;

  // Render bitmap, if requested.
  if (state->makebmp_flag) assemble_composition (state, &page);

  // Clear page for next Display Set.
  clear_page (state, page);
}
