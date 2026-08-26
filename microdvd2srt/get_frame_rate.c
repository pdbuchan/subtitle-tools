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

#include "microdvd2srt.h"

// Ask the user for the video frame rate used by the MicroDVD subtitles.
//
// The same arbitrary-length readline() routine used for subtitle input is used
// here so the prompt has no fixed input-buffer limitation.
int
get_frame_rate (long double *frame_rate) {

  int status;
  char *line;

  if (frame_rate == NULL) {
    errno = EINVAL;
    return (-1);
  }

  fprintf (stdout, "Video frame rate (fps) (e.g., 25, 23.976, or 24000/1001): ");
  if (fflush (stdout) == EOF) {
    fprintf (stderr, "Unable to write frame-rate prompt: %s\n", strerror (errno));
    return (-1);
  }

  status = readline (stdin, &line);
  if (status < 0) {
    fprintf (stderr, "Unable to read video frame rate: %s\n", strerror (errno));
    return (-1);
  }

  if (status == 0) {
    fprintf (stderr, "No video frame rate was supplied.\n");
    return (-1);
  }

  if (parse_frame_rate (line, frame_rate) != 0) {
    fprintf (stderr, "Invalid video frame rate: %s\n", line);
    free (line);
    return (-1);
  }

  free (line);

  return (0);
}
