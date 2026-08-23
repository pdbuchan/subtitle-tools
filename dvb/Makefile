CC = gcc
CCFLAGS =
LIBS = -lm
DEBUG = -Wall -g

all: dvb

dvb: allocate.o assemble_composition.o build_pes_segment.o build_psi_section.o clear_page.o clut_utils.o crc32.o data_ids.o \
     default_2clut.o default_4clut.o default_8clut.o disparity_shift_update_sequence.o emit_pixels.o finalize_page_if_needed.o \
     get_8bits.o initialize_clut_family.o main.o parse_adapt_field.o parse_cds.o parse_dds.o parse_dss.o \
     parse_eight_bit_code_string.o parse_end.o parse_four_bit_code_string.o parse_ods.o parse_pat.o parse_pcs.o parse_pes_header.o \
     parse_pes_segment.o parse_pmt.o parse_psi_section.o parse_rcs.o parse_sdt.o parse_ts_packet.o parse_two_bit_code_string.o \
     search_functions.o segment_types.o stream_ids.o stream_types.o time.o write_bmp.o write_utils.o YCbCr2RGB_bt601.o

	$(CC) $(DEBUG) -o dvb allocate.o assemble_composition.o build_pes_segment.o build_psi_section.o clear_page.o clut_utils.o crc32.o data_ids.o \
     default_2clut.o default_4clut.o default_8clut.o disparity_shift_update_sequence.o emit_pixels.o finalize_page_if_needed.o \
     get_8bits.o initialize_clut_family.o main.o parse_adapt_field.o parse_cds.o parse_dds.o parse_dss.o \
     parse_eight_bit_code_string.o parse_end.o parse_four_bit_code_string.o parse_ods.o parse_pat.o parse_pcs.o parse_pes_header.o \
     parse_pes_segment.o parse_pmt.o parse_psi_section.o parse_rcs.o parse_sdt.o parse_ts_packet.o parse_two_bit_code_string.o \
     search_functions.o segment_types.o stream_ids.o stream_types.o time.o write_bmp.o write_utils.o YCbCr2RGB_bt601.o $(LIBS)

allocate.o: allocate.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c allocate.c

assemble_composition.o: assemble_composition.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c assemble_composition.c

build_pes_segment.o: build_pes_segment.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c build_pes_segment.c

build_psi_section.o: build_psi_section.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c build_psi_section.c

clear_page.o: clear_page.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c clear_page.c

clut_utils.o: clut_utils.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c clut_utils.c

crc32.o: crc32.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c crc32.c

data_ids.o: data_ids.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c data_ids.c

default_2clut.o: default_2clut.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c default_2clut.c

default_4clut.o: default_4clut.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c default_4clut.c

default_8clut.o: default_8clut.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c default_8clut.c

disparity_shift_update_sequence.o: disparity_shift_update_sequence.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c disparity_shift_update_sequence.c

emit_pixels.o: emit_pixels.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c emit_pixels.c

finalize_page_if_needed.o: finalize_page_if_needed.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c finalize_page_if_needed.c

get_8bits.o: get_8bits.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c get_8bits.c

initialize_clut_family.o: initialize_clut_family.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c initialize_clut_family.c

main.o: main.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c main.c

parse_adapt_field.o: parse_adapt_field.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_adapt_field.c

parse_cds.o: parse_cds.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_cds.c

parse_dds.o: parse_dds.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_dds.c

parse_dss.o: parse_dss.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_dss.c

parse_eight_bit_code_string.o: parse_eight_bit_code_string.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_eight_bit_code_string.c

parse_end.o: parse_end.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_end.c

parse_four_bit_code_string.o: parse_four_bit_code_string.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_four_bit_code_string.c

parse_ods.o: parse_ods.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_ods.c

parse_pat.o: parse_pat.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_pat.c

parse_pcs.o: parse_pcs.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_pcs.c

parse_pes_header.o: parse_pes_header.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_pes_header.c

parse_pes_segment.o: parse_pes_segment.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_pes_segment.c

parse_pmt.o: parse_pmt.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_pmt.c

parse_psi_section.o: parse_psi_section.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_psi_section.c

parse_rcs.o: parse_rcs.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_rcs.c

parse_sdt.o: parse_sdt.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_sdt.c

parse_ts_packet.o: parse_ts_packet.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_ts_packet.c

parse_two_bit_code_string.o: parse_two_bit_code_string.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c parse_two_bit_code_string.c

search_functions.o: search_functions.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c search_functions.c

segment_types.o: segment_types.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c segment_types.c

stream_ids.o: stream_ids.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c stream_ids.c

stream_types.o: stream_types.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c stream_types.c

time.o: time.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c time.c

write_bmp.o: write_bmp.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c write_bmp.c

write_utils.o: write_utils.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c write_utils.c

YCbCr2RGB_bt601.o: YCbCr2RGB_bt601.c dvb.h
	$(CC) $(DEBUG) $(CCFLAGS) -c YCbCr2RGB_bt601.c

# Target for cleaning up
clean:
	rm -f *.o *.gch dvb

# Target for printing out flag settings
flags:
	@echo "CCFLAGS=$(CCFLAGS)"

# Declare 'all', 'clean', and 'flags' as phonies since they are
# not files and we want them to be 'built' (executed) every time.
# i.e., protect against situation in which a file in the directory
# has the same name.
.PHONY : all clean flags
