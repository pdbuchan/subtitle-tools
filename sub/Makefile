CC = gcc
CFLAGS ?= -O2
CFLAGS += -std=c17 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS = -lm

OBJECTS = allocate.o decode_rle.o extract_subs.o inputtext.o main.o parse_idx.o \
          parse_packets.o parse_spu.o parse_timestamp.o readline.o \
          record_pes_timestamp_change.o time_utils.o unpack_pxd.o write_bmp.o \
          write_idx_file.o write_utils.o

all: sub

sub: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

%.o: %.c sub.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.gch sub

flags:
	@echo "CFLAGS=$(CFLAGS)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "LDLIBS=$(LDLIBS)"

.PHONY: all clean flags
