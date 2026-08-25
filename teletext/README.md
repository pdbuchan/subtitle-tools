# teletext

Analyzes an isolated DVB Teletext stream carried in an MPEG-2 transport stream (`.ts`) and writes a report. It can optionally produce a new directory containing text files containing snapshots of Level 1 Teletext as UTF-8 (see discussion below for more).

The input transport stream must contain only one DVB Teletext stream, without audio or video streams. Use FFmpeg to list all streams first:

```sh
ffmpeg -i feature.ts
```

A typical result might look like:

```text
Input #0, mpegts, from feature.ts:
  Duration: 00:50:24.55, start: 0.060000, bitrate: 13418 kb/s
  Program 19401
  Stream #0:0[0x7d1]: Video: h264 (High) ([27][0][0][0] / 0x001B), yuv420p(tv, bt709, top first), 1920x1080 [SAR 1:1 DAR 16:9], 25 fps, 25 tbr, 90k tbn
  Stream #0:1[0x7db](dut): Audio: ac3 ([6][0][0][0] / 0x0006), 48000 Hz, 5.1(side), fltp, 448 kb/s
  Stream #0:2[0x7dc](dut): Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 256 kb/s
  Stream #0:3[0x7dd](GOS): Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 256 kb/s
  Stream #0:4[0x7de](dut): Subtitle: dvb_teletext ([6][0][0][0] / 0x0006), 492x250
  Stream #0:5[0x7df](dut): Subtitle: dvb_subtitle ([6][0][0][0] / 0x0006)
  Stream #0:6[0x7e0](888,dut): Subtitle: dvb_subtitle ([6][0][0][0] / 0x0006) (hearing impaired)
```

In this case there is one Dutch DVB Teletext stream: #`0:4`. You can then extract only the desired DVB Teletext stream. In this case let's use Stream #`0:4`.

```sh
ffmpeg -i feature.ts -map 0:4 -c:s copy filename.ts
```

## Build

```sh
make
```

## Usage

```sh
./teletext filename.ts [text]
```

The report filename is `teletext.out`. With the `text` option, a `teletext/` directory is created and populated with text files of Level 1 Teletext text snapshots as UTF-8. Existing output files are not overwritten.

The report file `teletext.out` can be approximately 10 times larger than the MPEG-2 stream (.ts) file.

## Why the extracted text is split into multiple files

A Teletext elementary stream is not one linear document. It can carry many
page numbers, subpages and languages at the same time, and a subtitle page can
be retransmitted repeatedly with different captions while retaining the same
page number. For that reason the extractor writes **one file per PID, page and
subpage**, for example:

```text
P888_S0000_PID0234.txt
```

Each such file contains that page/subpage's transmissions in stream order.
Consecutive identical retransmissions are collapsed and labelled with their
transmission range. This keeps related material together without losing the
changing states needed for Teletext subtitles.

An `index.txt` file in `teletext_pages/` lists the extracted PID/page/subpage
files and their transmission counts. The single `teletext.out` file remains
the exhaustive structural report.

## What is decoded

- 188-byte MPEG-2 TS packets, PAT, PMT and SDT using the inherited analyzer
  infrastructure.
- DVB Teletext descriptor (`descriptor_tag` `0x56`) entries, including ISO 639
  language, Teletext type, magazine and page number.
- Only PMT elementary streams with `stream_type` `0x06` **and** a Teletext
  descriptor are routed to the Teletext PES parser.
- `private_stream_1` (`stream_id` `0xbd`) PES packets with EBU data identifiers
  `0x10` through `0x1f`.
- EBU Teletext data units `0x02` (non-subtitle) and `0x03` (subtitle), whose
  data-unit length is 44 bytes (`0x2c`).
- Teletext packet address, page/subpage address, control bits C4-C14, Hamming
  8/4 correction/detection, odd character parity, and the standard Level 1
  display rows X/0-X/24.
- Basic Level 1 Latin national-character substitutions, using an exact PMT
  page/language match when available, an unambiguous stream language otherwise,
  and C12-C14 as a final fallback.

## Plain-text limitations

Teletext is a presentation format rather than simply a character stream.
Colours, flashing, conceal/reveal, double-height/width, hold mosaics, separated
mosaics and other spacing attributes cannot be represented faithfully in a
plain text file. This extractor consumes the spacing control codes, retains
alphanumeric text, and blanks mosaic graphics.

Packet X/25 is outside the standard 25-row Level 1 display area, and packets
X/26-X/31 carry enhancement or other non-row data. The analyzer records their
logical bytes in `teletext.out`, but this generic UTF-8 extractor does not try
to assign them a display-row meaning. The report therefore preserves evidence
that such information was present even where plain text is not an adequate
representation.

For C5 newsflash and C6 subtitle pages, Start Box and End Box controls are
honoured so that characters outside the boxed television-picture overlay are
not emitted as subtitle text.

## Page-update handling

Every packet X/0 starts a new page-transmission snapshot. When C4 (`erase page`)
is clear, rows not retransmitted are inherited from the preceding state of the
same page/subpage, matching a receiver's retained page memory. Serial and
parallel magazine modes are respected when selecting the active page to which
X/1-X/24 rows belong.

This snapshot model is important for subtitle pages: retaining only the final
state of page 888, for example, would discard almost all captions in a recorded
stream.

## Standards

- ISO/IEC 13818-1 — MPEG-2 systems / Transport Stream and PES
- ETSI EN 300 468 — DVB service information, including the Teletext descriptor
- ETSI EN 300 472 — transport of ITU-R System B Teletext in DVB bitstreams
- ETSI EN 300 706 — enhanced Teletext specification

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
