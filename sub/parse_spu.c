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

// Parse Subpicture Unit (SPU) Buffer.
// SPU data may be from one or more PES packets.
int
parse_spu (OPTIONS *options, uint8_t *spu_buffer, IDX *idx, PES *pes_info, SPU_PARMS *spu_info, SUB *sub_info, FILE *fo) {

  size_t pos, cmd_count, parm_len, block, n_dcsq;
  uint8_t cmd;
  uint16_t spu_sz, sp_dcsqt_sa, sp_nxt_dcsq_sa, sp_dcsq_stm, sp_dcsq[MAX_DCSQ];
  int64_t delay_ms;

  // Subpicture Unit (SPU) starts here.
  pos = 0;
  fprintf (fo, "  SUBPICTURE UNIT (SPU) (all offsets given here are within SPU)\n\n");

  // SPU_SZ - Total size of subpicture data (2 bytes) (may be composed from multiple packets)
  spu_sz = (spu_buffer[pos] << 8) | spu_buffer[pos + 1];
  fprintf (fo, "  SPU_SZ\tTotal size of subpicture data: %u bytes\n", spu_sz);
  pos += 2;

  // SP_DCSQT_SA - Start address of Subpicture Display Control Sequence Table (SP_DCSQT) (2 bytes)
  sp_dcsqt_sa = (spu_buffer[pos] << 8) | spu_buffer[pos+1];
  fprintf (fo, "  SP_DCSQT_SA\tStart Address of Subpicture Display Control Sequence Table (SP_DCSQT): 0x%04x\n", sp_dcsqt_sa);
  pos = sp_dcsqt_sa;  // Move to table SP_DCSQT.

  // SP_DCSQT - Table of (offsets to) command blocks (SP_DCSQ).
  //            Each SP_DCSQ starts with the two words: SP_DCSQ_STM and SP_NXT_DCSQ_SA.
  //            The last SP_DCSQ in a table points to itself.
  //            Each SP_DCSQ block contains commands. A SP_DCSQ block ends with 0xff.

  // Clear current array of SP_DCSQ offsets.
  memset (sp_dcsq, 0, MAX_DCSQ * sizeof (uint16_t));

  // Read in table of SP_DCSQ offsets into an array.
  fprintf (fo, "\n  SP_DCSQT - Table of SP_DCSQ offsets:\n");
  block = 0;  // Index specifying specific SP_DCSQ within table SP_DCSQT
  sp_dcsq[block] = sp_dcsqt_sa;  // The first SP_DCSQ is located at SP_DCSQT_SA.

  do {

    // Show offset/address of current SP_DCSQ.
    fprintf (fo, "    0x%04x\n", sp_dcsq[block]);

    // Move to current SP_DCSQ block.
    pos = sp_dcsq[block];

    // Bounds check
    if ((pos + 3) >= spu_sz) {
      fprintf (stderr, "Command stream exceeds SPU size in parse_spu().\n");
      exit (EXIT_FAILURE);
    }

    // Skip SP_DCSQ_STM (execution delay).
    pos += 2;

    // Extract offset of next SP_DCSQ in table SP_DCSQT.
    sp_nxt_dcsq_sa = (spu_buffer[pos] << 8) | spu_buffer[pos + 1];
 
    block++;

    if (block == (size_t) MAX_DCSQ) {
      fprintf (stderr, "block (%zu) has exceeded MAX_DCSQ (%d) in parse_spu().\n", block, MAX_DCSQ);
      exit (EXIT_FAILURE);
    }

    // Save offset of next SP_DCSQ block in array.
    sp_dcsq[block] = sp_nxt_dcsq_sa;

  } while ((sp_dcsq[block] != sp_dcsq[block-1]) &&
           (sp_dcsq[block] >= sp_dcsqt_sa) &&
           (sp_dcsq[block] < spu_sz));
  n_dcsq = block;  // Save count of SP_DCSQ blocks.

  // Clear SPU info struct for this subpicture unit.
  memset (spu_info, 0, sizeof (SPU_PARMS));

  // Loop through all SP-DCSQ blocks in current Subpicture Unit (SPU).
  for (block = 0; block < n_dcsq; block++) {

    // Set pointer to address of next SP_DCSQ block.
    pos = sp_dcsq[block];

    // Bounds check
    if ((pos + 3) >= spu_sz) {
      fprintf (stderr, "SP_DCSQ header exceeds SPU size in parse_spu().\n");
      exit (EXIT_FAILURE);
    }

    fprintf (fo, "\n  SP_DCSQ %zu at offset 0x%04x\n", block, sp_dcsq[block]);

    // SP_DCSQ_STM - Delay before executing commands (2 bytes)
    // When an SP_DCSQ block turns off the subtitle using STP_DSP command, this delay is the subtitle duration.
    // Note that the value is stored in compressed resolution (i.e., 90 kHz ticks / 1024) in order to ensure it always fits in 2 bytes.
    sp_dcsq_stm = (spu_buffer[pos] << 8) | spu_buffer[pos+1];
    delay_ms = (int64_t) sp_dcsq_stm * 1024 * 1000 / 90000;
    fprintf (fo, "    SP_DCSQ_STM\t\tDelay before executing commands: %" PRIi64 " ms\n", delay_ms);
    pos += 2;

    // SP_NXT_DCSQ_SA - Offset within SPU to next SP_DCSQ (2 bytes)
    sp_nxt_dcsq_sa = (spu_buffer[pos] << 8) | spu_buffer[pos+1];
    fprintf (fo, "    SP_NXT_DCSQ_STM\tOffset to next SP_DCSQ: 0x%04x\n", sp_nxt_dcsq_sa);
    pos += 2;

    // Process the commands for this SP_DCSQ.
    fprintf (fo, "\n  COMMANDS:\n");
    cmd_count = 0;
    while ((cmd_count < (size_t) MAX_CMD) && (pos < (size_t) spu_sz)) {

      // Extract command byte.
      cmd = spu_buffer[pos];
      pos++;  // Move past command byte.
      cmd_count++;  // Keep track of number of commands executed within this SP_DCSQ.

      switch (cmd) {

        case 0x00:  // FSTA_DSP - Forced Start Display (no arguments)
          fprintf (fo, "    0x00 FSTA_DSP - Forced Start Display\n");
          break;

        case 0x01:  // STA_DSP - Start Display (no arguments)
          fprintf (fo, "    0x01 STA_DSP - Start Display\n");
          break;

        case 0x02:  // STP_DSP - Stop Display (no arguments)
          fprintf (fo, "    0x02 STP_DSP - Stop Display\n");
          // Take the delay time as the subtitle duration; add it to PTS to get subtitle end time.
          delay_ms = (int64_t) sp_dcsq_stm * 1024 * 1000 / 90000;
          sub_info->end.totalms = pes_info->pts.totalms + delay_ms;
          mstotime (&sub_info->end);
          break;

        case 0x03:  // SET_COLOR (1 nibble per pixel type); Byte order: [E2|E1][P|B] (high nibble first)
          spu_info->clut[0] = spu_buffer[pos + 1] & 15;  // Index of idx palette for Background
          spu_info->clut[1] = spu_buffer[pos + 1] >> 4;  // Index of idx palette for Pattern
          spu_info->clut[2] = spu_buffer[pos] & 15;  // Index of idx palette for Emphasis 1
          spu_info->clut[3] = spu_buffer[pos] >> 4;  // Index of idx palette for Emphasis 2
          fprintf (fo, "    0x03 SET_COLOR (palette index): Background: 0x%01x, Pattern: 0x%01x, Emphasis 1: 0x%01x, Emphasis 2: 0x%01x\n",
                  spu_info->clut[0], spu_info->clut[1], spu_info->clut[2], spu_info->clut[3]);
          pos += 2;
          break;

        case 0x04:  // SET_CONTR - Alpha (contrast) (1 nibble per pixel type; 0x0 = transparent, 0xf = opaque); Byte order: [E2|E1][P|B] (high nibble first)
                    //             We'll multiply by 17 in unpack_pxd() to convert from 4-bit (0 to 15) to 8-bit (0 to 255)
          spu_info->alpha[0] = (spu_buffer[pos + 1] & 15);  // Alpha (contrast) value for Background
          spu_info->alpha[1] = (spu_buffer[pos + 1] >> 4);  // Alpha (contrast) value for Pattern
          spu_info->alpha[2] = (spu_buffer[pos] & 15);  // Alpha (contrast) value for Emphasis 1
          spu_info->alpha[3] = (spu_buffer[pos] >> 4);  // Alpha (contrast) value for Emphasis 2
          fprintf (fo, "    0x04 SET_CONTR (Alpha: 0 = transparent, 0xf = opaque): Background: 0x%01x, Pattern: 0x%01x, Emphasis 1: 0x%01x, Emphasis 2: 0x%01x\n",
                  spu_info->alpha[0], spu_info->alpha[1], spu_info->alpha[2], spu_info->alpha[3]);
          pos += 2;
          break;

        case 0x05:  // SET_DAREA - Start and end horizontal and vertical pixel positions (inclusive; e.g., width = x_end - x_start + 1)
          spu_info->x_start = (size_t) ((spu_buffer[pos] << 4) | (spu_buffer[pos + 1] >> 4));
          spu_info->x_end   = (size_t) (((spu_buffer[pos + 1] & 0x0f) << 8) | spu_buffer[pos + 2]);
          pos += 3;
          fprintf (fo, "    0x05 SET_DAREA:\n");
          fprintf (fo, "      info->x_start = %zu px\n", spu_info->x_start);
          fprintf (fo, "      info->x_end = %zu px\n", spu_info->x_end);

          spu_info->y_start = (size_t) ((spu_buffer[pos] << 4) | (spu_buffer[pos + 1] >> 4));
          spu_info->y_end   = (size_t) (((spu_buffer[pos + 1] & 0x0f) << 8) | spu_buffer[pos + 2]);
          pos += 3;
          fprintf (fo, "      info->y_start = %zu px\n", spu_info->y_start);
          fprintf (fo, "      info->y_end = %zu px\n", spu_info->y_end);
          break;

        case 0x06:  // SET_DSPXA - Define pixel data addresses (offsets within SPU to find pixel data).
          spu_info->pxd_tf =  (spu_buffer[pos] << 8) | spu_buffer[pos+1];  // Top Field Pixel Data
          pos += 2;
          fprintf (fo, "    0x06 SET_DSPXA:\n");
          fprintf (fo, "      Top Field Pixel Data Address Offset pxdtf = 0x%04x\n", spu_info->pxd_tf);
          spu_info->pxd_bf =  (spu_buffer[pos] << 8) | spu_buffer[pos + 1];  // Bottom Field Pixel Data
          pos += 2;
          fprintf (fo, "      Bottom Field Pixel Data Address Offset pxdbf = 0x%04x\n", spu_info->pxd_bf);
          break;

        case 0x07:  // CHG_COLCON - Change color and contrast (not implemented).
          parm_len = (size_t) ((spu_buffer[pos] << 8) | spu_buffer[pos + 1]);  // Total size of parameter area
          pos += 2;
          fprintf (fo, "    0x07 CHG_COLCON (not implemented)\n");

          // Skip all the parameter data.
          if (pos + parm_len > spu_sz) {
            fprintf (stderr, "CHG_COLCON exceeds SPU size\n");
            exit (EXIT_FAILURE);
          }
          pos += parm_len;
          break;

        case 0xff:  // CMD_END - Ends current SP_DCSQ.
          fprintf (fo, "    0xff CMD_END\n");
          break;

        default:  // Unknown or unsupported command; interpret as 0xff
          fprintf (fo, "    0x%02x Unknown or unsupported command; applying 0xff CMD_END\n", cmd);
          cmd = 0xff;
          break;

      }  // End switch (cmd)

      // End of control sequence
      if (cmd == 0xff) break;

    }  // End of processing of commands in this SP-DCSQ

  }  // Next SP_DCSQ

  return (EXIT_SUCCESS);
}
