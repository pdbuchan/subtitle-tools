# Subtitle Tools in C

A collection of command-line tools for working with subtitle files, subtitle timing, character encodings, bitmap subtitle formats, chapter files, and colorspace conversion. The programs are written primarily in C; `ced` and `enc` include C++ code from Google Compact Encoding Detection.

Before processing a SubRip file with another tool, it is generally a good idea to run [`check`](check/) and correct any reported structural errors.

## Repository layout

Each tool has its own directory at the repository root. A single-file program keeps its source file and README directly in that directory; larger projects such as `sub`, `pgs`, and `dvb` retain their existing multi-file layouts. An additional `src/` layer would not add useful organization here and would make individual tools less convenient to browse and build.

## Tools

| Directory | Category | Description |
|---|---|---|
| [`check/`](check/) | SubRip | Validate a SubRip (`.srt`) file and report formatting errors and numbering or chronology warnings. |
| [`offset/`](offset/) | SubRip | Apply a positive, negative, or zero timestamp offset, preserve durations, and renumber subtitles. |
| [`sync/`](sync/) | SubRip | Synchronize all timestamps to two user-supplied anchor points while preserving subtitle durations. |
| [`sub/`](sub/) | VobSub | Analyze an `.idx`/`.sub` pair, extract subtitle bitmaps, or offset/synchronize timestamps. |
| [`txtfiles2srt/`](txtfiles2srt/) | Conversion | Create one SubRip file from individual text files whose filenames contain start and end timestamps. |
| [`ssa2srt/`](ssa2srt/) | Conversion | Convert SubStation Alpha subtitles to SubRip while transferring supported styles and markup. |
| [`ssa2srt-nostyles/`](ssa2srt-nostyles/) | Conversion | Convert SubStation Alpha subtitles to SubRip without transferring style definitions. |
| [`reorder/`](reorder/) | SubRip | Sort subtitles chronologically by starting timestamp. |
| [`srt2txt/`](srt2txt/) | Conversion | Extract subtitle text from an SRT file, optionally without blank lines between subtitles. |
| [`txt2srt/`](txt2srt/) | Conversion | Combine timestamps from an SRT file with translated or edited text from a text file. |
| [`tag/`](tag/) | Formatting | Transfer supported formatting tags from one SRT file to the text of another. |
| [`fixtag/`](fixtag/) | Formatting | Find and repair common SRT markup-tag errors, with an option to add missing closing tags. |
| [`striptag/`](striptag/) | Formatting | Remove all or selected markup-tag classes from an SRT file. |
| [`long/`](long/) | Formatting | Join two-line subtitles into one line for machine translation workflows. |
| [`ellipsis/`](ellipsis/) | Formatting | Remove subtitles containing only ellipsis marks or common bogus ellipsis forms. |
| [`time-text/`](time-text/) | Conversion | Combine timestamps from one SRT file with subtitle text from another. |
| [`combine/`](combine/) | SubRip | Merge identical subtitles that have immediately adjacent timestamps. |
| [`split/`](split/) | Testing | Split each subtitle into two identical consecutive subtitles for testing `combine`. |
| [`readbom/`](readbom/) | Encoding | Identify a Byte Order Mark in an SRT or text file. |
| [`writebom/`](writebom/) | Encoding | Prepend a selected Byte Order Mark when one is not already present. |
| [`stripbom/`](stripbom/) | Encoding | Remove a Byte Order Mark from an SRT or text file. |
| [`ced/`](ced/) | Encoding | Command-line adaptation of Google Compact Encoding Detection for text and SRT files. |
| [`enc/`](enc/) | Encoding | Detect likely encoding, convert text to UTF-8 with `iconv`, and optionally add a UTF-8 BOM. |
| [`samples/`](samples/) | Encoding | Sample text files in many languages and character encodings for testing detection tools. |
| [`time-diff/`](time-diff/) | Time | Calculate the difference between two timestamps entered interactively. |
| [`time-add/`](time-add/) | Time | Add two timestamps entered interactively. |
| [`pgs/`](pgs/) | PGS | Analyze Blu-ray PGS `.sup` subtitles, extract bitmaps, or offset/synchronize timestamps. |
| [`dvb/`](dvb/) | DVB | Analyze an isolated DVB subtitle stream in an MPEG-2 transport stream and optionally extract bitmaps. |
| [`chapters/`](chapters/) | Chapters | Generate an XML chapters file from a feature duration and desired chapter count. |
| [`bt709/`](bt709/) | Colorspace | Derive BT.709 RGB/YCbCr colorspace constants from the standard color primaries. |
| [`ycbcr2rgb/`](ycbcr2rgb/) | Colorspace | Convert BT.709 YCbCr values to 8-bit sRGB. |
| [`rgb2ycbcr/`](rgb2ycbcr/) | Colorspace | Convert 8-bit sRGB values to BT.709 YCbCr. |

## Building

GCC and GNU Make are sufficient for all included builds. Build everything from the repository root with:

```sh
make
```

Remove generated binaries and object files with:

```sh
make clean
```

Every tool's README also gives its individual build and usage command. The colorspace programs link against the math library. `enc` additionally requires the command-line programs `chardet` and `iconv` at runtime.

## Generated files

Several programs write fixed output names such as `out.srt`, `out.txt`, `sub.out`, `pgs.out`, `dvb.out`, or bitmap files. Run a tool in a working directory where those output names will not overwrite files you need.

## Source and background

These programs were originally documented at <https://www.pdbuchan.com/subtitles/subtitles.html>.

## License

Code written by P. David Buchan is distributed under the GNU General Public License, version 3 or later, as stated in the source headers and the root [`LICENSE`](LICENSE) file.

The `ced` and `enc` directories also contain Google Compact Encoding Detection source files licensed under the Apache License 2.0. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and the license files within those directories.
