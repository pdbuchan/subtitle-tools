# striptag

Removes markup tags from a SubRip file. Supported classes include italics, bold, underline, strikethrough, font color, font size, and position. An input Byte Order Mark is preserved.

## Build

```sh
gcc -Wall striptag.c -o striptag
```

## Usage

```sh
./striptag inputfilename.srt option
```

Options:

| Option | Tags removed |
|---|---|
| `a` | All supported markup tags |
| `i` | Italics |
| `b` | Bold |
| `u` | Underline |
| `s` | Strikeout |
| `f` | Font color and size |
| `p` | Position |

Use one option at a time. Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
