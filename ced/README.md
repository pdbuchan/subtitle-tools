# ced

A command-line adaptation of Google's Compact Encoding Detection code for analyzing SubRip and text files. The original detector was intended for web content; this front end reads a local file and reports the likely encoding to standard output.

## Build

```sh
make
```

## Usage

```sh
./ced inputfilename
```

## Licensing

This directory contains code under two licenses:

- The command-line front end written by P. David Buchan is licensed under the GNU General Public License, version 3 or later.
- Google Compact Encoding Detection source files retain their Apache License 2.0 notices.

See `LICENSE-GPL-3.0`, `LICENSE-APACHE-2.0`, and the individual source headers.
