TARGET  = j12dump

DEFINES = -DVERSION=\"1.0.0\"

LFLAGS += -L$(PREFIX)/lib
CFLAGS += -I$(PREFIX)/include/

MAKECMDGOALS ?= debug

CFLAGS += -g3 -O0 -Wall

sources = $(wildcard *.c)
objects = $(subst .c,.o,$(sources))

depend  = .depend

$(depend): Makefile
	$(CC) -MM $(CFLAGS) $(sources) > $@

debug internal release: $(depend) $(objects)
	$(CC) $(LFLAGS) -o $(TARGET) $(objects)

install:
	mkdir -p $(PREFIX)/bin
	cp -f $(TARGET) $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(TARGET) *.o $(depend)

.c.o:
	$(COMPILE.c) $(DEFINES) $(CFLAGS) -c $< $(OUTPUT_OPTION)

-include $(depend)
