# enc

Detects the likely character encoding of a SubRip or text file using both Google Compact Encoding Detection and the Linux `chardet` command. It asks the user to select the most likely result, converts the file to UTF-8 with `iconv`, and can prepend a UTF-8 Byte Order Mark.

## Requirements

- A C/C++ build environment and GNU Make
- `chardet`
- `iconv`

The program invokes `chardet` and `iconv` through system calls.

## Build

```sh
make
```

The included Google detector code can produce numerous compiler warnings.

## Usage

```sh
./enc inputfilename
```

When prompted, enter an encoding name accepted by `iconv`, such as `KOI8-R` or `ISO_8859-2`. Output is written to `out.txt`.

## Licensing

This directory contains code under two licenses:

- The front end written by P. David Buchan is licensed under the GNU General Public License, version 3 or later.
- Google Compact Encoding Detection source files retain their Apache License 2.0 notices.

See `LICENSE-GPL-3.0`, `LICENSE-APACHE-2.0`, and the individual source headers.
