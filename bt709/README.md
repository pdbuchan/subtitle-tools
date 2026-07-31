# bt709

Derives RGB color constants for the BT.709 YCbCr colorspace. The Normalized Primary Matrix, including K<sub>R</sub>, K<sub>G</sub>, and K<sub>B</sub>, is derived from the BT.709 color primaries.

References include SMPTE RP 177-1993, ITU-R BT.709-6, and ITU-T H.273.

## Build

```sh
gcc -Wall bt709.c -lm -o bt709
```

## Usage

```sh
./bt709
```

Results are written to standard output.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
