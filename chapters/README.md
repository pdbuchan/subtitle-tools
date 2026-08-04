# chapters

Creates an XML chapters file from a feature duration and a desired number of chapters. The resulting `chapters.xml` can be added to a media container with software such as [MKVToolNix](https://mkvtoolnix.org/).

The duration uses the same notation printed by FFmpeg, including a fractional part of a second, rather than the comma-separated millisecond notation used by SubRip files. A duration can be obtained with:

```sh
ffmpeg -i filename.mkv
```

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
