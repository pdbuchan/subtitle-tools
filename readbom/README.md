# readbom

Examines a SubRip or text file for an existing Byte Order Mark and reports the result. This does not infer character encoding from the file's text; use [`ced`](../ced/) or [`enc`](../enc/) for encoding detection.

## Build

```sh
gcc -Wall readbom.c -o readbom
```

## Usage

```sh
./readbom inputfilename
```

Results are written to standard output.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
