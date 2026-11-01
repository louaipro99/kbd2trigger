PS3DEV   ?= $(HOME)/ps3dev
PSL1GHT  ?= $(HOME)/ps3dev

CC       := $(PS3DEV)/ppu/bin/ppu-gcc
SPRXLINK := sprxlinker

TARGET   := kbd2trigger
SRCS     := source/main.c
OBJS     := $(SRCS:.c=.o)

SPRX_CRT := $(PS3DEV)/ppu/ppu/lib/lv2-sprx.o

CFLAGS   := -O2 -Wall \
             -mhard-float -ffunction-sections -fdata-sections \
             -I$(PSL1GHT)/ppu/include

LDFLAGS  := -L$(PSL1GHT)/ppu/lib \
             -nostartfiles \
             -T$(PS3DEV)/ppu/ppu/lib/lv2.ld \
             -Wl,--gc-sections \
             -Wl,-u,module_start \
             -Wl,-u,module_stop \
             -Wl,--undefined=module_start \
             -Wl,--undefined=module_stop

LIBS     := -lio -llv2

all: $(TARGET).sprx

$(TARGET).sprx: $(TARGET).elf
	$(SPRXLINK) $< $@ && echo "sprxlinker succeeded" || (echo "sprxlinker failed"; exit 1)

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SPRX_CRT) $(OBJS) $(LIBS)
	@echo "Sections in ELF:"
	@$(PS3DEV)/ppu/bin/ppu-readelf -S $@ | grep -E "prx_param|lib.ent|lib.stub" || echo "WARNING: prx sections missing"

source/main.o: source/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f source/main.o $(TARGET).elf $(TARGET).sprx
