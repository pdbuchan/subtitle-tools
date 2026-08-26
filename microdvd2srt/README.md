# microdvd2srt

Read a MicroDVD subtitle file and convert contents to SubRip format and save to `.srt` file.

Note that MicroDVD files typically have a `.sub` or `.txt` file name extension.

## Features and Limitations

* Allows for the common optional first record `{1}{1}25.000` which identifies the MicroDVD frame rate.
* `{y:i}`, `{y:b}`, and `{y:u}` are converted to `<i>`, `<b>`, and `<u>`, respectively.
* Combined styles such as `{y:i,b,u}` are recognized.
* Uppercase controls such as `{Y:i}` are treated as cue-persistent; lowercase controls apply to the logical line containing them.
* `{c:$BBGGRR}` is converted from MicroDVD's BGR ordering to SRT `<font color="#RRGGBB">`.
* `{f:Arial}` and `{s:12}` become face and size attributes on an SRT `<font>` element. Support for these attributes varies somewhat among SRT renderers.
* `{y:s}` strike-through is removed because there is no sufficiently portable SRT equivalent.
* Charset and positioning controls are recognized and removed.
* `{DEFAULT}` formatting is supported. Because MicroDVD permits it anywhere in the file, the program makes a preliminary pass so that a late `{DEFAULT}` declaration also applies correctly to earlier cues.
* `|` becomes a newline.
* The old `/Italic` text convention is supported.
* Optional `[BEGIN]` and `[END]` lines are ignored.
* An optional UTF-8 BOM is accepted and skipped. Other recognized BOM-marked encodings are rejected because the converter does not transcode character data.
* One deliberate limitation is a MicroDVD extension such as: `{123}{}Text` with no ending frame. Some readers accept this, but SRT requires a definite ending timestamp, and there is no reliable value to infer without making assumptions. The program therefore diagnoses it as invalid rather than inventing an end time.

## Build

```sh
make
```

## Usage

```sh
./microdvd2srt filename.sub
```

The output file is `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
