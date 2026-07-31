# sync

Synchronizes all timestamps in a SubRip file to two user-supplied anchor points. Subtitle durations are preserved, and an input Byte Order Mark is retained.

## Build

```sh
gcc -Wall sync.c -o sync
```

## Usage

```sh
./sync inputfilename.srt
```

The program writes `out.srt` and asks for:

1. The current start timestamp of an early subtitle.
2. The current start timestamp of a late subtitle.
3. The desired start timestamp of the early subtitle.
4. The desired start timestamp of the late subtitle.

Choose anchors near the beginning and end of the feature to maximize scaling accuracy.

Example input:

```text
Current start timestamp for first anchor point subtitle (hh:mm:ss,ms)? 00:00:29,280
Current start timestamp for last anchor point subtitle (hh:mm:ss,ms)? 01:31:29,280
New start timestamp for first anchor point subtitle (hh:mm:ss,ms)? 00:00:22,280
New start timestamp for last anchor point subtitle (hh:mm:ss,ms)? 01:31:25,000
```

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
