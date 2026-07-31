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

// Record any PTS or DTS timestamp changes to change array.
void
record_pes_timestamp_change (OPTIONS *options, size_t pos, uint64_t ts90, uint8_t prefix) {

  options->change[options->nchanges].offset = pos;
  options->change[options->nchanges].new_value = prefix | ((ts90 >> 29) & 0x0e) | 0x01;
  options->nchanges++;

  options->change[options->nchanges].offset = pos + 1;
  options->change[options->nchanges].new_value = (ts90 >> 22) & 0xff;
  options->nchanges++;

  options->change[options->nchanges].offset = pos + 2;
  options->change[options->nchanges].new_value = ((ts90 >> 14) & 0xfe) | 0x01;
  options->nchanges++;

  options->change[options->nchanges].offset = pos + 3;
  options->change[options->nchanges].new_value = (ts90 >> 7) & 0xff;
  options->nchanges++;

  options->change[options->nchanges].offset = pos + 4;
  options->change[options->nchanges].new_value = ((ts90 & 0x7f) << 1) | 0x01;
  options->nchanges++;
}
