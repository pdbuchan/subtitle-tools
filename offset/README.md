# offset

Applies a positive, negative, or zero offset to every timestamp in a SubRip file. Subtitle durations are preserved, existing subtitle numbers are ignored and entries are renumbered from 1 through N. If present, the Byte Order Mark (BOM) of the input file will be included in the output file.

## Build

```sh
gcc -Wall offset.c -o offset
```

## Usage

```sh
./offset inputfilename.srt
```

The program prompts for hour, minute, second, and millisecond offsets and writes `out.srt`. Enter zero for every offset to renumber a file without changing its timestamps.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
