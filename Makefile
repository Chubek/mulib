CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude
LDLIBS ?= -lm
TESTS := $(patsubst tests/%.c,build/%,$(wildcard tests/test-*.c))
HEADERS := $(wildcard include/*.h include/detail/*.h)
TEST_FIXTURES := $(wildcard tests/fixtures/*.h)
EXAMPLES := $(patsubst examples/%.c,build/%,$(wildcard examples/mulib-*.c)) build/fontshape-native build/docir-publication
.PHONY: all check check-examples check-compiler demo formats docir-formats clean
all: check
check-compiler:
	python3 tools/check_compiler.py --cc "$(CC)"
build:
	mkdir -p build
build/test-%: tests/test-%.c $(HEADERS) $(TEST_FIXTURES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
check: $(TESTS)
	@set -eu; for t in $(TESTS); do ./$$t; done
	@echo "All $(words $(TESTS)) C11 API tests passed."
build/mulib-%: examples/mulib-%.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
check-examples: $(EXAMPLES)
	@set -eu; for e in $(EXAMPLES); do ./$$e; done
	@echo "All $(words $(EXAMPLES)) publishing examples passed."
build/fontshape-native: examples/fontshape-native.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
build/docir-publication: examples/docir-publication.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
docir-formats: build/docir-publication
	mkdir -p build/docir-fixtures
	./build/docir-publication build/docir-fixtures
	python3 tests/verify-docir-publication.py build/docir-fixtures
build/publish-example: examples/build-publication.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
demo: build/publish-example
	@echo "Use: mkdir -p /tmp/mulib-published && ./build/publish-example /tmp/mulib-published"
formats: build/publish-example
	mkdir -p build/fixtures
	./build/publish-example build/fixtures
	python3 tests/verify-formats.py build/fixtures
clean:
	rm -rf build
