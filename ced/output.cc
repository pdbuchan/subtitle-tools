#include <stdio.h>
#include <stdlib.h>

// Print to stdout encoding name when passed encoding value.
int
enc_name (int number) {

  switch (number) {
    case 0:
      fprintf (stdout, "ISO_8859-1 (LATIN1, ISO-IR-100, CSISOLATIN1, L1, IBM819, WINDOWS-28591)\n");
      return (EXIT_SUCCESS);

    case 1:
      fprintf (stdout, "ISO_8859-2 (LATIN2, WINDOWS-28592)\n");
      return (EXIT_SUCCESS);

    case 2:
      fprintf (stdout, "ISO_8859-3 (LATIN3, IBM913, WINDOWS-28593)\n");
      return (EXIT_SUCCESS);

    case 3:
      fprintf (stdout, "ISO_8859-4 (LATIN4, IBM914, WINDOWS-28594)\n");
      return (EXIT_SUCCESS);

    case 4:
      fprintf (stdout, "ISO_8859-5 (IBM915, WINDOWS-28595)\n");
      return (EXIT_SUCCESS);

    case 5:
      fprintf (stdout, "ISO_8859-6 (IBM1089, ASMO-708)\n");
      return (EXIT_SUCCESS);

    case 6:
      fprintf (stdout, "ISO_8859-7 (WINDOWS-28597)\n");
      return (EXIT_SUCCESS);

    case 7:
      fprintf (stdout, "ISO_8859-8 (Latin/Hebrew)\n");
      return (EXIT_SUCCESS);

    case 8:
      fprintf (stdout, "ISO_8859-9 (LATIN5, ECMA-128, TS 5881)\n");
      return (EXIT_SUCCESS);

    case 9:
      fprintf (stdout, "ISO_8859-10 (LATIN6, WINDOWS-28600)\n");
      return (EXIT_SUCCESS);

    case 10:
      fprintf (stdout, "EUC-JP (Unixized JIS (UJIS), AT&T JIS)\n");
      return (EXIT_SUCCESS);

    case 11:
      fprintf (stdout, "SHIFT-JIS (SJIS, PCK)\n");
      return (EXIT_SUCCESS);

    case 12:
      fprintf (stdout, "JAPANESE_JIS (Teragram JIS)\n");
      return (EXIT_SUCCESS);

    case 13:
      fprintf (stdout, "BIG-5 (BIG5, BIG-FIVE, BIGFIVE)\n");
      return (EXIT_SUCCESS);

    case 14:
      fprintf (stdout, "GB (GB2312)\n");
      return (EXIT_SUCCESS);

    case 15:
      fprintf (stdout, "EUC-TW\n");
      return (EXIT_SUCCESS);

    case 16:
      fprintf (stdout, "EUC-KR (Korean Wansung, EUC Korean)\n");
      return (EXIT_SUCCESS);

    case 17:
      fprintf (stdout, "UNICODE (ISO-10646/UCS2)\n");
      return (EXIT_SUCCESS);

    case 18:
      fprintf (stdout, "EUC-TW\n");
      return (EXIT_SUCCESS);

    case 19:
      fprintf (stdout, "Should be EUC_TW\n");
      return (EXIT_SUCCESS);

    case 20:
      fprintf (stdout, "CHINESE_BIG5_CP950 (Teragram BIG5_CP950)\n");
      return (EXIT_SUCCESS);

    case 21:
      fprintf (stdout, "JAPANESE_CP932 (Teragram CP932)\n");
      return (EXIT_SUCCESS);

    case 22:
      fprintf (stdout, "UTF8\n");
      return (EXIT_SUCCESS);

    case 23:
      fprintf (stdout, "UNKNOWN_ENCODING\n");
      return (EXIT_SUCCESS);

    case 24:
      fprintf (stdout, "ASCII_7BIT (ISO_8859_1 with all characters <= 127. Should *never* as a result of Document::encoding().)\n");
      return (EXIT_SUCCESS);

    case 25:
      fprintf (stdout, "KOI8-R (IBM878)\n");
      return (EXIT_SUCCESS);

    case 26:
      fprintf (stdout, "WINDOWS-1251 (CP1251)\n");
      return (EXIT_SUCCESS);

    case 27:
      fprintf (stdout, "WINDOWS-1252 (CP1252, IBM1252, WE8MSWIN1252)\n");
      return (EXIT_SUCCESS);

    case 28:
      fprintf (stdout, "RUSSIAN_KOI8_RU (CP21866 aka KOI8-U)\n");
      return (EXIT_SUCCESS);

    case 29:
      fprintf (stdout, "CP1250 (WINDOWS-1250, IBM1250)\n");
      return (EXIT_SUCCESS);

    case 30:
      fprintf (stdout, "ISO_8859-15 (WINDOWS-28605, ISO_8859-0, ISO_8859-1, IBM923)\n");
      return (EXIT_SUCCESS);

    case 31:
      fprintf (stdout, "CP1254 (WINDOWS-1254, IBM1254)\n");
      return (EXIT_SUCCESS);

    case 32:
      fprintf (stdout, "CP1257 (WINDOWS-1257, IBM1257)\n");
      return (EXIT_SUCCESS);

    case 33:
      fprintf (stdout, "ISO_8859_11 (aka TIS-620, used for Thai)\n");
      return (EXIT_SUCCESS);

    case 34:
      fprintf (stdout, "MSFT_CP874 (WINDOWS-874 used for Thai)\n");
      return (EXIT_SUCCESS);

    case 35:
      fprintf (stdout, "MSFT_CP1256 (WINDOWS-1256 used for Arabic)\n");
      return (EXIT_SUCCESS);

    case 36:
      fprintf (stdout, "MSFT_CP1255 (WINDOWS-1255 Logical Hebrew Microsoft)\n");
      return (EXIT_SUCCESS);

    case 37:
      fprintf (stdout, "ISO_8859_8_I (Iso Hebrew Logical)\n");
      return (EXIT_SUCCESS);

    case 38:
      fprintf (stdout, "HEBREW_VISUAL (Iso Hebrew Visual)\n");
      return (EXIT_SUCCESS);

    case 39:
      fprintf (stdout, "CZECH_CP852\n");
      return (EXIT_SUCCESS);

    case 40:
      fprintf (stdout, "CZECH_CSN_369103 (aka ISO_IR_139 aka KOI8_CS)\n");
      return (EXIT_SUCCESS);

    case 41:
      fprintf (stdout, "MSFT_CP1253 (WINDOWS-1253 used for Greek)\n");
      return (EXIT_SUCCESS);

    case 42:
      fprintf (stdout, "RUSSIAN_CP866\n");
      return (EXIT_SUCCESS);

    case 43:
      fprintf (stdout, "ISO_8859_13\n");
      return (EXIT_SUCCESS);

    case 44:
      fprintf (stdout, "ISO_2022_KR\n");
      return (EXIT_SUCCESS);

    case 45:
      fprintf (stdout, "GBK\n");
      return (EXIT_SUCCESS);

    case 46:
      fprintf (stdout, "GB18030\n");
      return (EXIT_SUCCESS);

    case 47:
      fprintf (stdout, "BIG5_HKSCS\n");
      return (EXIT_SUCCESS);

    case 48:
      fprintf (stdout, "ISO_2022_CN\n");
      return (EXIT_SUCCESS);

    case 49:
      fprintf (stdout, "TSCII\n");
      return (EXIT_SUCCESS);

    case 50:
      fprintf (stdout, "TAMIL_MONO\n");
      return (EXIT_SUCCESS);

    case 51:
      fprintf (stdout, "TAMIL_BI\n");
      return (EXIT_SUCCESS);

    case 52:
      fprintf (stdout, "JAGRAN\n");
      return (EXIT_SUCCESS);

    case 53:
      fprintf (stdout, "MACINTOSH_ROMAN\n");
      return (EXIT_SUCCESS);

    case 54:
      fprintf (stdout, "UTF7\n");
      return (EXIT_SUCCESS);

    case 55:
      fprintf (stdout, "BHASKAR (Indic encoding - Devanagari)\n");
      return (EXIT_SUCCESS);

    case 56:
      fprintf (stdout, "HTCHANAKYA (56 Indic encoding - Devanagari)\n");
      return (EXIT_SUCCESS);

    case 57:
      fprintf (stdout, "UTF16BE (big-endian UTF-16)\n");
      return (EXIT_SUCCESS);

    case 58:
      fprintf (stdout, "UTF16LE (little-endian UTF-16)\n");
      return (EXIT_SUCCESS);

    case 59:
      fprintf (stdout, "UTF32BE (big-endian UTF-32, big-endian UCS-4)\n");
      return (EXIT_SUCCESS);

    case 60:
      fprintf (stdout, "UTF32LE (little-endian UTF-32, little-endian UCS-4)\n");
      return (EXIT_SUCCESS);

    case 61:
      fprintf (stdout, "BINARYENC\n");
      return (EXIT_SUCCESS);

    case 62:
      fprintf (stdout, "HZ_GB_2312\n");
      return (EXIT_SUCCESS);

    case 63:
      fprintf (stdout, "UTF8UTF8\n");
      return (EXIT_SUCCESS);

    case 64:
      fprintf (stdout, "TAM_ELANGO (Elango - Tamil)\n");
      return (EXIT_SUCCESS);

    case 65:
      fprintf (stdout, "TAM_LTTMBARANI (Barani - Tamil)\n");
      return (EXIT_SUCCESS);

    case 66:
      fprintf (stdout, "TAM_SHREE (Shree - Tamil)\n");
      return (EXIT_SUCCESS);

    case 67:
      fprintf (stdout, "TAM_TBOOMIS (TBoomis - Tamil)\n");
      return (EXIT_SUCCESS);

    case 68:
      fprintf (stdout, "TAM_TMNEWS (TMNews - Tamil)\n");
      return (EXIT_SUCCESS);

    case 69:
      fprintf (stdout, "TAM_WEBTAMIL (Webtamil - Tamil)\n");
      return (EXIT_SUCCESS);

    case 70:
      fprintf (stdout, "KDDI_SHIFT_JIS\n");
      return (EXIT_SUCCESS);

    case 71:
      fprintf (stdout, "DOCOMO_SHIFT_JIS\n");
      return (EXIT_SUCCESS);

    case 72:
      fprintf (stdout, "SOFTBANK_SHIFT_JIS\n");
      return (EXIT_SUCCESS);

    case 73:
      fprintf (stdout, "KDDI_ISO_2022_JP\n");
      return (EXIT_SUCCESS);

    case 74:
      fprintf (stdout, "SOFTBANK_ISO_2022_JP\n");
      return (EXIT_SUCCESS);

    default:
      fprintf (stderr, "Unknowm encoding type value %i in output.cc enc_name()\n", number);
      return (EXIT_FAILURE);
  }
}
