# Subtitle Tools in C

A collection of command-line tools for working with subtitle files, subtitle timing, character encodings, bitmap subtitle formats, chapter files, and colorspace conversion. The programs are written primarily in C; `ced` and `enc` include C++ code from Google Compact Encoding Detection.

Before processing a SubRip file with another tool, it is generally a good idea to run [`check`](check/) and correct any reported structural errors.

## Repository layout

Each tool has its own directory at the repository root. A single-file program keeps its source file and README directly in that directory; larger projects such as `sub`, `pgs`, and `dvb` retain their existing multi-file layouts. An additional `src/` layer would not add useful organization here and would make individual tools less convenient to browse and build.

## Check Tool

| Directory | Description |
|---|---|
| [`check/`](check/) | Validate a SubRip (`.srt`) file and report formatting errors and numbering or chronology warnings. |

## Time and Order Adjustment Tools
| Directory | Description |
|---|---|
| [`offset/`](offset/) | Apply a positive, negative, or zero timestamp offset, preserve durations, and renumber subtitles. |
| [`sync/`](sync/) | Synchronize all timestamps to two user-supplied anchor points while preserving subtitle durations. |
| [`reorder/`](reorder/) | Sort subtitles chronologically by starting timestamp. |
| [`time-text/`](time-text/) | Combine timestamps from one SRT file with subtitle text from another. |

## Chapter Tool
| Directory | Description |
|---|---|
| [`chapters/`](chapters/) | Generate an XML chapters file from a feature duration and desired chapter count. |

## VobSub, PGS, and DVB Subtitle Tools
| Directory | Description |
|---|---|
| [`sub/`](sub/) | Analyze an `.idx`/`.sub` pair, extract subtitle bitmaps, or offset/synchronize timestamps. |
| [`pgs/`](pgs/) | Analyze Blu-ray PGS `.sup` subtitles, extract subtitle bitmaps, or offset/synchronize timestamps. |
| [`dvb/`](dvb/) | Analyze an isolated DVB subtitle stream in an MPEG-2 transport stream and optionally extract subtitle bitmaps. |

## Post-OCR Tool
| Directory | Description |
|---|---|
| [`txtfiles2srt/`](txtfiles2srt/) | Create one SubRip file from individual text files whose filenames contain start and end timestamps. |

## In-Line Markup Tag Tools
| Directory | Description |
|---|---|
| [`fixtag/`](fixtag/) | Find and repair common SRT markup tag errors, with an option to add missing closing tags. |
| [`striptag/`](striptag/) | Remove all or selected markup tag classes from an SRT file. |
| [`tag/`](tag/) | Transfer supported formatting tags from one SRT file to the text of another. |

## Contiguous Subtitle Combine / Split Tools
| Directory | Description |
|---|---|
| [`combine/`](combine/) | Merge identical subtitles that have immediately adjacent timestamps. |
| [`split/`](split/) | Split each subtitle into two identical consecutive subtitles for testing `combine`. |

## Tools for Aiding in Translation Preparation
When machine-translating a SubRip (`.srt`) from one language to another, you can improve the results by removing markup tags, timestamps, and keeping sentences on a single line. A typical workflow might consist of:

1. Removing all in-line markup tags using [`striptag`](striptag/).
2. Removing any lines containing only ellipses ("...") using [`ellipsis`](ellipsis/). These are common in French SDH subtitles, but often undesirable in other languages.
3. Combine adjacent subtitles that have contiguous timestamps using [`combine`](combine/). i.e., the end timestamp of one subtitle is also the start timestamp of the subsequent subtitle.
4. Join two lines of text which are part of the same sentence using [`long`](long/).
5. Removing subtitle numbering and timestamps using [`srt2txt`](srt2txt/)
6. Passing the resulting text file to a translation service such as [DeepL](https://www.deepl.com).
7. Adding back subtitle numbering and timestamps using [`txt2srt`](txt2srt/).
8. Using [Subtitle Edit](https://www.nikse.dk/subtitleedit) to split long sentences into multple lines, and correct common errors: Click on Tools, Fix common errors...

It is difficult to automatically reinstate any stripped in-line markup tags from the original SubRip file onto a translated file.

| Directory | Description |
|---|---|
| [`ellipsis/`](ellipsis/) | Remove subtitles containing only ellipsis marks or common bogus ellipsis forms. |
| [`long/`](long/) | Join two-line subtitles into one line for machine translation workflows. |
| [`srt2txt/`](srt2txt/) | Extract subtitle text from an SRT file, optionally without blank lines between subtitles. |
| [`txt2srt/`](txt2srt/) | Combine timestamps from an SRT file with translated or edited text from a text file. |

## SubStation Alpha Tools
| Directory | Description |
|---|---|
| [`ssa2srt/`](ssa2srt/) | Convert SubStation Alpha subtitles to SubRip while transferring supported styles and markup. |
| [`ssa2srt-nostyles/`](ssa2srt-nostyles/) | Convert SubStation Alpha subtitles to SubRip without transferring style definitions. |

## Byte Order Mark Tools
| Directory | Description |
|---|---|
| [`readbom/`](readbom/) | Identify a Byte Order Mark in an SRT or text file. |
| [`writebom/`](writebom/) | Prepend a selected Byte Order Mark when one is not already present. |
| [`stripbom/`](stripbom/) | Remove a Byte Order Mark from an SRT or text file. |

## Character Encoding Tools
| Directory | Description |
|---|---|
| [`ced/`](ced/) | Command-line adaptation of Google Compact Encoding Detection for text and SRT files. |
| [`enc/`](enc/) | Detect likely encoding, convert text to UTF-8 with `iconv`, and optionally add a UTF-8 BOM. |
| [`samples/`](samples/) | Sample text files in many languages and character encodings for testing detection tools. |

## Handy Timestamp Tools
| Directory | Description |
|---|---|
| [`time-diff/`](time-diff/) | Calculate the difference between two timestamps entered interactively. |
| [`time-add/`](time-add/) | Add two timestamps entered interactively. |

## Colorspace tools
| Directory | Description |
|---|---|
| [`bt709/`](bt709/) | Derive BT.709 RGB/YCbCr colorspace constants from the standard color primaries. |
| [`ycbcr2rgb/`](ycbcr2rgb/) | Convert BT.709 YCbCr values to 8-bit sRGB. |
| [`rgb2ycbcr/`](rgb2ycbcr/) | Convert 8-bit sRGB values to BT.709 YCbCr. |

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

## License

Code written by P. David Buchan is distributed under the GNU General Public License, version 3 or later, as stated in the source headers and the root [`LICENSE`](LICENSE) file.

The `ced` and `enc` directories also contain Google Compact Encoding Detection source files licensed under the Apache License 2.0. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and the license files within those directories.
