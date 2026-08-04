# tag

Transfers supported formatting tags from one SubRip file to the text of another. This is intended for translation workflows in which formatting is stripped before translation and added back afterward.

The two SRT files must contain the same number of subtitles and the same number of lines in each subtitle. Because the text can differ, `tag` only handles opening tags at the beginning of a line and closing tags at the end of a line. The Byte Order Mark from the text input is preserved.

The following are some examples of in-line format tag position arrangements that `tag` can process:

```sh
2
00:00:44,819 --> 00:00:46,819
<font color="#ffffff">* Unheimliches Knurren *  </font>

3
00:00:47,719 --> 00:00:51,219
<font color="#ffffff">Ich geh vielleicht
als Frankenstein auf Falkenstein.  </font>

4
00:00:51,319 --> 00:00:55,819
Boah. Also ich hab noch keinen Plan,
als was ich mich verkleide.

5
00:00:55,819 --> 00:00:58,519
<font color="#00ffff"><i>Aber Bibi,  </i></font>
<font color="#00ffff">die hat richtig gute Ideen,  </font>
```

## Build

```sh
gcc -Wall tag.c -o tag
```

## Usage

```sh
./tag taginputfilename.srt textinputfilename.srt
```

Output is written to `out.srt`.

## License

This program is licensed under the GNU General Public License, version 3 or later. See the repository root `LICENSE` file.
