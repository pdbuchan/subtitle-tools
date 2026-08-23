/*  Copyright (C) 2025 P. David Buchan (pdbuchan@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// main.cc - Detect character encoding of a text file, convert to UTF-8, and add a Byte Order Mark (BOM) if requested.

// Run without command line arguments to see usage notes.
// Output: out.txt

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "compact_enc_det.h"

// Definition of structs
typedef struct {
  size_t len;
  const char *name;
  const uint8_t *sequence;
} BOM;

// Function prototypes
int inputtext (char *, size_t);
int byteordermark (const uint8_t *, size_t, const BOM *);
int run_program (char *const *, int);
int copy_stream (FILE *, FILE *, size_t *);
const char *enc_name (Encoding);
char *allocate_strmem (size_t);

// Set some symbolic constants.
#define MAXLEN 256  // Maximum number of characters accepted from standard input
#define MAXBOM 14  // Number of Byte Order Mark (BOM) signatures recognized

int
main (int argc, char **argv) {

  int type, choice, bytes_consumed, output_fd;
  long file_size;
  size_t nbytes, nread, bytes_written;
  char *temp, *temp2, *text;
  const char *encname;
  bool is_reliable;
  Encoding encoding;
  FILE *fi, *ft, *fo;

  // Byte Order Mark (BOM) names and sequences. UTF-7 has four possible
  // four-byte signatures, so each is represented separately in this table.
  static const uint8_t utf8[]       = {0xef, 0xbb, 0xbf};
  static const uint8_t utf16be[]    = {0xfe, 0xff};
  static const uint8_t utf16le[]    = {0xff, 0xfe};
  static const uint8_t utf32be[]    = {0x00, 0x00, 0xfe, 0xff};
  static const uint8_t utf32le[]    = {0xff, 0xfe, 0x00, 0x00};
  static const uint8_t utf7_1[]     = {0x2b, 0x2f, 0x76, 0x38};
  static const uint8_t utf7_2[]     = {0x2b, 0x2f, 0x76, 0x39};
  static const uint8_t utf7_3[]     = {0x2b, 0x2f, 0x76, 0x2b};
  static const uint8_t utf7_4[]     = {0x2b, 0x2f, 0x76, 0x2f};
  static const uint8_t utf1[]       = {0xf7, 0x64, 0x4c};
  static const uint8_t utfebcdic[]  = {0xdd, 0x73, 0x66, 0x73};
  static const uint8_t scsu[]       = {0x0e, 0xfe, 0xff};
  static const uint8_t bocu1[]      = {0xfb, 0xee, 0x28};
  static const uint8_t gb18030[]    = {0x84, 0x31, 0x95, 0x33};

  static const BOM bom[MAXBOM] = {
    {sizeof (utf8),      "UTF-8",         utf8},
    {sizeof (utf16be),   "UTF-16 (BE)",   utf16be},
    {sizeof (utf16le),   "UTF-16 (LE)",   utf16le},
    {sizeof (utf32be),   "UTF-32 (BE)",   utf32be},
    {sizeof (utf32le),   "UTF-32 (LE)",   utf32le},
    {sizeof (utf7_1),    "UTF-7",         utf7_1},
    {sizeof (utf7_2),    "UTF-7",         utf7_2},
    {sizeof (utf7_3),    "UTF-7",         utf7_3},
    {sizeof (utf7_4),    "UTF-7",         utf7_4},
    {sizeof (utf1),      "UTF-1",         utf1},
    {sizeof (utfebcdic), "UTF-EBCDIC",    utfebcdic},
    {sizeof (scsu),      "SCSU",          scsu},
    {sizeof (bocu1),     "BOCU-1",        bocu1},
    {sizeof (gb18030),   "GB18030",       gb18030}
  };

  // Process the command line arguments, if any.
  if (argc != 2) {
    fprintf (stdout, "\nUsage: ./enc inputfilename\n");
    fprintf (stdout, "       Output will be out.txt.\n\n");
    return (EXIT_SUCCESS);
  }

  fprintf (stdout, "\nInput file: %s\n\n", argv[1]);

  // Open input file.
  fi = fopen (argv[1], "rb");
  if (fi == NULL) {
    fprintf (stderr, "ERROR: Unable to open input file %s.\n", argv[1]);
    return (EXIT_FAILURE);
  }

  // Determine the size of the input file. CompactEncDet::DetectEncoding()
  // accepts the input length as an int, so files larger than INT_MAX cannot
  // be passed to it safely.
  if (fseek (fi, 0L, SEEK_END) != 0) {
    fprintf (stderr, "ERROR: Unable to seek to the end of input file %s.\n", argv[1]);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  file_size = ftell (fi);
  if (file_size < 0) {
    fprintf (stderr, "ERROR: Unable to determine the size of input file %s.\n", argv[1]);
    fclose (fi);
    return (EXIT_FAILURE);
  }
  if (file_size > INT_MAX) {
    fprintf (stderr, "ERROR: Input file %s is too large for CED.\n", argv[1]);
    fclose (fi);
    return (EXIT_FAILURE);
  }

  nbytes = (size_t) file_size;
  rewind (fi);
  fprintf (stdout, "%zu bytes in input file.\n", nbytes);

  // Allocate one extra zero-filled byte. CED uses the explicit byte count,
  // but the extra byte makes the buffer safe to inspect as a C string while
  // debugging and also permits a zero-length input file.
  text = allocate_strmem (nbytes + 1);

  // Read the complete file, including any embedded NUL bytes.
  nread = fread (text, 1, nbytes, fi);
  if (nread != nbytes) {
    fprintf (stderr, "ERROR: Unable to read complete input file %s.\n", argv[1]);
    fclose (fi);
    free (text);
    return (EXIT_FAILURE);
  }

  // Close input file.
  fclose (fi);

  // Detect any known Byte Order Mark (BOM) at the start of the file. The
  // detector chooses the longest matching signature because UTF-16LE's
  // FF FE signature is a prefix of UTF-32LE's FF FE 00 00 signature.
  type = byteordermark ((const uint8_t *) text, nbytes, bom);
  if (type < 0) {
    fprintf (stdout, "\nNo known existing Byte Order Mark (BOM) found in %s.\n", argv[1]);
  } else {
    fprintf (stdout, "\nExisting Byte Order Mark (BOM) detected for character encoding type: %s\n", bom[type].name);
    fprintf (stdout, "No action taken.\n\n");
    free (text);
    return (EXIT_SUCCESS);
  }

  // Allocate buffers used for interactive input.
  temp = allocate_strmem (MAXLEN);
  temp2 = allocate_strmem (MAXLEN);

  // Ask if Byte Order Mark (BOM) should be prepended to output file.
  fprintf (stdout, "\nDo you want a UTF-8 Byte Order Mark (BOM) prepended to output file out.txt (y/n)? ");
  choice = 0;  // Default to no.
  do {
    memset (temp, 0, MAXLEN * sizeof (char));
    if (inputtext (temp, MAXLEN) != EXIT_SUCCESS) {
      fprintf (stderr, "\nERROR: Unable to read response from standard input.\n");
      free (temp);
      free (temp2);
      free (text);
      return (EXIT_FAILURE);
    }
  } while (((temp[0] != 'y') && (temp[0] != 'Y') && (temp[0] != 'n') && (temp[0] != 'N')) || (strnlen (temp, MAXLEN) != 1));
  if ((temp[0] == 'y') || (temp[0] == 'Y')) {
    choice = 1;
  }

  // Detect probable current encoding of input file. Pass the actual byte
  // count instead of strlen(), since CED is designed to examine arbitrary
  // raw bytes and the input may contain embedded NUL bytes.
  encoding = CompactEncDet::DetectEncoding (text, (int) nbytes, nullptr, nullptr, nullptr,
    UNKNOWN_ENCODING, UNKNOWN_LANGUAGE, CompactEncDet::QUERY_CORPUS, true,
    &bytes_consumed, &is_reliable);

  // Obtain name of probable current encoding.
  encname = enc_name (encoding);
  if (encname == nullptr) {
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }

  // Present CED's results.
  fprintf (stdout, "\nCED:\nProbable encoding: %s\n", encname);
  fprintf (stdout, "bytes consumed: %i\n", bytes_consumed);
  fprintf (stdout, "Is reliable?: %s\n", is_reliable ? "true" : "false");

  // Obtain and present chardet's results. Execute chardet directly rather
  // than building a shell command, so spaces and shell metacharacters in the
  // filename cannot change the command being executed.
  fprintf (stdout, "\nchardet:\n");
  fflush (stdout);
  {
    char command[] = "chardet";
    char *command_argv[] = {command, argv[1], nullptr};
    if (run_program (command_argv, -1) != EXIT_SUCCESS) {
      fprintf (stderr, "WARNING: chardet did not complete successfully.\n");
    }
  }

  // Ask for user to select their choice for most probable encoding.
  fprintf (stdout, "\nChoose most likely encoding: ");
  memset (temp2, 0, MAXLEN * sizeof (char));
  if (inputtext (temp2, MAXLEN) != EXIT_SUCCESS) {
    fprintf (stderr, "\nERROR: Unable to read encoding from standard input.\n");
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }
  if (temp2[0] == '\0') {
    fprintf (stderr, "ERROR: No input encoding was specified.\n");
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }
  fprintf (stdout, "\n");

  // Convert from the selected current encoding to UTF-8. tmpfile() provides
  // a private temporary file and avoids races or accidental overwriting of a
  // fixed filename such as out.tmp. iconv's stdout is redirected to it.
  ft = tmpfile ();
  if (ft == NULL) {
    fprintf (stderr, "ERROR: Unable to create temporary file: %s.\n", strerror (errno));
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }

  fflush (ft);
  {
    char command[] = "iconv";
    char from[] = "-f";
    char to[] = "-t";
    char utf8_name[] = "UTF-8";
    char *command_argv[] = {command, from, temp2, to, utf8_name, argv[1], nullptr};
    if (run_program (command_argv, fileno (ft)) != EXIT_SUCCESS) {
      fprintf (stderr, "ERROR: iconv was unable to convert input file %s using encoding %s.\n", argv[1], temp2);
      fclose (ft);
      free (temp);
      free (temp2);
      free (text);
      return (EXIT_FAILURE);
    }
  }

  if (fseek (ft, 0L, SEEK_SET) != 0) {
    fprintf (stderr, "ERROR: Unable to rewind temporary conversion file.\n");
    fclose (ft);
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }

  // Create out.txt without overwriting an existing file. O_EXCL also avoids
  // the check-then-open race that occurs when existence is tested separately.
  output_fd = open ("out.txt", O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (output_fd < 0) {
    if (errno == EEXIST) {
      fprintf (stderr, "Output file out.txt already exists.\n");
    } else {
      fprintf (stderr, "ERROR: Can't create output file out.txt: %s.\n", strerror (errno));
    }
    fclose (ft);
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }

  fo = fdopen (output_fd, "wb");
  if (fo == NULL) {
    fprintf (stderr, "ERROR: Can't associate a stream with output file out.txt: %s.\n", strerror (errno));
    close (output_fd);
    remove ("out.txt");
    fclose (ft);
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }

  // Write UTF-8 BOM to output file, if requested.
  if (choice > 0) {
    if (fwrite (utf8, 1, sizeof (utf8), fo) != sizeof (utf8)) {
      fprintf (stderr, "ERROR: Unable to write UTF-8 Byte Order Mark to out.txt.\n");
      fclose (fo);
      remove ("out.txt");
      fclose (ft);
      free (temp);
      free (temp2);
      free (text);
      return (EXIT_FAILURE);
    }
    fprintf (stdout, "UTF-8 Byte Order Mark (BOM) prepended to output file out.txt.\n\n");
  } else {
    fprintf (stdout, "No Byte Order Mark (BOM) prepended to output file out.txt.\n\n");
  }

  // Append converted UTF-8 data from the temporary file to out.txt.
  if (copy_stream (ft, fo, &bytes_written) != EXIT_SUCCESS) {
    fprintf (stderr, "ERROR: Unable to copy converted data to out.txt.\n");
    fclose (fo);
    remove ("out.txt");
    fclose (ft);
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }
  fprintf (stdout, "%zu bytes of converted UTF-8 data written to out.txt.\n", bytes_written);

  // Close files.
  if (fclose (fo) != 0) {
    fprintf (stderr, "ERROR: Unable to close output file out.txt successfully.\n");
    remove ("out.txt");
    fclose (ft);
    free (temp);
    free (temp2);
    free (text);
    return (EXIT_FAILURE);
  }
  fclose (ft);

  // Free allocated memory.
  free (temp);
  free (temp2);
  free (text);

  return (EXIT_SUCCESS);
}

// Obtain a text string from standard input. It can include spaces.
int
inputtext (char *text, size_t len) {

  size_t n;
  int c;

  if ((text == nullptr) || (len < 2) || (len > INT_MAX)) {
    return (EXIT_FAILURE);
  }

  // Request new text from standard input.
  if (fgets (text, (int) len, stdin) == nullptr) {
    return (EXIT_FAILURE);
  }

  n = strnlen (text, len);

  // Remove trailing newline, if there. If the input line was longer than the
  // supplied buffer, discard its remainder so it does not become the answer
  // to the next prompt.
  if ((n > 0) && (text[n - 1] == '\n')) {
    text[n - 1] = '\0';
  } else {
    do {
      c = fgetc (stdin);
    } while ((c != '\n') && (c != EOF));
  }

  return (EXIT_SUCCESS);
}

// Detect a Byte Order Mark (BOM), if one exists at the beginning of the file.
// Return the index of the longest matching bom[] entry, or -1 if no listed
// signature is present. Choosing the longest match is important because the
// UTF-16LE signature is a prefix of the UTF-32LE signature.
int
byteordermark (const uint8_t *text, size_t nbytes, const BOM *bom) {

  int type, found_type;
  size_t longest;

  found_type = -1;
  longest = 0;

  // Loop through all recognized Byte Order Mark signatures.
  for (type = 0; type < MAXBOM; type++) {
    if ((bom[type].len <= nbytes) && (bom[type].len > longest) &&
      (memcmp (text, bom[type].sequence, bom[type].len) == 0)) {
      found_type = type;
      longest = bom[type].len;
    }
  }

  return (found_type);
}

// Execute a program directly without invoking a command shell. If output_fd
// is non-negative, redirect the child's standard output to that descriptor.
int
run_program (char *const *program_argv, int output_fd) {

  int status;
  pid_t pid, result;

  pid = fork ();
  if (pid < 0) {
    fprintf (stderr, "ERROR: fork() failed while starting %s: %s.\n", program_argv[0], strerror (errno));
    return (EXIT_FAILURE);
  }

  if (pid == 0) {
    if ((output_fd >= 0) && (dup2 (output_fd, STDOUT_FILENO) < 0)) {
      fprintf (stderr, "ERROR: Unable to redirect output for %s: %s.\n", program_argv[0], strerror (errno));
      _exit (EXIT_FAILURE);
    }

    execvp (program_argv[0], program_argv);
    fprintf (stderr, "ERROR: Unable to execute %s: %s.\n", program_argv[0], strerror (errno));
    _exit (EXIT_FAILURE);
  }

  do {
    result = waitpid (pid, &status, 0);
  } while ((result < 0) && (errno == EINTR));

  if (result < 0) {
    fprintf (stderr, "ERROR: waitpid() failed for %s: %s.\n", program_argv[0], strerror (errno));
    return (EXIT_FAILURE);
  }

  if (!WIFEXITED (status) || (WEXITSTATUS (status) != EXIT_SUCCESS)) {
    return (EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}

// Copy all bytes from one stream to another.
int
copy_stream (FILE *fi, FILE *fo, size_t *bytes_written) {

  unsigned char buffer[8192];
  size_t nread, total;

  total = 0;
  while ((nread = fread (buffer, 1, sizeof (buffer), fi)) > 0) {
    if (fwrite (buffer, 1, nread, fo) != nread) {
      return (EXIT_FAILURE);
    }
    total += nread;
  }

  if (ferror (fi)) {
    return (EXIT_FAILURE);
  }

  *bytes_written = total;

  return (EXIT_SUCCESS);
}

// Allocate memory for an array of chars.
char *
allocate_strmem (size_t len) {

  char *tmp;

  if (len == 0) {
    fprintf (stderr, "ERROR: Cannot allocate zero bytes in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }

  tmp = (char *) calloc (len, sizeof (char));
  if (tmp == nullptr) {
    fprintf (stderr, "ERROR: Cannot allocate memory for array in allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }

  return (tmp);
}
