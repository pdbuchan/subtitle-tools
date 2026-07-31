# ssa2srt-nostyles

Converts a SubStation Alpha (`.ssa`) file to SubRip (`.srt`) without transferring SSA style definitions. Supported inline markup for font color, bold, italic, underline, strikeout, and alignment is retained.

## Build

```sh
gcc -Wall ssa2srt-nostyles.c -o ssa2srt-nostyles
```

## Usage

```sh
./ssa2srt-nostyles inputfilename
```

Output is written to `out.srt`. Because SSA events need not be chronological, use [`reorder`](../reorder/) on the result when needed.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
