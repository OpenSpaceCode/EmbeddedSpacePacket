CC ?= cc
# Stronger warnings for code quality
CFLAGS ?= -O2 -Iinclude -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
		  -Wcast-align -Wcast-qual -Wpointer-arith -Wformat=2 \
		  -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wundef \
		  -std=c11
AR ?= ar

PREFIX ?= /usr/local

LIBNAME = libspacepacket.a

all: lib example test ctest

lib: src/space_packet.c
	$(CC) $(CFLAGS) -c src/space_packet.c -o src/space_packet.o
	$(AR) rcs $(LIBNAME) src/space_packet.o

example: lib examples/main.c
	$(CC) $(CFLAGS) -Iinclude -L. examples/main.c -L. -lspacepacket -o examples/spacepacket_example


ctest: lib tests/unit_tests.c
	$(CC) $(CFLAGS) -Iinclude tests/unit_tests.c -L. -lspacepacket -o tests/ctest

clean:
	rm -f src/*.o $(LIBNAME) examples/spacepacket_example tests/ctest

install: lib
	mkdir -p $(PREFIX)/lib $(PREFIX)/include
	cp $(LIBNAME) $(PREFIX)/lib/
	cp include/space_packet.h $(PREFIX)/include/

.PHONY: all lib example test ctest clean install
