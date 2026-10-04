CC ?= cc
# Optimisation / instrumentation flags; overridden by tools/coverage_html.sh
OPT ?= -O2
SANITIZE_OPT = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
			   -fno-sanitize-recover=all
# Stronger warnings for code quality; any warning fails the build
CFLAGS ?= $(OPT) -Iinclude -Werror -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
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
SANITIZE_DIR = $(BUILD_DIR)/sanitize

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

# Rebuild everything with ASan + UBSan and run the unit tests and the example.
# Program output is shown only on failure; sanitizer reports go to stderr and are always
# visible. The instrumented build is removed afterwards, on success and on failure.
sanitize:
	@$(MAKE) --no-print-directory clean >/dev/null
	@$(MAKE) --no-print-directory lib example ctest OPT="$(SANITIZE_OPT)" >/dev/null \
		|| { $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@mkdir -p $(SANITIZE_DIR)
	@echo "Sanitizers (ASan + UBSan):"
	@./$(CTEST_PATH) >$(SANITIZE_DIR)/unit_tests.log \
		&& echo "  library via unit tests : no errors detected" \
		|| { cat $(SANITIZE_DIR)/unit_tests.log; echo "  library via unit tests : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@./$(EXAMPLE_PATH) >$(SANITIZE_DIR)/example.log \
		&& echo "  library via example    : no errors detected" \
		|| { cat $(SANITIZE_DIR)/example.log; echo "  library via example    : FAILED"; \
		     $(MAKE) --no-print-directory clean >/dev/null; exit 1; }
	@$(MAKE) --no-print-directory clean >/dev/null
	@echo "Result: PASS"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all lib example test ctest coverage-html sanitize clean
