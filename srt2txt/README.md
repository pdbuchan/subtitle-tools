# srt2txt

Extracts only the text lines from a SubRip file. This is useful when sending subtitle text to a translation tool without including subtitle numbers and timestamps. If present, the Byte Order Mark (BOM) of the input file will be included in the output file.

## Build

```sh
gcc -Wall srt2txt.c -o srt2txt
```

## Usage

```sh
./srt2txt inputfilename.srt [nospace]
```

Output is written to `out.txt`. By default, subtitle texts are separated by blank lines. The optional `nospace` argument suppresses those blank lines.

Use [`txt2srt`](../txt2srt/) to combine translated text with the original timestamps.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
