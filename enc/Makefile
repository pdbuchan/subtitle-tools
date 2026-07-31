CC      = gcc
CCFLAGS = 
LIBS    = -lstdc++ -lm
DEBUG   = -Wall -g -Wuninitialized -Wunused -Wextra -Wswitch-default -Wswitch-enum \
          -Wstrict-aliasing=3 -Wfloat-equal

all: enc

enc: main.o compact_enc_det.o compact_enc_det_hint_code.o encodings.o languages.o output.o

	$(CC) $(DEBUG) main.o compact_enc_det.o compact_enc_det_hint_code.o encodings.o languages.o output.o -o enc $(LIBS)

main.o: main.cc
	$(CC) $(DEBUG) $(CCFLAGS) -c main.cc

compact_enc_det.o: compact_enc_det.cc compact_enc_det.h compact_enc_det_hint_code.h string_util.h basictypes.h \
                   commandlineflags.h logging.h compact_enc_det_generated_tables.h
	$(CC) $(DEBUG) $(CCFLAGS) -c compact_enc_det.cc

compact_enc_det_hint_code.o: compact_enc_det_hint_code.cc compact_enc_det_hint_code.h basictypes.h string_util.h
	$(CC) $(DEBUG) $(CCFLAGS) -c compact_enc_det_hint_code.cc

encodings.o: encodings.cc encodings.h basictypes.h string_util.h case_insensitive_hash.h
	$(CC) $(DEBUG) $(CCFLAGS) -c encodings.cc

languages.o: languages.cc languages.h basictypes.h string_util.h
	$(CC) $(DEBUG) $(CCFLAGS) -c languages.cc

output.o: output.cc
	$(CC) $(DEBUG) $(CCFLAGS) -c output.cc

# Target for cleaning up
clean:
	rm -f *.o enc

# Declare 'all' and 'clean' as phonies since they are
# not files.
# i.e., protect against situation in which a file in the directory
# has the same name.
.PHONY : all clean
