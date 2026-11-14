PS3DEV   ?= $(HOME)/ps3dev
PSL1GHT  ?= $(HOME)/ps3dev

CC       := $(PS3DEV)/ppu/bin/ppu-gcc
SPRXLINK := sprxlinker
FSELF    := fself

TARGET   := kbd2trigger
SRCS     := source/main.c
OBJS     := $(SRCS:.c=.o)

CFLAGS   := -O2 -Wall \
             -mhard-float -ffunction-sections -fdata-sections \
             -I$(PSL1GHT)/ppu/include

LDFLAGS  := -L$(PSL1GHT)/ppu/lib \
             -nostartfiles \
             -T$(PS3DEV)/ppu/ppu/lib/lv2.ld \
             -Wl,--gc-sections \
             -Wl,--undefined=module_start \
             -Wl,--undefined=module_stop

LIBS     := -lio -llv2

all: $(TARGET).sprx

# Correct pipeline:
# 1. Compile + link to ELF
# 2. fself wraps ELF as fake SELF -> .sprx
# 3. sprxlinker patches PRX sections in the SELF in-place
$(TARGET).sprx: $(TARGET).elf
	$(FSELF) $< $@
	$(SPRXLINK) $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

source/main.o: source/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f source/main.o $(TARGET).elf $(TARGET).sprx
