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

// Find the PAT program whose PMT is carried on pid. Return -1 when the PID is
// not one of the PMT PIDs listed in the current PAT.
int
find_program_by_pmt_pid (PAT *pat, uint16_t pid) {

  size_t i;

  for (i = 0; i < pat->nprograms; i++) if (pat->program[i].pmt_pid == pid) {
    if (i > (size_t) INT_MAX) exit (EXIT_FAILURE);
    return ((int) i);
  }

  return (-1);
}

// Locate the PMT elementary-stream record for a PID. A Teletext PID may be
// shared by several descriptor entries (for example several page/language
// combinations), so callers receive the stream record rather than one entry.
const PMT_STREAM *
find_pmt_stream_by_pid (PAT *pat, uint16_t pid) {

  size_t i, j;

  for (i = 0; i < pat->nprograms; i++) {
    for (j = 0; j < pat->program[i].pmt.nstreams; j++) {
      if (pat->program[i].pmt.stream[j].elementary_stream_pid == pid) {
        return (&pat->program[i].pmt.stream[j]);
      }
    }
  }

  return (NULL);
}

// Match a decoded page to the PMT Teletext descriptor. Descriptor magazine 0
// denotes magazine 8, while the page itself is stored internally as 1..8.
const TELETEXT_SERVICE *
find_teletext_service (PAT *pat, uint16_t pid, uint8_t magazine, uint8_t page_number) {

  const PMT_STREAM *stream;
  size_t i;
  uint8_t descriptor_magazine;

  stream = find_pmt_stream_by_pid (pat, pid);
  if (!stream) return (NULL);

  for (i = 0; i < stream->nteletext_services; i++) {
    descriptor_magazine = stream->teletext_service[i].magazine;
    if (descriptor_magazine == 0) descriptor_magazine = 8;
    if (descriptor_magazine == magazine && stream->teletext_service[i].page_number == page_number) {
      return (&stream->teletext_service[i]);
    }
  }

  return (NULL);
}
