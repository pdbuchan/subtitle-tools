# time-text

Creates a SubRip file using timestamps from one SRT file and subtitle text from another. The two files should contain the same number of subtitles. A Byte Order Mark in the text input is preserved.

## Build

```sh
gcc -Wall time-text.c -o time-text
```

## Usage

```sh
./time-text timeinputfile.srt textinputfile.srt
```

Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
