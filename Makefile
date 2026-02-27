CC ?= cc
# Stronger warnings for code quality
CFLAGS ?= -O2 -Iinclude -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
		  -Wcast-align -Wcast-qual -Wpointer-arith -Wformat=2 \
		  -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wundef \
		  -std=c11
AR ?= ar

PREFIX ?= /usr/local

LIBNAME = libspacepacket.a
BUILD_DIR = build
LIB_PATH = $(BUILD_DIR)/$(LIBNAME)
OBJ_PATH = $(BUILD_DIR)/src/space_packet.o
EXAMPLE_PATH = $(BUILD_DIR)/examples/spacepacket_example
CTEST_PATH = $(BUILD_DIR)/tests/ctest

all: lib example test ctest

lib: $(LIB_PATH)

$(OBJ_PATH): src/space_packet.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c src/space_packet.c -o $(OBJ_PATH)

$(LIB_PATH): $(OBJ_PATH)
	mkdir -p $(dir $@)
	$(AR) rcs $(LIB_PATH) $(OBJ_PATH)

example: $(EXAMPLE_PATH)

$(EXAMPLE_PATH): lib examples/main.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude examples/main.c $(LIB_PATH) -o $(EXAMPLE_PATH)


ctest: $(CTEST_PATH)

$(CTEST_PATH): lib tests/unit_tests.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude tests/unit_tests.c $(LIB_PATH) -o $(CTEST_PATH)

coverage-html:
	bash scripts/coverage_html.sh

clean:
	rm -rf $(BUILD_DIR)

install: lib
	mkdir -p $(PREFIX)/lib $(PREFIX)/include
	cp $(LIB_PATH) $(PREFIX)/lib/$(LIBNAME)
	cp include/space_packet.h $(PREFIX)/include/

.PHONY: all lib example test ctest coverage-html clean install
