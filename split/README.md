# split

Splits every SubRip subtitle into two identical subtitles with consecutive timestamps. It was created to generate test files for [`combine`](../combine/). An input Byte Order Mark is preserved.

## Build

```sh
gcc -Wall split.c -o split
```

## Usage

```sh
./split inputfilename.srt
```

Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
