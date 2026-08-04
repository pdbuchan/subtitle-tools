# combine

Combines identical subtitles when their timestamps are immediately adjacent, meaning there is no interval in which no subtitle is displayed. The merged entry uses the first subtitle's start time and the last subtitle's end time. If present, the Byte Order Mark (BOM) of the input file will be included in the output file.

## Build

```sh
gcc -Wall combine.c -o combine
```

## Usage

```sh
./combine inputfilename.srt
```

Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
