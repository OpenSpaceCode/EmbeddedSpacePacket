CC ?= cc
# Optimisation / instrumentation flags; overridden by tools/coverage_html.sh
OPT ?= -O2
# Stronger warnings for code quality
CFLAGS ?= $(OPT) -Iinclude -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
		  -Wcast-align -Wcast-qual -Wpointer-arith -Wformat=2 \
		  -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wundef \
		  -std=c99
AR ?= ar

LIBNAME = libspacepacket.a
BUILD_DIR = build
LIB_PATH = $(BUILD_DIR)/$(LIBNAME)
OBJ_PATH = $(BUILD_DIR)/src/space_packet.o
EXAMPLE_PATH = $(BUILD_DIR)/examples/spacepacket_example
CTEST_PATH = $(BUILD_DIR)/tests/ctest

PUBLIC_HEADERS = include/space_packet.h
TEST_SOURCES = tests/unit_tests.c tests/test_space_packet.c
TEST_HEADERS = tests/cunit.h tests/test_runners.h

all: lib example test

lib: $(LIB_PATH)

$(OBJ_PATH): src/space_packet.c $(PUBLIC_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude -c src/space_packet.c -o $(OBJ_PATH)

$(LIB_PATH): $(OBJ_PATH)
	mkdir -p $(dir $@)
	$(AR) rcs $(LIB_PATH) $(OBJ_PATH)

example: $(EXAMPLE_PATH)

$(EXAMPLE_PATH): $(LIB_PATH) examples/main.c $(PUBLIC_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude examples/main.c $(LIB_PATH) -o $(EXAMPLE_PATH)

ctest: $(CTEST_PATH)

$(CTEST_PATH): $(LIB_PATH) $(TEST_SOURCES) $(TEST_HEADERS) $(PUBLIC_HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(TEST_SOURCES) $(LIB_PATH) -o $(CTEST_PATH)

test: ctest
	./$(CTEST_PATH)

coverage-html:
	bash tools/coverage_html.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all lib example test ctest coverage-html clean
