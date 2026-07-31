# pgs

Analyzes a Presentation Graphic Stream (`.sup`) subtitle file and writes a report. Optional operations extract subtitle bitmaps, apply timestamp offsets, or synchronize timestamps to new anchor points while preserving subtitle durations. Input can originate from HD or UHD Blu-ray material.

## Build

```sh
make
```

## Usage

```sh
./pgs filename.sup [option]
```

Options are used one at a time:

| Option | Action |
|---|---|
| `bmp` | Produce a bitmap for every subtitle. |
| `offset` | Apply user-entered timestamp offsets and write `out.sup`. |
| `sync` | Synchronize timestamps to user-entered anchor points and write `out.sup`. |

The normal report is `pgs.out`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
