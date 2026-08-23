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

/*
 * A DVD/VobSub Subpicture Unit (SPU) consists of pixel data followed by a
 * linked list of Display Control Sequences (SP_DCSQ).  Each SP_DCSQ contains
 * a time relative to the PES PTS, a pointer to the next SP_DCSQ, and one or
 * more commands terminated by CMD_END (0xff).
 *
 * Classic DVD SPUs use 16-bit SPU sizes/offsets and 2-bit RLE pixels.  The
 * extended HD form supported here uses a zero 16-bit prefix followed by
 * 32-bit SPU sizes/offsets and adds commands for a 256-entry palette, alpha
 * table, display area, and 32-bit pixel-data addresses.
 *
 * parse_spu() walks the complete SP_DCSQ chain and reports every state
 * change.  Because the BMP extraction mode produces one still image per
 * subtitle, the SPU parameters in effect at the first display-start command
 * are copied to spu_info and later display-control changes are only reported.
 */

// Read fixed-width big-endian integer fields used throughout the SPU format.
static uint16_t
read_be16 (const uint8_t *p) {
  return (uint16_t) (((uint16_t) p[0] << 8) | p[1]);
}

static uint32_t
read_be32 (const uint8_t *p) {
  return ((uint32_t) p[0] << 24) |
         ((uint32_t) p[1] << 16) |
         ((uint32_t) p[2] << 8) |
          (uint32_t) p[3];
}

// SP_NXT_DCSQ_SA and related address fields are 16 bits in classic SPUs and
// 32 bits in the extended HD form.
static size_t
read_offset (const uint8_t *p, uint8_t big_offsets) {
  return big_offsets ? (size_t) read_be32 (p) : (size_t) read_be16 (p);
}

// Saturate a converted RGB component to the range representable by uint8_t.
static uint8_t
clamp_u8 (int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (uint8_t) value;
}

// Extended command 0x83 stores each HD palette entry as Y, Cr, Cb.  Convert
// studio-range digital component values to 8-bit RGB using BT.601 coefficients.
static RGB
ycrcb_to_rgb (uint8_t y, uint8_t cr, uint8_t cb) {

  int c, d, e, r, g, b;

  c = (int) y - 16;
  if (c < 0) c = 0;
  d = (int) cb - 128;
  e = (int) cr - 128;

  r = (298 * c + 409 * e + 128) >> 8;
  g = (298 * c - 100 * d - 208 * e + 128) >> 8;
  b = (298 * c + 516 * d + 128) >> 8;

  return (RGB){clamp_u8 (r), clamp_u8 (g), clamp_u8 (b)};
}

// CHG_COLCON can describe an arbitrary number of horizontal color/contrast
// regions.  Grow the retained list geometrically rather than imposing a fixed
// per-subtitle limit.
static void
ensure_colcon_capacity (SPU_PARMS *spu, size_t additional) {

  size_t needed, capacity;
  COLCON_ENTRY *tmp;

  if (additional > SIZE_MAX - spu->n_colcon) {
    fprintf (stderr, "CHG_COLCON entry count overflow.\n");
    exit (EXIT_FAILURE);
  }

  needed = spu->n_colcon + additional;
  if (needed <= spu->colcon_capacity) return;

  capacity = spu->colcon_capacity == 0 ? 16 : spu->colcon_capacity;
  while (capacity < needed) {
    if (capacity > SIZE_MAX / 2) {
      capacity = needed;
      break;
    }
    capacity *= 2;
  }

  if (capacity > SIZE_MAX / sizeof (COLCON_ENTRY)) {
    fprintf (stderr, "CHG_COLCON entry array is too large.\n");
    exit (EXIT_FAILURE);
  }

  tmp = realloc (spu->colcon, capacity * sizeof (COLCON_ENTRY));
  if (tmp == NULL) {
    fprintf (stderr, "Cannot allocate CHG_COLCON entries.\n");
    exit (EXIT_FAILURE);
  }

  spu->colcon = tmp;
  spu->colcon_capacity = capacity;
}

// Append one decoded PX_CTLI region to the current SPU state.
static void
append_colcon (SPU_PARMS *spu, const COLCON_ENTRY *entry) {
  ensure_colcon_capacity (spu, 1);
  spu->colcon[spu->n_colcon++] = *entry;
}

// Release only the dynamically allocated portion of SPU_PARMS.  The caller
// owns the structure itself.
void
free_spu_parms (SPU_PARMS *spu) {
  if (spu == NULL) return;
  free (spu->colcon);
  spu->colcon = NULL;
  spu->n_colcon = 0;
  spu->colcon_capacity = 0;
}

