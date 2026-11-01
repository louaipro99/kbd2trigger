PS3DEV   ?= /usr/local/ps3dev
PSL1GHT  ?= $(PS3DEV)

CC       := $(PS3DEV)/ppu/bin/ppu-gcc
LD       := $(PS3DEV)/ppu/bin/ppu-ld
SPRXLD   := $(PS3DEV)/bin/fself.py
ODIR     := build

CFLAGS   := -O2 -Wall \
             -I$(PSL1GHT)/ppu/include \
             -I$(PSL1GHT)/ppu/include/psl1ght \
             -mabi=altivec \
             -maltivec \
             --sysroot=$(PS3DEV)/ppu

LDFLAGS  := -L$(PSL1GHT)/ppu/lib \
             -lpad -lio -llv2 \
             -Wl,-Map,$(TARGET).map

TARGET   := kbd2trigger
SRCS     := source/main.c
OBJS     := $(SRCS:.c=.o)

all: $(TARGET).sprx

$(TARGET).sprx: $(TARGET).elf
	$(PS3DEV)/bin/sprxlinker $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).sprx $(TARGET).map
