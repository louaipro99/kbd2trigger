PS3DEV   ?= $(HOME)/ps3dev
PSL1GHT  ?= $(HOME)/ps3dev

PREFIX   := ppu-
CC       := $(PS3DEV)/ppu/bin/$(PREFIX)gcc
SPRXLINK := sprxlinker

TARGET   := kbd2trigger
SRCS     := source/main.c
OBJS     := $(SRCS:.c=.o)

MACHDEP  := -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections

CFLAGS   := -O2 -Wall $(MACHDEP) \
             -I$(PSL1GHT)/ppu/include \
             -I$(PSL1GHT)/ppu/include/psl1ght

LDFLAGS  := -L$(PSL1GHT)/ppu/lib \
             -nostartfiles \
             -Wl,-Map,$(TARGET).map \
             -Wl,--gc-sections

LIBS     := -llv2

all: $(TARGET).sprx

$(TARGET).sprx: $(TARGET).elf
	$(SPRXLINK) $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

source/main.o: source/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f source/main.o $(TARGET).elf $(TARGET).sprx $(TARGET).map
