#include <stdio.h>
#include <stdlib.h>

// Print to stdout encoding name when passed encoding value.
const char *
enc_name (int number) {

  switch (number) {
    case 0:
      return ("ISO_8859-1");

    case 1:
      return ("ISO_8859-2");

    case 2:
      return ("ISO_8859-3");

    case 3:
      return ("ISO_8859-4");

    case 4:
      return ("ISO_8859-5");

    case 5:
      return ("ISO_8859-6");

    case 6:
      return ("ISO_8859-7");

    case 7:
      return ("ISO_8859-8");

    case 8:
      return ("ISO_8859-9");

    case 9:
      return ("ISO_8859-10");

    case 10:
      return ("EUC-JP");

    case 11:
      return ("SHIFT-JIS");

    case 12:
      return ("JAPANESE_JIS");

    case 13:
      return ("BIG-5");

    case 14:
      return ("GB2312");

    case 15:
      return ("EUC-TW");

    case 16:
      return ("EUC-KR");

    case 17:
      return ("ISO-10646");

    case 18:
      return ("EUC-TW");

    case 19:
      return ("EUC_TW");

    case 20:
      return ("CHINESE_BIG5_CP950");

    case 21:
      return ("JAPANESE_CP932");

    case 22:
      return ("UTF8");

    case 23:
      return ("UNKNOWN_ENCODING");

    case 24:
      return ("ASCII_7BIT");

    case 25:
      return ("KOI8-R");

    case 26:
      return ("WINDOWS-1251");

    case 27:
      return ("WINDOWS-1252");

    case 28:
      return ("RUSSIAN_KOI8_RU");

    case 29:
      return ("CP1250");

    case 30:
      return ("ISO_8859-15");

    case 31:
      return ("CP1254");

    case 32:
      return ("CP1257");

    case 33:
      return ("ISO_8859_11");

    case 34:
      return ("MSFT_CP874");

    case 35:
      return ("MSFT_CP1256");

    case 36:
      return ("MSFT_CP1255");

    case 37:
      return ("ISO_8859_8_I");

    case 38:
      return ("HEBREW_VISUAL");

    case 39:
      return ("CZECH_CP852");

    case 40:
      return ("CZECH_CSN_369103");

    case 41:
      return ("MSFT_CP1253");

    case 42:
      return ("RUSSIAN_CP866");

    case 43:
      return ("ISO_8859_13");

    case 44:
      return ("ISO_2022_KR");

    case 45:
      return ("GBK");

    case 46:
      return ("GB18030");

    case 47:
      return ("BIG5_HKSCS");

    case 48:
      return ("ISO_2022_CN");

    case 49:
      return ("TSCII");

    case 50:
      return ("TAMIL_MONO");

    case 51:
      return ("TAMIL_BI");

    case 52:
      return ("JAGRAN");

    case 53:
      return ("MACINTOSH_ROMAN");

    case 54:
      return ("UTF7");

    case 55:
      return ("BHASKAR");

    case 56:
      return ("HTCHANAKYA");

    case 57:
      return ("UTF16BE");

    case 58:
      return ("UTF16LE");

    case 59:
      return ("UTF32BE");

    case 60:
      return ("UTF32LE");

    case 61:
      return ("BINARYENC");

    case 62:
      return ("HZ_GB_2312");

    case 63:
      return ("UTF8UTF8");

    case 64:
      return ("TAM_ELANGO");

    case 65:
      return ("TAM_LTTMBARANI");

    case 66:
      return ("TAM_SHREE");

    case 67:
      return ("TAM_TBOOMIS");

    case 68:
      return ("TAM_TMNEWS");

    case 69:
      return ("TAM_WEBTAMIL");

    case 70:
      return ("KDDI_SHIFT_JIS");

    case 71:
      return ("DOCOMO_SHIFT_JIS");

    case 72:
      return ("SOFTBANK_SHIFT_JIS");

    case 73:
      return ("KDDI_ISO_2022_JP");

    case 74:
      return ("SOFTBANK_ISO_2022_JP");

    default:
      fprintf (stderr, "Unknowm encoding type value %i in output.cc enc_name()", number);
      exit (EXIT_FAILURE);
  }
}
