# long

Joins each two-line SubRip subtitle into a single line so complete sentences can be presented to a machine translator. The program is designed for characters encountered in English and French. It also changes standalone bogus ellipsis marks (`---`) to `...`. An input Byte Order Mark is preserved.

Use this tool after removing markup with [`striptag`](../striptag/).

## Build

```sh
gcc -Wall long.c -o long
```

## Usage

```sh
./long inputfilename.srt
```

Output is written to `out.srt`.

## License

The collection is distributed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
