# Character-encoding samples

A collection of sample text files in multiple languages and character encodings for testing [`ced`](../ced/) and [`enc`](../enc/).

The original English UTF-8 text was derived from the Wikipedia article about Mary Read. The text was translated into other languages and converted to a variety of encodings with `iconv`. Some names refer to encodings that are identical or subsets of one another.

Detection is probabilistic: depending on the language and encoding, `ced`, `chardet`, both, or neither may identify a sample correctly.

The filename convention is generally:

```text
ENCODING.language.sample
```

Examples include `UTF-8.en.sample`, `KOI8-R.ru.sample`, and `EUC-JP.jp.sample`.
