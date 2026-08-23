#include <stdio.h>
#include <stdlib.h>

#include "encodings.h"

// Print to stdout encoding name when passed encoding value.
int
enc_name (Encoding encoding) {

  switch (encoding) {
    case ISO_8859_1:
      fprintf (stdout, "ISO_8859-1 (LATIN1, ISO-IR-100, CSISOLATIN1, L1, IBM819, WINDOWS-28591)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_2:
      fprintf (stdout, "ISO_8859-2 (LATIN2, WINDOWS-28592)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_3:
      fprintf (stdout, "ISO_8859-3 (LATIN3, IBM913, WINDOWS-28593)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_4:
      fprintf (stdout, "ISO_8859-4 (LATIN4, IBM914, WINDOWS-28594)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_5:
      fprintf (stdout, "ISO_8859-5 (IBM915, WINDOWS-28595)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_6:
      fprintf (stdout, "ISO_8859-6 (IBM1089, ASMO-708)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_7:
      fprintf (stdout, "ISO_8859-7 (WINDOWS-28597)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_8:
      fprintf (stdout, "ISO_8859-8 (Latin/Hebrew)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_9:
      fprintf (stdout, "ISO_8859-9 (LATIN5, ECMA-128, TS 5881)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_10:
      fprintf (stdout, "ISO_8859-10 (LATIN6, WINDOWS-28600)\n");
      return (EXIT_SUCCESS);

    case JAPANESE_EUC_JP:
      fprintf (stdout, "EUC-JP (Unixized JIS (UJIS), AT&T JIS)\n");
      return (EXIT_SUCCESS);

    case JAPANESE_SHIFT_JIS:
      fprintf (stdout, "SHIFT-JIS (SJIS, PCK)\n");
      return (EXIT_SUCCESS);

    case JAPANESE_JIS:
      fprintf (stdout, "JAPANESE_JIS (Teragram JIS)\n");
      return (EXIT_SUCCESS);

    case CHINESE_BIG5:
      fprintf (stdout, "BIG-5 (BIG5, BIG-FIVE, BIGFIVE)\n");
      return (EXIT_SUCCESS);

    case CHINESE_GB:
      fprintf (stdout, "GB (GB2312)\n");
      return (EXIT_SUCCESS);

    case CHINESE_EUC_CN:
      fprintf (stdout, "EUC-TW\n");
      return (EXIT_SUCCESS);

    case KOREAN_EUC_KR:
      fprintf (stdout, "EUC-KR (Korean Wansung, EUC Korean)\n");
      return (EXIT_SUCCESS);

    case UNICODE:
      fprintf (stdout, "UNICODE (ISO-10646/UCS2)\n");
      return (EXIT_SUCCESS);

    case CHINESE_EUC_DEC:
      fprintf (stdout, "EUC-TW\n");
      return (EXIT_SUCCESS);

    case CHINESE_CNS:
      fprintf (stdout, "Should be EUC_TW\n");
      return (EXIT_SUCCESS);

    case CHINESE_BIG5_CP950:
      fprintf (stdout, "CHINESE_BIG5_CP950 (Teragram BIG5_CP950)\n");
      return (EXIT_SUCCESS);

    case JAPANESE_CP932:
      fprintf (stdout, "JAPANESE_CP932 (Teragram CP932)\n");
      return (EXIT_SUCCESS);

    case UTF8:
      fprintf (stdout, "UTF8\n");
      return (EXIT_SUCCESS);

    case UNKNOWN_ENCODING:
      fprintf (stdout, "UNKNOWN_ENCODING\n");
      return (EXIT_SUCCESS);

    case ASCII_7BIT:
      fprintf (stdout, "ASCII_7BIT (ISO_8859_1 with all characters <= 127. Should *never* as a result of Document::encoding().)\n");
      return (EXIT_SUCCESS);

    case RUSSIAN_KOI8_R:
      fprintf (stdout, "KOI8-R (IBM878)\n");
      return (EXIT_SUCCESS);

    case RUSSIAN_CP1251:
      fprintf (stdout, "WINDOWS-1251 (CP1251)\n");
      return (EXIT_SUCCESS);

    case MSFT_CP1252:
      fprintf (stdout, "WINDOWS-1252 (CP1252, IBM1252, WE8MSWIN1252)\n");
      return (EXIT_SUCCESS);

    case RUSSIAN_KOI8_RU:
      fprintf (stdout, "RUSSIAN_KOI8_RU (CP21866 aka KOI8-U)\n");
      return (EXIT_SUCCESS);

    case MSFT_CP1250:
      fprintf (stdout, "CP1250 (WINDOWS-1250, IBM1250)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_15:
      fprintf (stdout, "ISO_8859-15 (WINDOWS-28605, ISO_8859-0, ISO_8859-1, IBM923)\n");
      return (EXIT_SUCCESS);

    case MSFT_CP1254:
      fprintf (stdout, "CP1254 (WINDOWS-1254, IBM1254)\n");
      return (EXIT_SUCCESS);

    case MSFT_CP1257:
      fprintf (stdout, "CP1257 (WINDOWS-1257, IBM1257)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_11:
      fprintf (stdout, "ISO_8859_11 (aka TIS-620, used for Thai)\n");
      return (EXIT_SUCCESS);

    case MSFT_CP874:
      fprintf (stdout, "MSFT_CP874 (WINDOWS-874 used for Thai)\n");
      return (EXIT_SUCCESS);

    case MSFT_CP1256:
      fprintf (stdout, "MSFT_CP1256 (WINDOWS-1256 used for Arabic)\n");
      return (EXIT_SUCCESS);

    case MSFT_CP1255:
      fprintf (stdout, "MSFT_CP1255 (WINDOWS-1255 Logical Hebrew Microsoft)\n");
      return (EXIT_SUCCESS);

    case ISO_8859_8_I:
      fprintf (stdout, "ISO_8859_8_I (Iso Hebrew Logical)\n");
      return (EXIT_SUCCESS);

    case HEBREW_VISUAL:
      fprintf (stdout, "HEBREW_VISUAL (Iso Hebrew Visual)\n");
      return (EXIT_SUCCESS);

    case CZECH_CP852:
      fprintf (stdout, "CZECH_CP852\n");
      return (EXIT_SUCCESS);

    case CZECH_CSN_369103:
      fprintf (stdout, "CZECH_CSN_369103 (aka ISO_IR_139 aka KOI8_CS)\n");
      return (EXIT_SUCCESS);

    case MSFT_CP1253:
      fprintf (stdout, "MSFT_CP1253 (WINDOWS-1253 used for Greek)\n");
      return (EXIT_SUCCESS);

    case RUSSIAN_CP866:
      fprintf (stdout, "RUSSIAN_CP866\n");
      return (EXIT_SUCCESS);

    case ISO_8859_13:
      fprintf (stdout, "ISO_8859_13\n");
      return (EXIT_SUCCESS);

    case ISO_2022_KR:
      fprintf (stdout, "ISO_2022_KR\n");
      return (EXIT_SUCCESS);

    case GBK:
      fprintf (stdout, "GBK\n");
      return (EXIT_SUCCESS);

    case GB18030:
      fprintf (stdout, "GB18030\n");
      return (EXIT_SUCCESS);

    case BIG5_HKSCS:
      fprintf (stdout, "BIG5_HKSCS\n");
      return (EXIT_SUCCESS);

    case ISO_2022_CN:
      fprintf (stdout, "ISO_2022_CN\n");
      return (EXIT_SUCCESS);

    case TSCII:
      fprintf (stdout, "TSCII\n");
      return (EXIT_SUCCESS);

    case TAMIL_MONO:
      fprintf (stdout, "TAMIL_MONO\n");
      return (EXIT_SUCCESS);

    case TAMIL_BI:
      fprintf (stdout, "TAMIL_BI\n");
      return (EXIT_SUCCESS);

    case JAGRAN:
      fprintf (stdout, "JAGRAN\n");
      return (EXIT_SUCCESS);

    case MACINTOSH_ROMAN:
      fprintf (stdout, "MACINTOSH_ROMAN\n");
      return (EXIT_SUCCESS);

    case UTF7:
      fprintf (stdout, "UTF7\n");
      return (EXIT_SUCCESS);

    case BHASKAR:
      fprintf (stdout, "BHASKAR (Indic encoding - Devanagari)\n");
      return (EXIT_SUCCESS);

    case HTCHANAKYA:
      fprintf (stdout, "HTCHANAKYA (56 Indic encoding - Devanagari)\n");
      return (EXIT_SUCCESS);

    case UTF16BE:
      fprintf (stdout, "UTF16BE (big-endian UTF-16)\n");
      return (EXIT_SUCCESS);

    case UTF16LE:
      fprintf (stdout, "UTF16LE (little-endian UTF-16)\n");
      return (EXIT_SUCCESS);

    case UTF32BE:
      fprintf (stdout, "UTF32BE (big-endian UTF-32, big-endian UCS-4)\n");
      return (EXIT_SUCCESS);

    case UTF32LE:
      fprintf (stdout, "UTF32LE (little-endian UTF-32, little-endian UCS-4)\n");
      return (EXIT_SUCCESS);

    case BINARYENC:
      fprintf (stdout, "BINARYENC\n");
      return (EXIT_SUCCESS);

    case HZ_GB_2312:
      fprintf (stdout, "HZ_GB_2312\n");
      return (EXIT_SUCCESS);

    case UTF8UTF8:
      fprintf (stdout, "UTF8UTF8\n");
      return (EXIT_SUCCESS);

    case TAM_ELANGO:
      fprintf (stdout, "TAM_ELANGO (Elango - Tamil)\n");
      return (EXIT_SUCCESS);

    case TAM_LTTMBARANI:
      fprintf (stdout, "TAM_LTTMBARANI (Barani - Tamil)\n");
      return (EXIT_SUCCESS);

    case TAM_SHREE:
      fprintf (stdout, "TAM_SHREE (Shree - Tamil)\n");
      return (EXIT_SUCCESS);

    case TAM_TBOOMIS:
      fprintf (stdout, "TAM_TBOOMIS (TBoomis - Tamil)\n");
      return (EXIT_SUCCESS);

    case TAM_TMNEWS:
      fprintf (stdout, "TAM_TMNEWS (TMNews - Tamil)\n");
      return (EXIT_SUCCESS);

    case TAM_WEBTAMIL:
      fprintf (stdout, "TAM_WEBTAMIL (Webtamil - Tamil)\n");
      return (EXIT_SUCCESS);

    case KDDI_SHIFT_JIS:
      fprintf (stdout, "KDDI_SHIFT_JIS\n");
      return (EXIT_SUCCESS);

    case DOCOMO_SHIFT_JIS:
      fprintf (stdout, "DOCOMO_SHIFT_JIS\n");
      return (EXIT_SUCCESS);

    case SOFTBANK_SHIFT_JIS:
      fprintf (stdout, "SOFTBANK_SHIFT_JIS\n");
      return (EXIT_SUCCESS);

    case KDDI_ISO_2022_JP:
      fprintf (stdout, "KDDI_ISO_2022_JP\n");
      return (EXIT_SUCCESS);

    case SOFTBANK_ISO_2022_JP:
      fprintf (stdout, "SOFTBANK_ISO_2022_JP\n");
      return (EXIT_SUCCESS);

    case NUM_ENCODINGS:
      fprintf (stderr, "Unknown encoding type value %i in enc_name().\n", (int) encoding);
      return (EXIT_FAILURE);

    default:
      // Guard against an out-of-range integer cast to Encoding.
      fprintf (stderr, "Unknown encoding type value %i in enc_name().\n", (int) encoding);
      return (EXIT_FAILURE);
  }
}
