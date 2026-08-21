CC = gcc
CCFLAGS =
LIBS = -lm
DEBUG = -Wall -Wextra -Wpedantic -g

OBJS = adjust_timestamps.o allocate.o clear_objects.o clear_palettes.o decode_rle.o framerates.o inputtext.o main.o \
       parse_header.o parse_ods.o parse_pcs.o parse_pds.o parse_timestamp.o parse_wds.o record_u32_change.o \
       render_subtitle.o time_utils.o write_bmp.o write_utils.o YCbCr2RGB_bt709.o

all: pgs

pgs: $(OBJS)
	$(CC) $(DEBUG) -o pgs $(OBJS) $(LIBS)

%.o: %.c pgs.h
	$(CC) $(DEBUG) $(CCFLAGS) -c $<

# Target for cleaning up
clean:
	rm -f *.o *.gch pgs

# Target for printing out flag settings
flags:
	@echo "CCFLAGS=$(CCFLAGS)"

.PHONY : all clean flags
