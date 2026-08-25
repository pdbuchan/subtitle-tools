CC = gcc
CCFLAGS =
DEBUG = -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -g

OBJS = allocate.o build_pes_segment.o build_psi_section.o crc32.o data_ids.o \
       main.o parse_adapt_field.o parse_pat.o parse_pes_header.o \
       parse_pes_segment.o parse_pmt.o parse_psi_section.o parse_sdt.o \
       parse_teletext.o parse_ts_packet.o search_functions.o stream_ids.o \
       stream_types.o time.o write_teletext.o

all: teletext

teletext: $(OBJS)
	$(CC) $(DEBUG) -o teletext $(OBJS)

%.o: %.c teletext.h
	$(CC) $(DEBUG) $(CCFLAGS) -c $<

clean:
	rm -f *.o *.gch teletext

flags:
	@echo "CCFLAGS=$(CCFLAGS)"

.PHONY: all clean flags
