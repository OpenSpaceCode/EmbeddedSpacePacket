CC ?= cc
CFLAGS ?= -O2 -Iinclude -Wall -Wextra -std=c99

CC ?= cc
CXX ?= g++
CFLAGS ?= -O2 -Iinclude -Wall -Wextra -std=c99
CXXFLAGS ?= -O2 -Iinclude -Wall -Wextra -std=gnu++11
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
	rm -f src/*.o $(LIBNAME) examples/spacepacket_example tests/test_packet tests/gtest_tests

install: lib
	mkdir -p $(PREFIX)/lib $(PREFIX)/include
	cp $(LIBNAME) $(PREFIX)/lib/
	cp include/space_packet.h $(PREFIX)/include/

.PHONY: all lib example test gtest clean install
