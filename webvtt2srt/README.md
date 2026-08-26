# webvtt2srt

Read a WebVTT (`.webvtt`) file and convert contents to SubRip format and save to `.srt` file.

## Limitations

* Bold, italics, and underline markups are preserved.
* Standard WebVTT colors are converted to SubRip format colors. For example, `<c.red>` is converted to `<font color="#FF0000">`.
* The color `green`, if it appears in a WebVTT file, is treated as an alias for the WebVTT color `lime`.
* Where multiple `<c>` classes appear, the last recognized foreground color is used.
* The WebVTT background class `bg_*` is ignored since it isn't supported in SubRip format.
* `<v Speaker>` and `<lang ...>` tags are removed while visible text are retained.
* Annotative/pronunciation tags `<ruby>` are discarded, base text is retained.
* Pronunciation/annotation text itself appearing within `<rt>...</rt>` is discarded.
* Internal timestamps such as `<00:01.500>` are discarded.
* An optional UTF-8 Byte Order Mark (BOM) at the beginning of the WebVTT file is accepted and skipped.
* Other recognized BOM-marked encodings are rejected because WebVTT input is parsed as UTF-8.

## Build

```sh
make
```

## Usage

```sh
./webvtt2srt filename.webvtt
```

The output file is `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
