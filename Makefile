CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude
LDLIBS ?= -lm
TESTS := $(patsubst tests/%.c,build/%,$(wildcard tests/test-*.c))
HEADERS := $(wildcard include/*.h include/detail/*.h include/detail/*.inc)
TEST_FIXTURES := $(wildcard tests/fixtures/*.h)
EXAMPLES := $(patsubst examples/%.c,build/%,$(wildcard examples/mulib-*.c)) build/fontshape-native build/docir-publication
.PHONY: all check check-shell check-examples check-compiler demo formats docir-formats clean
.PHONY: muw3 check-muw3 check-tls
.PHONY: check-openai
check-openai: build/test-openai build/test-openai-alloc build/test-http-stream build/test-sse build/test-multipart
	./build/test-openai
	./build/test-openai-alloc
	./build/test-http-stream
	./build/test-sse
	./build/test-multipart
	python3 tools/generate_openai.py --check
	python3 tests/test_openai_integration.py --cc "$(CC)"
muw3: build/muw3 build/microchand
build/microchan-runtime.o: library/microchan/runtime.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/lmuw3-runtime.o: library/lmuw3/runtime.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/muw3: library/lmuw3/main.c build/lmuw3-runtime.o build/microchan-runtime.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -pthread -lm -ldl -o $@
build/microchand: library/microchan/main.c build/lmuw3-runtime.o build/microchan-runtime.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -pthread -lm -ldl -o $@
check-muw3: muw3 build/test-muw3 build/test-microchan build/test-muw3-mune
	./build/test-muw3
	./build/test-microchan
	./build/test-muw3-mune
	python3 tests/test_muw3_integration.py --muw3 build/muw3 --microchand build/microchand
check-tls: muw3 build/test-tls-crypto build/test-signature build/test-x509 build/test-tls
	./build/test-tls-crypto
	./build/test-signature
	./build/test-x509
	./build/test-tls
	python3 tests/test_tls_integration.py --muw3 build/muw3 --x509 build/test-x509 --signature build/test-signature
build/test-muw3-mune: LDLIBS += -pthread -ldl
all: check
check-shell:
	CC="$(CC)" sh tests/run_tests.sh
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
