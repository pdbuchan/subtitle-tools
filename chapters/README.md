# chapters

Creates an XML chapters file from a feature duration and a desired number of chapters. The resulting `chapters.xml` can be added to a media container with software such as [MKVToolNix](https://mkvtoolnix.org/).

The duration uses the same notation printed by FFmpeg, including a fractional part of a second, rather than the comma-separated millisecond notation used by SubRip files. A duration can be obtained with:

```sh
ffmpeg -i filename.mkv
```

A typical result would look like:

```sh
Input #0, matroska,webm, from 'filename.mkv':
  Metadata:
    encode          : TMPGEnc Video Mastering Works 7 Version 7.0.30.33
    creation_time   : 2024-08-04T15:17:54.341000Z
  Duration: 01:29:53.83, start: 0.000000, bitrate: 6681 kb/s
  Stream #0:0: Video: h264 (High), yuv420p(tv, bt709, progressive), 1920x1080 [SAR 1:1 DAR 16:9], 23.98 fps, 23.98 tbr, 1k tbn (default)
  Stream #0:1: Audio, aac (Main), 48000 Hz, stereo, fltp (default)

etc...
```

Here, the duration to use in `chapters` is `01:29:53.83`, which you can simply copy and paste.

## Build

```sh
gcc -Wall chapters.c -o chapters
```

## Usage

```sh
./chapters
```

The program prompts for the duration and desired chapter count, then writes `chapters.xml`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
