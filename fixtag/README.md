# fixtag

Finds and repairs common in-line markup tag errors in a SubRip file. Supported tags include italics, bold, underline, strikethrough, font color, font size, and position. If present, the Byte Order Mark (BOM) of the input file will be included in the output file.

## Build

```sh
gcc -Wall fixtag.c -o fixtag
```

## Usage

```sh
./fixtag inputfilename.srt [close]
```

Output is written to `out.srt`. The optional `close` argument appends missing closing tags to the final text line of a subtitle. Compare the result with the original file to verify the author's intended formatting.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