// SPU_PARMS contains a pointer, so a plain structure assignment would leave
// dst and src sharing the same CHG_COLCON array.  Make a true deep copy when
// preserving the display state selected for bitmap extraction.
static void
clone_spu_parms (SPU_PARMS *dst, const SPU_PARMS *src) {

  COLCON_ENTRY *entries;

  free_spu_parms (dst);
  *dst = *src;
  dst->colcon = NULL;
  dst->colcon_capacity = 0;

  if (src->n_colcon == 0) return;

  if (src->n_colcon > SIZE_MAX / sizeof (COLCON_ENTRY)) {
    fprintf (stderr, "CHG_COLCON clone size overflow.\n");
    exit (EXIT_FAILURE);
  }

  entries = malloc (src->n_colcon * sizeof (COLCON_ENTRY));
  if (entries == NULL) {
    fprintf (stderr, "Cannot clone CHG_COLCON data.\n");
    exit (EXIT_FAILURE);
  }

  memcpy (entries, src->colcon, src->n_colcon * sizeof (COLCON_ENTRY));
  dst->colcon = entries;
  dst->colcon_capacity = src->n_colcon;
}

// SET_COLOR and PX_CTLI pack four 4-bit CLUT indexes into two bytes.  The
// logical pixel-code order used by the decoder is background, pattern,
// emphasis-1, emphasis-2 (B/P/E1/E2).
static void
decode_classic_map (const uint8_t *p, uint8_t map[4]) {
  map[0] = p[1] & 0x0f;
  map[1] = p[1] >> 4;
  map[2] = p[0] & 0x0f;
  map[3] = p[0] >> 4;
}

// SET_CONTR uses the same nibble ordering.  Expand each 4-bit contrast value
// to an 8-bit alpha value by multiplying by 17 (0x0 -> 0x00, 0xf -> 0xff).
static void
decode_classic_alpha (const uint8_t *p, uint8_t alpha[4]) {
  alpha[0] = (uint8_t) ((p[1] & 0x0f) * 17u);
  alpha[1] = (uint8_t) ((p[1] >> 4) * 17u);
  alpha[2] = (uint8_t) ((p[0] & 0x0f) * 17u);
  alpha[3] = (uint8_t) ((p[0] >> 4) * 17u);
}

/*
 * Parse command 0x07 (CHG_COLCON).
 *
 * The command begins with a 16-bit parameter-area length.  Within that area,
 * each LN_CTLI word defines a vertical line range and the number of following
 * PX_CTLI entries.  A PX_CTLI entry supplies the x coordinate at which a new
 * classic four-color mapping and alpha mapping take effect.  The list ends
 * with the 32-bit value 0x0fffffff.
 *
 * The decoded regions are retained in current->colcon; unpack_pxd() later
 * selects the rightmost applicable PX_CTLI entry for each rendered pixel.
 */
