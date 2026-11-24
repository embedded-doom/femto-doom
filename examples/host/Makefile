CFLAGS  ?= -O2
DEFINES ?=
SDL     ?= $(shell sdl2-config --cflags --libs)

ENGINE  := $(wildcard ../../src/*.c)

host: host.c screen.c screen.h $(ENGINE) $(wildcard ../../src/*.h)
	$(CC) -std=c11 -fno-short-enums $(CFLAGS) -I../../src $(addprefix -D,$(DEFINES)) \
		-o $@ host.c screen.c $(ENGINE) $(SDL)

clean:
	rm -f host host.exe

.PHONY: clean
