CC = gcc
CCFLAGS =
LIBS = -lm
DEBUG = -Wall -Wextra -Wpedantic -g

OBJS = append_line.o convert_cue_text.o convert_file.o convert_timestamp.o convert_timing_line.o free_block.o \
       is_webvtt_header.o main.o process_block.o readline.o starts_keyword.o

all: webvtt2srt

webvtt2srt: $(OBJS)
	$(CC) $(DEBUG) -o webvtt2srt $(OBJS) $(LIBS)

%.o: %.c webvtt2srt.h
	$(CC) $(DEBUG) $(CCFLAGS) -c $<

# Target for cleaning up
clean:
	rm -f *.o *.gch webvtt2srt

# Target for printing out flag settings
flags:
	@echo "CCFLAGS=$(CCFLAGS)"

.PHONY : all clean flags
