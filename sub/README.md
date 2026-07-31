# sub

Analyzes a VobSub `.idx`/`.sub` pair and writes a detailed report. Optional operations extract each subtitle as a bitmap, apply a timestamp offset, or synchronize timestamps to new anchor points while preserving durations.

Input can originate from NTSC or PAL DVD material or from HD/UHD Blu-ray sources. Files containing many subtitle languages can generate a very large number of bitmap files.

## Build

```sh
make
```

## Usage

```sh
./sub filename.idx filename.sub [option]
```

Options are used one at a time:

| Option | Action |
|---|---|
| `bmp` | Produce a bitmap for every subtitle. |
| `offset` | Apply user-entered offsets and write `out.idx` and `out.sub`. |
| `sync` | Synchronize timestamps to user-entered anchor points and write `out.idx` and `out.sub`. |

The normal report is `sub.out`. This report can be similar in size to the input `.sub` file.

Bitmap filenames include start and end times, language ID, and language index. The language index distinguishes multiple streams that use the same language ID.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
