# check

Checks a SubRip (`.srt`) file for structural errors and reports the results. Run this before using an SRT file as input to the other SubRip tools.

## Build

```sh
gcc -Wall check.c -o check
```

## Usage

```sh
./check inputfilename.srt
```

Results are written to standard output. Correct any reported error and run `check` again until no errors remain.

Warnings about subtitle numbers being out of ascending order can usually be ignored by media players. To renumber entries, run [`offset`](../offset/) with zero offsets. If starting timestamps are not chronological, use [`reorder`](../reorder/).

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
