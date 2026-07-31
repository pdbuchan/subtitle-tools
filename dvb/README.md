# dvb

Analyzes an isolated DVB subtitle stream stored in an MPEG-2 transport stream (`.ts`) and writes a report. It can optionally produce one bitmap per subtitle.

The input transport stream must contain only one DVB subtitle stream, without audio or video. Use FFmpeg to list streams and extract the desired subtitle stream first:

```sh
ffmpeg -i feature.ts
ffmpeg -i feature.ts -map 0:6 -c:s copy filename.ts
```

Replace `0:6` with the stream index reported for the desired DVB subtitle stream.

## Build

```sh
make
```

## Usage

```sh
./dvb filename.ts [bmp]
```

The normal report is `dvb.out`. With the `bmp` option, bitmap files are also produced.

Unlike the VobSub and PGS tools, this program does not offset or synchronize timestamps because PTS and DTS values are nested inside PES packets carried by transport-stream packets.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
