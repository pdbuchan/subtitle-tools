# ellipsis

Read an existing SubRip file and remove any subtitles consisting of only one of the following:

`...\n`
`...\n...\n`
` ...\n...\n`
`...\n ...\n`

or bogus ellipsis marks:

`---\n`
`---\n---\n`
` ---\n---\n`
`---\n ---\n`

If present, the Byte Order Mark (BOM) of the input file will be included in the output file.

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
