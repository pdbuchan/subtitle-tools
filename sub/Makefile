CC = gcc
CCFLAGS =
LIBS = 
DEBUG = -Wall -g

all: sub

sub: allocate.o decode_rle.o extract_subs.o get_16bits.o inputtext.o main.o parse_idx.o parse_packets.o parse_spu.o parse_timestamp.o \
     readline.o record_pes_timestamp_change.o time_utils.o unpack_pxd.o write_bmp.o write_idx_file.o write_utils.o

	$(CC) $(DEBUG) -o sub allocate.o decode_rle.o extract_subs.o get_16bits.o inputtext.o main.o parse_idx.o parse_packets.o parse_spu.o \
        parse_timestamp.o  readline.o record_pes_timestamp_change.o time_utils.o unpack_pxd.o write_bmp.o write_idx_file.o write_utils.o $(LIBS)

allocate.o: allocate.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c allocate.c

decode_rle.o: decode_rle.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c decode_rle.c

extract_subs.o: extract_subs.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c extract_subs.c

get_16bits.o: get_16bits.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c get_16bits.c

inputtext.o: inputtext.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c inputtext.c

main.o: main.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c main.c

parse_idx.o: parse_idx.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_idx.c

parse_packets.o: parse_packets.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_packets.c

parse_spu.o: parse_spu.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_spu.c

parse_timestamp.o: parse_timestamp.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_timestamp.c

readline.o: readline.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c readline.c

record_pes_timestamp_change.o: record_pes_timestamp_change.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c record_pes_timestamp_change.c

time_utils.o: time_utils.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c time_utils.c

unpack_pxd.o: unpack_pxd.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c unpack_pxd.c

write_bmp.o: write_bmp.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c write_bmp.c

write_idx_file.o: write_idx_file.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c write_idx_file.c

write_utils.o: write_utils.c sub.h
	$(CC) $(DEBUG) $(CCFLAGS) -c write_utils.c

# Target for cleaning up
clean:
	rm -f *.o *.gch sub

# Target for printing out flag settings
flags:
	@echo "CCFLAGS=$(CCFLAGS)"

# Declare 'all', 'clean', and 'flags' as phonies since they are
# not files and we want them to be 'built' (executed) every time.
# i.e., protect against situation in which a file in the directory
# has the same name.
.PHONY : all clean flags
