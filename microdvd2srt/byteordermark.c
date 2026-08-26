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

// Byte Order Mark (BOM) byte sequences.
static const uint8_t utf8[]       = {0xef, 0xbb, 0xbf};
static const uint8_t utf16be[]    = {0xfe, 0xff};
static const uint8_t utf16le[]    = {0xff, 0xfe};
static const uint8_t utf32be[]    = {0x00, 0x00, 0xfe, 0xff};
static const uint8_t utf32le[]    = {0xff, 0xfe, 0x00, 0x00};
static const uint8_t utf7_1[]     = {0x2b, 0x2f, 0x76, 0x38};
static const uint8_t utf7_2[]     = {0x2b, 0x2f, 0x76, 0x39};
static const uint8_t utf7_3[]     = {0x2b, 0x2f, 0x76, 0x2b};
static const uint8_t utf7_4[]     = {0x2b, 0x2f, 0x76, 0x2f};
static const uint8_t utf1[]       = {0xf7, 0x64, 0x4c};
static const uint8_t utfebcdic[]  = {0xdd, 0x73, 0x66, 0x73};
static const uint8_t scsu[]       = {0x0e, 0xfe, 0xff};
static const uint8_t bocu1[]      = {0xfb, 0xee, 0x28};
static const uint8_t gb18030[]    = {0x84, 0x31, 0x95, 0x33};

// Table of recognized Byte Order Marks. Four distinct UTF-7 signatures are
// retained because the three-byte prefix +/v is not itself a complete BOM.
const BOM bom[] = {
  {sizeof (utf8),      "UTF-8",        utf8},
  {sizeof (utf16be),   "UTF-16 (BE)",  utf16be},
  {sizeof (utf16le),   "UTF-16 (LE)",  utf16le},
  {sizeof (utf32be),   "UTF-32 (BE)",  utf32be},
  {sizeof (utf32le),   "UTF-32 (LE)",  utf32le},
  {sizeof (utf7_1),    "UTF-7",        utf7_1},
  {sizeof (utf7_2),    "UTF-7",        utf7_2},
  {sizeof (utf7_3),    "UTF-7",        utf7_3},
  {sizeof (utf7_4),    "UTF-7",        utf7_4},
  {sizeof (utf1),      "UTF-1",        utf1},
  {sizeof (utfebcdic), "UTF-EBCDIC",   utfebcdic},
  {sizeof (scsu),      "SCSU",         scsu},
  {sizeof (bocu1),     "BOCU-1",       bocu1},
  {sizeof (gb18030),   "GB18030",      gb18030}
};

const size_t nbom = sizeof (bom) / sizeof (bom[0]);

// Detect a Byte Order Mark (BOM), if one exists at the beginning of the file.
// If more than one signature is a prefix of the input, return the longest
// matching signature. This prevents UTF-32 LE (ff fe 00 00), for example,
// from being mistaken for UTF-16 LE (ff fe).
// Return the index of the matching bom array entry, or -1 if none matches.
int
byteordermark (const uint8_t *text, size_t nbytes, const BOM *bom, size_t nbom) {

  size_t type, best_len;
  int best;

  if ((text == NULL) || (bom == NULL)) {
    return (-1);
  }

  best = -1;
  best_len = 0u;

  for (type=0u; type<nbom; type++) {

    // The file must contain the complete signature.
    if (bom[type].len > nbytes) {
      continue;
    }

    if ((bom[type].len > best_len) &&
        (memcmp (text, bom[type].sequence, bom[type].len) == 0)) {
      best = (int) type;
      best_len = bom[type].len;
    }
  }

  return (best);
}
