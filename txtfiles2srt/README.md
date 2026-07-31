# txtfiles2srt

Creates one SubRip file from a collection of individual subtitle text files. The start and end timestamps are read from each filename. No Byte Order Mark is prepended.

Each subtitle text file should contain only subtitle text, without blank lines or trailing line feeds. Filenames must use this form:

```text
hh_mm_ss_ms__hh_mm_ss_ms.txt
```

For example:

```text
00_13_11_959__00_13_15_213.txt
```

## Build

```sh
gcc -Wall txtfiles2srt.c -o txtfiles2srt
```

## Usage

```sh
./txtfiles2srt filelistfilename
```

`filelistfilename` is a text file containing the subtitle text filenames, one per line. Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
