# tag

Transfers supported formatting tags from one SubRip file to the text of another. This is intended for translation workflows in which formatting is stripped before translation and added back afterward.

The two SRT files must contain the same number of subtitles and the same number of lines in each subtitle. Because the text can differ, `tag` only handles opening tags at the beginning of a line and closing tags at the end of a line. The Byte Order Mark from the text input is preserved.

## Build

```sh
gcc -Wall tag.c -o tag
```

## Usage

```sh
./tag taginputfilename.srt textinputfilename.srt
```

Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
