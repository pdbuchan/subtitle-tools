# ycbcr2rgb

Converts BT.709 YCbCr values to 8-bit sRGB. It assumes BT.709 color primaries and transfer characteristics were used to create the YCbCr values, then applies the sRGB transfer function to produce sRGB coordinates.

References include SMPTE RP 177-1993, ITU-R BT.709-6, ITU-T H.273, and IEC 61966-2-1.

## Build

```sh
gcc -Wall ycbcr2rgb.c -lm -o ycbcr2rgb
```

## Usage

```sh
./ycbcr2rgb
```

Results are written to standard output.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
