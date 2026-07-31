# txt2srt

Combines timestamps from a SubRip file with text from a separate text file. The files must contain the same number of subtitles, and the text entries must be separated by single blank lines. A Byte Order Mark in the text file is copied to the output.

## Build

```sh
gcc -Wall txt2srt.c -o txt2srt
```

## Usage

```sh
./txt2srt inputfilename.srt inputfilename.txt
```

Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
