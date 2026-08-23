#include <stdio.h>

#include "encodings.h"

// Return a convenient display name for an Encoding value. The Encoding enum
// is deliberately contiguous and starts at zero, so a table indexed by the
// enum avoids duplicating its numeric values in a switch statement.
const char *
enc_name (Encoding encoding) {

  static const char *const encoding_name[] = {
    "ISO_8859-1",
    "ISO_8859-2",
    "ISO_8859-3",
    "ISO_8859-4",
    "ISO_8859-5",
    "ISO_8859-6",
    "ISO_8859-7",
    "ISO_8859-8",
    "ISO_8859-9",
    "ISO_8859-10",
    "EUC-JP",
    "SHIFT-JIS",
    "JAPANESE_JIS",
    "BIG-5",
    "GB2312",
    "EUC-TW",
    "EUC-KR",
    "ISO-10646",
    "EUC-TW",
    "EUC_TW",
    "CHINESE_BIG5_CP950",
    "JAPANESE_CP932",
    "UTF8",
    "UNKNOWN_ENCODING",
    "ASCII_7BIT",
    "KOI8-R",
    "WINDOWS-1251",
    "WINDOWS-1252",
    "RUSSIAN_KOI8_RU",
    "CP1250",
    "ISO_8859-15",
    "CP1254",
    "CP1257",
    "ISO_8859_11",
    "MSFT_CP874",
    "MSFT_CP1256",
    "MSFT_CP1255",
    "ISO_8859_8_I",
    "HEBREW_VISUAL",
    "CZECH_CP852",
    "CZECH_CSN_369103",
    "MSFT_CP1253",
    "RUSSIAN_CP866",
    "ISO_8859_13",
    "ISO_2022_KR",
    "GBK",
    "GB18030",
    "BIG5_HKSCS",
    "ISO_2022_CN",
    "TSCII",
    "TAMIL_MONO",
    "TAMIL_BI",
    "JAGRAN",
    "MACINTOSH_ROMAN",
    "UTF7",
    "BHASKAR",
    "HTCHANAKYA",
    "UTF16BE",
    "UTF16LE",
    "UTF32BE",
    "UTF32LE",
    "BINARYENC",
    "HZ_GB_2312",
    "UTF8UTF8",
    "TAM_ELANGO",
    "TAM_LTTMBARANI",
    "TAM_SHREE",
    "TAM_TBOOMIS",
    "TAM_TMNEWS",
    "TAM_WEBTAMIL",
    "KDDI_SHIFT_JIS",
    "DOCOMO_SHIFT_JIS",
    "SOFTBANK_SHIFT_JIS",
    "KDDI_ISO_2022_JP",
    "SOFTBANK_ISO_2022_JP"
  };

  static_assert ((sizeof (encoding_name) / sizeof (encoding_name[0])) == NUM_ENCODINGS,
    "encoding_name[] must contain one entry for every Encoding value");

  if ((encoding < 0) || (encoding >= NUM_ENCODINGS)) {
    fprintf (stderr, "ERROR: Unknown encoding type value %i in enc_name().\n", (int) encoding);
    return (nullptr);
  }

  return (encoding_name[(int) encoding]);
}
