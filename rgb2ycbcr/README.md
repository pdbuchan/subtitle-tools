# rgb2ycbcr

Converts 8-bit sRGB values to BT.709 YCbCr. It assumes the input uses the sRGB transfer function and applies the BT.709 transfer characteristics when producing YCbCr.

References include SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273, and IEC 61966-2-1.

## Build

```sh
gcc -Wall rgb2ycbcr.c -lm -o rgb2ycbcr
```

## Usage

```sh
./rgb2ycbcr
```

Results are written to standard output.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
