# writebom

Prepends a user-selected Byte Order Mark to a SubRip or text file when a BOM is not already present. It does not convert the file's character encoding; use [`enc`](../enc/) for conversion to UTF-8.

## Build

```sh
gcc -Wall writebom.c -o writebom
```

## Usage

```sh
./writebom inputfilename
```

Output is written to `out.txt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
