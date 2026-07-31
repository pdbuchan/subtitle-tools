# reorder

Sorts subtitles in a SubRip file by starting timestamp. An input Byte Order Mark is preserved.

## Build

```sh
gcc -Wall reorder.c -o reorder
```

## Usage

```sh
./reorder inputfilename.srt
```

Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
