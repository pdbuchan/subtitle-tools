# dvb

Analyzes an isolated DVB subtitle stream stored in an MPEG-2 transport stream (`.ts`) and writes a report. It can optionally produce one bitmap per subtitle. Unlike the [VobSub](../sub/) and [PGS](../pgs/) tools, this program does not have the option to offset or synchronize timestamps because PTS and DTS timestamps are carried in PES packets which are, in turn, carried by transport-stream packets. i.e., there isn't visibility between layers.

The input transport stream must contain only one DVB subtitle stream, without audio or video. Use FFmpeg to list all streams first:

```sh
ffmpeg -i feature.ts
```

A typical result might look like:

```sh
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

In this case there are two Dutch DVB subtitle streams: #`0:5`, and an SDH Stream #`0:6`. Note that Stream #`0:4` is DVB TeleText and not subtitles. You can then extract only the desired DVB subtitle stream. In this case let's use Stream #`0:6`.

```sh
ffmpeg -i feature.ts -map 0:6 -c:s copy filename.ts
```

## Build

```sh
make
```

## Usage

```sh
./dvb filename.ts [bmp]
```

The report filename is `dvb.out`. With the `bmp` option, bitmap files are also produced.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
