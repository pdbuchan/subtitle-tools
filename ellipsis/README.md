# ellipsis

Removes SubRip entries whose text consists only of ellipsis marks, including common one-line and two-line forms made from `...` or the bogus form `---`. If present, the Byte Order Mark (BOM) of the input file will be included in the output file.

## Build

```sh
gcc -Wall ellipsis.c -o ellipsis
```

## Usage

```sh
./ellipsis inputfilename.srt
```

Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
