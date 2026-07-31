# ssa2srt

Converts a SubStation Alpha (`.ssa`) file to SubRip (`.srt`). It recognizes V4 and V4+ styles and transfers supported formatting for font color, bold, italic, underline, strikeout, and alignment. SSA attributes and override tags that have no SubRip equivalent are ignored.

## Build

```sh
gcc -Wall ssa2srt.c -o ssa2srt
```

## Usage

```sh
./ssa2srt inputfilename
```

Output is written to `out.srt`.

SSA does not require subtitle events to be chronological. If necessary, process the output with [`reorder`](../reorder/).

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
