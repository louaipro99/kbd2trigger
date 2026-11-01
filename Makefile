TARGET		:= kbd2trigger
OBJS		:= source/main.o

TITLE		:= kbd2trigger
APPID		:= KBD00001
CONTENTID	:= UP0001-KBD00001_00-0000000000000000

include $(PSL1GHT)/ppu_rules

LIBS		:= -lpad -lkb -lsysutil -llv2

$(TARGET).sprx: $(TARGET).elf
	$(SPRXLINKER) $< $@

$(TARGET).elf: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).sprx