static int
parse_chg_colcon (const uint8_t *spu_buffer, size_t *pos, size_t command_limit,
                  SPU_PARMS *current, FILE *fo) {

  size_t param_start, param_end, npx, i, start_y, end_y, start_x, prev_x;
  uint16_t param_len;
  uint32_t line_control;
  uint8_t map[4], alpha[4], saw_sentinel;
  COLCON_ENTRY entry;

  // param_len includes its own two-byte length field.
  param_start = *pos;
  if (command_limit - *pos < 2) {
    fprintf (stderr, "Truncated CHG_COLCON size field.\n");
    return (EXIT_FAILURE);
  }

  param_len = read_be16 (spu_buffer + *pos);
  if (param_len < 2 || (size_t) param_len > command_limit - param_start) {
    fprintf (stderr, "Invalid CHG_COLCON parameter length %u.\n", param_len);
    return (EXIT_FAILURE);
  }

  param_end = param_start + param_len;
  *pos += 2;
  saw_sentinel = 0;
  fprintf (fo, "      0x07 CHG_COLCON - Change Color/Contrast (%u-byte parameter area)\n", param_len);

  while (*pos < param_end) {
    if (param_end - *pos < 4) {
      fprintf (stderr, "Truncated LN_CTLI in CHG_COLCON.\n");
      return (EXIT_FAILURE);
    }

    // LN_CTLI bit layout: reserved[31..28], start_y[27..16],
    // npx[15..12], end_y[11..0].  The reserved nibble must be zero.
    line_control = read_be32 (spu_buffer + *pos);
    *pos += 4;

    // 0x0fffffff terminates the entire CHG_COLCON parameter list.
    if (line_control == UINT32_C(0x0fffffff)) {
      saw_sentinel = 1;
      break;
    }

    if ((line_control >> 28) != 0) {
      fprintf (stderr, "Non-zero reserved nibble in CHG_COLCON LN_CTLI.\n");
      return (EXIT_FAILURE);
    }

    start_y = (line_control >> 16) & 0x0fff;
    npx = (line_control >> 12) & 0x0f;
    end_y = line_control & 0x0fff;
    if (end_y < start_y) {
      fprintf (stderr, "CHG_COLCON line range is reversed.\n");
      return (EXIT_FAILURE);
    }

    fprintf (fo, "        LN_CTLI: lines %zu-%zu, %zu PX_CTLI entries\n", start_y, end_y, npx);
    prev_x = 0;

    for (i = 0; i < npx; i++) {
      if (param_end - *pos < 6) {
        fprintf (stderr, "Truncated PX_CTLI in CHG_COLCON.\n");
        return (EXIT_FAILURE);
      }

      // PX_CTLI is six bytes: x_start, two bytes of palette mapping, and
      // two bytes of contrast/alpha mapping.  Entries must progress left to
      // right so the active region can be selected unambiguously.
      start_x = read_be16 (spu_buffer + *pos);
      if (i > 0 && start_x <= prev_x) {
        fprintf (stderr, "CHG_COLCON PX_CTLI columns are not strictly increasing.\n");
        return (EXIT_FAILURE);
      }
      prev_x = start_x;

      decode_classic_map (spu_buffer + *pos + 2, map);
      decode_classic_alpha (spu_buffer + *pos + 4, alpha);

      memset (&entry, 0, sizeof (entry));
      entry.x_start = start_x;
      entry.y_start = start_y;
      entry.y_end = end_y;
      memcpy (entry.clut, map, sizeof (entry.clut));
      memcpy (entry.alpha, alpha, sizeof (entry.alpha));
      append_colcon (current, &entry);

      fprintf (fo, "          PX_CTLI: x >= %zu, palette %x/%x/%x/%x, alpha %u/%u/%u/%u\n",
               start_x, map[0], map[1], map[2], map[3],
               alpha[0], alpha[1], alpha[2], alpha[3]);
      *pos += 6;
    }
  }

  if (!saw_sentinel) {
    fprintf (stderr, "CHG_COLCON is missing its 0x0fffffff terminator.\n");
    return (EXIT_FAILURE);
  }

  *pos = param_end;
  return (EXIT_SUCCESS);
}

/*
 * Parse one completely assembled Subpicture Unit.
 *
 * sub_info->start initially contains the PES presentation time established by
 * parse_packets().  SP_DCSQ_STM values are relative to that base time.
 * spu_info receives the parameter state used to decode the single extracted
 * bitmap; all DCSQs are nevertheless parsed so the text report describes the
 * complete SPU command sequence and the stop time can be found.
 */
