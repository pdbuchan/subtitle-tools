CC = gcc
CCFLAGS =
LIBS = -lm
DEBUG = -Wall -Wextra -Wpedantic -g

OBJS = apply_format_tag.o byteordermark.o convert_file.o convert_text.o frame_to_timestamp.o get_frame_rate.o main.o \
       parse_default_line.o parse_frame_rate.o parse_subtitle_line.o readline.o scan_defaults.o

all: microdvd2srt

microdvd2srt: $(OBJS)
	$(CC) $(DEBUG) -o microdvd2srt $(OBJS) $(LIBS)

%.o: %.c microdvd2srt.h
	$(CC) $(DEBUG) $(CCFLAGS) -c $<

# Target for cleaning up
clean:
	rm -f *.o *.gch microdvd2srt

# Target for printing out flag settings
flags:
	@echo "CCFLAGS=$(CCFLAGS)"

.PHONY : all clean flags
