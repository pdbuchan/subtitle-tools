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

Warnings will be reported if subtitle numbers (not referring to timestamps here) are not in proper ascending order. These can be ignored if you wish, as most media players ignore subtitle numbers as long as integers are used. You can use [`offset`](../offset/) with 0 offset values as input and it will renumber the subtitles correctly.

If timestamps/subtitles are correct but the starting timestamps are not in ascending order, you can use [`reorder`](../reorder/) to sort subtitles by starting timestamp to ensure they are chronological.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
