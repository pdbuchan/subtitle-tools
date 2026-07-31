CC = gcc
CCFLAGS =
LIBS = -lm
DEBUG = -Wall -g

all: pgs

pgs: allocate.o clear_objects.o clear_palettes.o decode_rle.o framerates.o inputtext.o inttofourbytes.o main.o parse_header.o parse_ods.o parse_pcs.o \
     parse_pds.o parse_timestamp.o parse_wds.o record_u32_change.o time_utils.o write_bmp.o write_utils.o YCbCr2RGB_bt709.o

	$(CC) $(DEBUG) -o pgs allocate.o clear_objects.o clear_palettes.o decode_rle.o framerates.o inputtext.o inttofourbytes.o main.o parse_header.o parse_ods.o \
        parse_pcs.o parse_pds.o parse_timestamp.o parse_wds.o record_u32_change.o time_utils.o write_bmp.o write_utils.o YCbCr2RGB_bt709.o $(LIBS)

allocate.o: allocate.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c allocate.c

clear_objects.o: clear_objects.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c clear_objects.c

clear_palettes.o: clear_palettes.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c clear_palettes.c

decode_rle.o: decode_rle.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c decode_rle.c

framerates.o: framerates.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c framerates.c

inputtext.o: inputtext.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c inputtext.c

inttofourbytes.o: inttofourbytes.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c inttofourbytes.c

main.o: main.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c main.c

parse_header.o: parse_header.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_header.c

parse_ods.o: parse_ods.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_ods.c

parse_pcs.o: parse_pcs.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_pcs.c

parse_pds.o: parse_pds.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_pds.c

parse_timestamp.o: parse_timestamp.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_timestamp.c

parse_wds.o: parse_wds.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_wds.c

record_u32_change.o: record_u32_change.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c record_u32_change.c

time_utils.o: time_utils.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c time_utils.c

write_bmp.o: write_bmp.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c write_bmp.c

write_utils.o: write_utils.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c write_utils.c

YCbCr2RGB_bt709.o: YCbCr2RGB_bt709.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c YCbCr2RGB_bt709.c

# Target for cleaning up
clean:
	rm -f *.o *.gch pgs

# Target for printing out flag settings
flags:
	@echo "CCFLAGS=$(CCFLAGS)"

# Declare 'all', 'clean', and 'flags' as phonies since they are
# not files and we want them to be 'built' (executed) every time.
# i.e., protect against situation in which a file in the directory
# has the same name.
.PHONY : all clean flags
