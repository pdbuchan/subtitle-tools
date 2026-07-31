CC = gcc
CFLAGS ?= -Wall

PLAIN_TOOLS := check offset sync txtfiles2srt ssa2srt ssa2srt-nostyles reorder srt2txt txt2srt tag fixtag striptag long ellipsis time-text combine split readbom writebom stripbom time-diff time-add chapters
MATH_TOOLS := bt709 ycbcr2rgb rgb2ycbcr
PROJECTS := sub pgs dvb ced enc

PLAIN_BINARIES := $(foreach tool,$(PLAIN_TOOLS),$(tool)/$(tool))
MATH_BINARIES := $(foreach tool,$(MATH_TOOLS),$(tool)/$(tool))

all: standalone projects

standalone: $(PLAIN_BINARIES) $(MATH_BINARIES)

define PLAIN_TOOL_template
$(1)/$(1): $(1)/$(1).c
	$$(CC) $$(CFLAGS) $$< -o $$@
endef
$(foreach tool,$(PLAIN_TOOLS),$(eval $(call PLAIN_TOOL_template,$(tool))))

define MATH_TOOL_template
$(1)/$(1): $(1)/$(1).c
	$$(CC) $$(CFLAGS) $$< -lm -o $$@
endef
$(foreach tool,$(MATH_TOOLS),$(eval $(call MATH_TOOL_template,$(tool))))

projects:
	@set -e; for dir in $(PROJECTS); do $(MAKE) -C $$dir; done

clean:
	rm -f $(PLAIN_BINARIES) $(MATH_BINARIES)
	@for dir in $(PROJECTS); do $(MAKE) -C $$dir clean; done

.PHONY: all standalone projects clean
