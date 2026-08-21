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

static uint32_t
clamp_ticks (int64_t ticks) {

  if (ticks < 0) return 0u;
  if ((uint64_t) ticks > UINT32_MAX) return UINT32_MAX;
  return (uint32_t) ticks;
}

static int64_t
scale_ticks (uint32_t ticks, OPTIONS *options) {

  double ratio, old_first_ticks, new_first_ticks, scaled;

  old_first_ticks = (double) options->oldfirstms * 90.0;
  new_first_ticks = (double) options->newfirstms * 90.0;
  ratio = ((double) (options->newlastms - options->newfirstms)) /
          ((double) (options->oldlastms - options->oldfirstms));
  scaled = new_first_ticks + (((double) ticks - old_first_ticks) * ratio);

  if (scaled <= 0.0) return 0;
  if (scaled >= (double) UINT32_MAX) return (int64_t) UINT32_MAX;
  return (int64_t) llround (scaled);
}

// Record revised PTS/DTS values after the current segment has been parsed and the
// role of its PTS (start/end/middle) is therefore known.
int
adjust_timestamps (STATE *state, HEAD *head, OPTIONS *options, SYNC *sync) {

  int64_t new_pts, new_dts, decode_lead;

  if (state->prescan || (!options->offset_flag && !options->sync_flag)) return (EXIT_SUCCESS);

  if (options->offset_flag) {
    new_pts = (int64_t) head->pts_ticks + options->offset_ticks;
    new_dts = (int64_t) head->dts_ticks + options->offset_ticks;

  } else {
    // Scale ordinary timestamps in the 90-kHz domain, retaining the full input precision.
    // For an ending PCS, preserve the original subtitle duration exactly. A PCS that both
    // ends one presentation and starts another uses the preserved end time; the shared PTS
    // cannot independently satisfy two different transformations.
    if (state->pts_type == PTS_END || state->pts_type == PTS_END_START) {
      int64_t scaled_start, duration;

      if (sync == NULL || sync->end_ticks < sync->start_ticks) {
        fprintf (stderr, "Invalid synchronization interval in adjust_timestamps().\n");
        exit (EXIT_FAILURE);
      }
      scaled_start = scale_ticks (sync->start_ticks, options);
      duration = (int64_t) sync->end_ticks - (int64_t) sync->start_ticks;
      new_pts = scaled_start + duration + ((int64_t) head->pts_ticks - (int64_t) sync->end_ticks);
    } else {
      new_pts = scale_ticks (head->pts_ticks, options);
    }

    // Preserve the original decoding lead/lag relative to this segment's PTS.
    decode_lead = (int64_t) head->dts_ticks - (int64_t) head->pts_ticks;
    new_dts = new_pts + decode_lead;
  }

  record_u32_change (options, head->pts_offset, clamp_ticks (new_pts));
  record_u32_change (options, head->dts_offset, clamp_ticks (new_dts));

  return (EXIT_SUCCESS);
}
