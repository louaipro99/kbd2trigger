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
             -I$(PSL1GHT)/ppu/include \
             -I$(PSL1GHT)/ppu/include/cell

LDFLAGS  := -L$(PSL1GHT)/ppu/lib \
             -nostartfiles \
             -T$(PS3DEV)/ppu/ppu/lib/lv2.ld \
             -Wl,--gc-sections \
             -Wl,--undefined=module_start \
             -Wl,--undefined=module_stop

LIBS     := -lio -llv2 -lpad -lkb

all: $(TARGET).sprx

$(TARGET).sprx: $(TARGET).elf
	cp $< $(TARGET).prx
	$(SPRXLINK) $(TARGET).prx
	$(FSELF) $(TARGET).prx $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

source/main.o: source/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f source/main.o $(TARGET).elf $(TARGET).prx $(TARGET).sprx
