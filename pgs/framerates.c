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

// Look up framerate (fps) by framerate ID.
double
framerates (uint8_t id) {

  switch (id) {

    case 0x10:
      return (24000.0 / 1001.0);

    case 0x20:
      return (24.0);

    case 0x30:  // PAL progressive
      return (25.0);

    case 0x40:  // NTSC progressive
      return (30000.0 / 1001);

    case 0x50:
      return (30.0);

    case 0x60:  // PAL interlaced
      return (50.0);

    case 0x70:  // NTSC interlaced
      return (60000.0 / 1001.0);

    default:
      return (23.976);

  }
}