int
parse_spu (const uint8_t *spu_buffer, size_t spu_buffer_size, IDX *idx,
           PES *pes_info, SPU_PARMS *spu_info, SUB *sub_info, FILE *fo) {

  size_t declared_size, cmd_pos, next_cmd_pos, pos, command_limit, block;
  size_t offset_size, i;
  uint16_t date;
  uint8_t big_offsets, cmd, ended, dcsq_has_start, captured;
  int64_t delay_ms, base_pts_ms;
  SPU_PARMS current;

  (void) idx;
  (void) pes_info;

  if (spu_buffer == NULL || spu_buffer_size < 4) {
    fprintf (stderr, "SPU is too short.\n");
    return (EXIT_FAILURE);
  }

  // Save the PES PTS before any display-sequence delay is applied.  Every
  // SP_DCSQ_STM is measured relative to this same timestamp.
  base_pts_ms = sub_info->start.totalms;

  // current accumulates command effects as the DCSQ chain is traversed.
  // spu_info is reset here and later receives a deep copy of the chosen state.
  memset (&current, 0, sizeof (current));
  free_spu_parms (spu_info);
  memset (spu_info, 0, sizeof (*spu_info));

  /*
   * Classic header:
   *   0..1  SPU_SZ       (16-bit)
   *   2..3  SP_DCSQT_SA  (16-bit)
   *
   * Extended HD header:
   *   0..1  zero marker
   *   2..5  SPU_SZ       (32-bit)
   *   6..9  SP_DCSQT_SA  (32-bit)
   */
  big_offsets = read_be16 (spu_buffer) == 0;
  if (big_offsets) {
    if (spu_buffer_size < 10) {
      fprintf (stderr, "Extended HD SPU header is truncated.\n");
      return (EXIT_FAILURE);
    }
    offset_size = 4;
    declared_size = read_be32 (spu_buffer + 2);
    cmd_pos = read_be32 (spu_buffer + 6);
  } else {
    offset_size = 2;
    declared_size = read_be16 (spu_buffer);
    cmd_pos = read_be16 (spu_buffer + 2);
  }

  if (declared_size != spu_buffer_size) {
    fprintf (stderr, "SPU declared size (%zu) differs from assembled size (%zu).\n",
             declared_size, spu_buffer_size);
    return (EXIT_FAILURE);
  }

  if (cmd_pos == 0 || declared_size < 2 + offset_size ||
      cmd_pos > declared_size - (2 + offset_size)) {
    fprintf (stderr, "Invalid SP_DCSQT start offset 0x%zx.\n", cmd_pos);
    return (EXIT_FAILURE);
  }

  fprintf (fo, "  SUBPICTURE UNIT (SPU) (all offsets given here are within SPU)\n\n");
  fprintf (fo, "  SPU size: %zu bytes%s\n", declared_size,
           big_offsets ? " (extended HD 32-bit offsets)" : "");
  fprintf (fo, "  SP_DCSQT start offset: 0x%zx\n", cmd_pos);

  captured = 0;  // True after the bitmap-display state has been saved.
  block = 0;

  // Follow the linked SP_DCSQ list until an entry points to itself or zero.
  for (;;) {
    if (cmd_pos > declared_size - (2 + offset_size)) {
      fprintf (stderr, "SP_DCSQ header exceeds SPU size.\n");
      free_spu_parms (&current);
      return (EXIT_FAILURE);
    }

    // SP_DCSQ_STM is a 16-bit delay in units of 1024/90000 second.
    // SP_NXT_DCSQ_SA is an absolute offset from the beginning of this SPU.
    date = read_be16 (spu_buffer + cmd_pos);
    next_cmd_pos = read_offset (spu_buffer + cmd_pos + 2, big_offsets);
    if (next_cmd_pos > declared_size) {
      fprintf (stderr, "SP_NXT_DCSQ_SA points outside the SPU.\n");
      free_spu_parms (&current);
      return (EXIT_FAILURE);
    }
    if (next_cmd_pos < cmd_pos) {
      fprintf (stderr, "SP_NXT_DCSQ_SA moves backwards.\n");
      free_spu_parms (&current);
      return (EXIT_FAILURE);
    }

    // Convert the control-sequence delay to milliseconds for TIME/reporting.
    delay_ms = (int64_t) date * 1024 * 1000 / 90000;
    fprintf (fo, "\n  SP_DCSQ %zu at offset 0x%zx\n", block, cmd_pos);
    fprintf (fo, "    SP_DCSQ_STM: %" PRId64 " ms\n", delay_ms);
    fprintf (fo, "    SP_NXT_DCSQ_SA: 0x%zx\n", next_cmd_pos);

    // Commands immediately follow the DCSQ time and next-address fields.
    // A forward link also gives a natural hard boundary for this command list;
    // the final/self-linked DCSQ may extend to the declared end of the SPU.
    pos = cmd_pos + 2 + offset_size;
    command_limit = (next_cmd_pos > cmd_pos) ? next_cmd_pos : declared_size;
    ended = 0;
    dcsq_has_start = 0;

    fprintf (fo, "    COMMANDS:\n");
    while (pos < command_limit) {
      cmd = spu_buffer[pos++];

      switch (cmd) {
        // Forced Start Display.  Besides starting display, remember that this
        // subtitle is flagged for forced presentation by the DVD player.
        case 0x00:
          current.forced = 1;
          dcsq_has_start = 1;
          fprintf (fo, "      0x00 FSTA_DSP - Forced Start Display\n");
          if (!captured) {
            if (base_pts_ms > INT64_MAX - delay_ms) {
              fprintf (stderr, "Subtitle start timestamp overflow.\n");
              free_spu_parms (&current);
              return (EXIT_FAILURE);
            }
            sub_info->start.totalms = base_pts_ms + delay_ms;
            if (mstotime (&sub_info->start) != EXIT_SUCCESS) {
              free_spu_parms (&current);
              return (EXIT_FAILURE);
            }
          }
          break;

        // Normal Start Display.  Only the first start encountered selects the
        // state that will be used for the one extracted bitmap.
        case 0x01:
          dcsq_has_start = 1;
          fprintf (fo, "      0x01 STA_DSP - Start Display\n");
          if (!captured) {
            if (base_pts_ms > INT64_MAX - delay_ms) {
              fprintf (stderr, "Subtitle start timestamp overflow.\n");
              free_spu_parms (&current);
              return (EXIT_FAILURE);
            }
            sub_info->start.totalms = base_pts_ms + delay_ms;
            if (mstotime (&sub_info->start) != EXIT_SUCCESS) {
              free_spu_parms (&current);
              return (EXIT_FAILURE);
            }
          }
          break;

        // Stop Display determines the subtitle's ending timestamp.
        case 0x02:
          fprintf (fo, "      0x02 STP_DSP - Stop Display\n");
          if (base_pts_ms > INT64_MAX - delay_ms) {
            fprintf (stderr, "Subtitle end timestamp overflow.\n");
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          sub_info->end.totalms = base_pts_ms + delay_ms;
          if (mstotime (&sub_info->end) != EXIT_SUCCESS) {
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          break;

        // Classic SET_COLOR: map the four 2-bit RLE pixel codes to entries in
        // the 16-color palette supplied by the .idx file.
        case 0x03:
          if (command_limit - pos < 2) {
            fprintf (stderr, "Truncated SET_COLOR command.\n");
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          decode_classic_map (spu_buffer + pos, current.clut);
          fprintf (fo, "      0x03 SET_COLOR: B=%x P=%x E1=%x E2=%x\n",
                   current.clut[0], current.clut[1], current.clut[2], current.clut[3]);
          pos += 2;
          break;

        // Classic SET_CONTR: four 4-bit contrast values corresponding to the
        // same B/P/E1/E2 pixel codes used by SET_COLOR.
        case 0x04: {
          uint8_t alpha4[4];
          if (command_limit - pos < 2) {
            fprintf (stderr, "Truncated SET_CONTR command.\n");
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          decode_classic_alpha (spu_buffer + pos, alpha4);
          memcpy (current.alpha, alpha4, sizeof (alpha4));
          fprintf (fo, "      0x04 SET_CONTR alpha: B=%u P=%u E1=%u E2=%u\n",
                   alpha4[0], alpha4[1], alpha4[2], alpha4[3]);
          pos += 2;
          break;
        }

        // SET_DAREA stores inclusive 12-bit start/end coordinates packed into
        // six bytes.  Extended command 0x85 has the same coordinate layout and
        // additionally identifies the extended 8-bit-pixel form.
        case 0x05:
        case 0x85:
          if (command_limit - pos < 6) {
            fprintf (stderr, "Truncated SET_DAREA/HD SET_DAREA command.\n");
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          current.x_start = ((size_t) spu_buffer[pos] << 4) | (spu_buffer[pos + 1] >> 4);
          current.x_end = ((size_t) (spu_buffer[pos + 1] & 0x0f) << 8) | spu_buffer[pos + 2];
          current.y_start = ((size_t) spu_buffer[pos + 3] << 4) | (spu_buffer[pos + 4] >> 4);
          current.y_end = ((size_t) (spu_buffer[pos + 4] & 0x0f) << 8) | spu_buffer[pos + 5];
          if (cmd == 0x85) current.is_8bit = 1;
          fprintf (fo, "      0x%02x %sSET_DAREA: x=%zu..%zu y=%zu..%zu\n", cmd,
                   cmd == 0x85 ? "HD " : "", current.x_start, current.x_end,
                   current.y_start, current.y_end);
          pos += 6;
          break;

        // Classic SET_DSPXA: absolute byte offsets to the RLE data for the top
        // and bottom interlaced fields.
        case 0x06:
          if (command_limit - pos < 4) {
            fprintf (stderr, "Truncated SET_DSPXA command.\n");
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          current.pxd_tf = read_be16 (spu_buffer + pos);
          current.pxd_bf = read_be16 (spu_buffer + pos + 2);
          fprintf (fo, "      0x06 SET_DSPXA: top=0x%zx bottom=0x%zx\n",
                   current.pxd_tf, current.pxd_bf);
          pos += 4;
          break;

        // Per-region classic color/contrast changes.
        case 0x07:
          if (parse_chg_colcon (spu_buffer, &pos, command_limit, &current, fo) != EXIT_SUCCESS) {
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          break;

        // Extended HD palette: 256 consecutive Y/Cr/Cb triplets.  Unlike
        // classic SPUs, these colors are carried inside the SPU rather than
        // referenced through the global 16-entry .idx palette.
        case 0x83:
          if (command_limit - pos < 3u * MAX_HD_PALETTE) {
            fprintf (stderr, "Truncated HD palette command.\n");
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          for (i = 0; i < MAX_HD_PALETTE; i++) {
            current.hd_palette[i] = ycrcb_to_rgb (spu_buffer[pos + 3 * i],
                                                  spu_buffer[pos + 3 * i + 1],
                                                  spu_buffer[pos + 3 * i + 2]);
          }
          current.has_hd_palette = 1;
          current.is_8bit = 1;
          fprintf (fo, "      0x83 HD SET_PALETTE: 256 Y/Cr/Cb entries\n");
          pos += 3u * MAX_HD_PALETTE;
          break;

        // Extended HD alpha table: one value per 8-bit pixel code.  The
        // encoded value is transparency, so invert it to the RGBA convention
        // used by the bitmap writer (0 = transparent, 255 = opaque).
        case 0x84:
          if (command_limit - pos < MAX_HD_PALETTE) {
            fprintf (stderr, "Truncated HD alpha command.\n");
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          for (i = 0; i < MAX_HD_PALETTE; i++) {
            current.alpha[i] = (uint8_t) (0xffu - spu_buffer[pos + i]);
          }
          current.is_8bit = 1;
          fprintf (fo, "      0x84 HD SET_ALPHA: 256 entries\n");
          pos += MAX_HD_PALETTE;
          break;

        // Extended HD pixel-data addresses use two 32-bit absolute offsets
        // instead of the two 16-bit offsets used by command 0x06.
        case 0x86:
          if (command_limit - pos < 8) {
            fprintf (stderr, "Truncated HD pixel-data-address command.\n");
            free_spu_parms (&current);
            return (EXIT_FAILURE);
          }
          current.pxd_tf = read_be32 (spu_buffer + pos);
          current.pxd_bf = read_be32 (spu_buffer + pos + 4);
          current.is_8bit = 1;
          fprintf (fo, "      0x86 HD SET_DSPXA: top=0x%zx bottom=0x%zx\n",
                   current.pxd_tf, current.pxd_bf);
          pos += 8;
          break;

        // Every DCSQ command list must explicitly terminate with CMD_END.
        case 0xff:
          fprintf (fo, "      0xff CMD_END\n");
          ended = 1;
          break;

        default:
          fprintf (stderr, "Unknown SPU command 0x%02x at offset 0x%zx.\n", cmd, pos - 1);
          free_spu_parms (&current);
          return (EXIT_FAILURE);
      }

      if (ended) break;
    }

    if (!ended) {
      fprintf (stderr, "SP_DCSQ at offset 0x%zx has no CMD_END before its boundary.\n", cmd_pos);
      free_spu_parms (&current);
      return (EXIT_FAILURE);
    }

    /*
     * A VobSub SPU may contain later DCSQs that animate/fade an already
     * displayed subtitle.  Bitmap extraction intentionally emits one still
     * image, so preserve the state at the first FSTA_DSP/STA_DSP.  Continue
     * parsing subsequent DCSQs to obtain the stop time and complete report.
     */
    if (dcsq_has_start && !captured) {
      clone_spu_parms (spu_info, &current);
      captured = 1;
    }

    block++;
    if (next_cmd_pos == cmd_pos || next_cmd_pos == 0) break;
    cmd_pos = next_cmd_pos;
  }

  // Some malformed/non-display SPUs may contain no explicit start command.
  // Retain the final parsed state so the validation below still diagnoses the
  // actual missing/invalid fields rather than operating on an empty structure.
  if (!captured) clone_spu_parms (spu_info, &current);
  free_spu_parms (&current);

  // Validate the fields required by unpack_pxd() before any RLE decoding.
  if (spu_info->x_end < spu_info->x_start || spu_info->y_end < spu_info->y_start) {
    fprintf (stderr, "SPU display area is invalid or missing.\n");
    return (EXIT_FAILURE);
  }
  if (spu_info->pxd_tf >= declared_size || spu_info->pxd_bf >= declared_size) {
    fprintf (stderr, "SPU pixel-data address points outside the SPU.\n");
    return (EXIT_FAILURE);
  }
  if (spu_info->is_8bit && !spu_info->has_hd_palette) {
    fprintf (stderr, "8-bit HD SPU has no HD palette.\n");
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
