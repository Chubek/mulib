CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude
LDLIBS ?= -lm
TESTS := $(patsubst tests/%.c,build/%,$(wildcard tests/test-*.c)) build/cdd-test
TESTS += $(patsubst tests/%.c,build/%,$(wildcard tests/media-*-test.c)) build/media-link
# These fixtures need additional TUs, arguments, or a private Xvfb.
TESTS := $(filter-out build/test-notebook-headless build/test-wl-headless-private build/test-x11-nested-live,$(TESTS))
HEADERS := $(wildcard include/*.h include/detail/*.h include/detail/*.inc)
TEST_FIXTURES := $(wildcard tests/fixtures/*.h)
EXAMPLES := $(patsubst examples/%.c,build/%,$(wildcard examples/mulib-*.c)) build/fontshape-native build/docir-publication
EXAMPLES += build/cdd-pipeline
EXAMPLES += build/media-pipeline
EXAMPLES += build/kernel-substrates
.PHONY: all check check-shell check-examples check-compiler demo formats docir-formats clean
.PHONY: muw3 check-muw3 check-muw3lib check-tls
.PHONY: check-openai check-markup check-api-kernels
.PHONY: check-web-primitives
.PHONY: check-graphics check-vulkan check-raytracing check-minimpeg-video check-notebook-desktop
.PHONY: check-notebook-headless check-wl-headless-private check-x11-nested
.PHONY: check-media
check-media:
	python3 tools/media_inventory.py --check
	python3 tools/check_media.py
WEB_PRIMITIVES_TESTS := pricing extraction browser css alloc hostile
TESTS += $(addprefix build/test_web_,$(WEB_PRIMITIVES_TESTS))
build/test_web_%: tests/test_web_%.c $(HEADERS) $(TEST_FIXTURES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
check-web-primitives: $(addprefix build/test_web_,$(WEB_PRIMITIVES_TESTS))
	@set -eu; for name in $(WEB_PRIMITIVES_TESTS); do ./build/test_web_$$name; done
	python3 tests/test_money_reference.py --cc "$(CC)"
	@echo "All 6 web/pricing primitive suites passed."
check-openai: build/test-openai build/test-openai-alloc build/test-http-stream build/test-sse build/test-multipart
	./build/test-openai
	./build/test-openai-alloc
	./build/test-http-stream
	./build/test-sse
	./build/test-multipart
	python3 tools/generate_openai.py --check
	python3 tests/test_openai_integration.py --cc "$(CC)"
check-markup:
	CC="$(CC)" sh tests/run-markup.sh
check-api-kernels:
	CC="$(CC)" CXX="$(CXX)" sh tests/run-api-kernels.sh
check-graphics:
	CC="$(CC)" CXX="$(CXX)" sh tests/run-graphics.sh
VULKAN_CPPFLAGS ?= $(shell pkg-config --cflags vulkan 2>/dev/null)
check-vulkan: build/vulkan-device
	@status=0; ./build/vulkan-device || status=$$?; test "$$status" -eq 0 || test "$$status" -eq 77
build/vulkan-device: tests/graphics/vulkan-device.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(VULKAN_CPPFLAGS) $(CFLAGS) -DMULIB_GLX_VULKAN_ENABLE $< $(LDLIBS) -ldl -o $@
check-raytracing:
	CC="$(CC)" sh tests/run-raytracing.sh
MINIMPEG_CPPFLAGS ?= $(shell pkg-config --cflags libavcodec libavutil)
MINIMPEG_LDLIBS ?= $(shell pkg-config --libs libavcodec libavutil)
check-minimpeg-video: build/minimpeg-disabled build/minimpeg-video
	./build/minimpeg-disabled
	./build/minimpeg-video
	@if command -v mpeg2enc >/dev/null && command -v mpeg2dec >/dev/null; then \
		python3 tests/check-minimpeg-video.py --codec ./build/minimpeg-video; \
	else \
		printf '%s\n' 'SKIP: independent MPEG-1 oracle requires mpeg2enc and mpeg2dec'; \
	fi
build/minimpeg-disabled: tests/minimpeg-video.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
build/minimpeg-video: tests/minimpeg-video.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(MINIMPEG_CPPFLAGS) $(CFLAGS) -DMULIB_MINIMPEG_USE_LIBAVCODEC $< $(LDLIBS) $(MINIMPEG_LDLIBS) -o $@
check-notebook-desktop:
	python3 tests/run-notebook-desktop-headers.py
check-notebook-headless:
	CC="$(CC)" sh tests/run-notebook-headless.sh
check-wl-headless-private: build/test-wl-headless-private
	./build/test-wl-headless-private
check-x11-nested:
	CC="$(CC)" sh tests/run-x11-nested.sh
build/test-wl-headless-private: tests/test-wl-headless-private.c examples/wl-headless-private.c examples/wl-headless-private.h $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_GNU_SOURCE $(filter %.c,$^) $(LDLIBS) -ldl -o $@
build/test-x11-nested-wire: CFLAGS += -D_POSIX_C_SOURCE=200809L
build/test-gfx-%: CFLAGS += -D_POSIX_C_SOURCE=200809L -Wno-misleading-indentation
build/test-gfx-%: LDLIBS += -ldl -pthread
build/test-gfx-%: tests/test-gfx-%.c $(HEADERS) $(TEST_FIXTURES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) $(LDLIBS) -o $@
build/test-gfx-input: tests/gfx-input-tu.c tests/gfx-input-unix.c tests/gfx-input-evdev.c
build/test-gfx-retgtk-core: tests/gfx-retgtk-core-types-a.c tests/gfx-retgtk-core-types-b.c
build/test-gfx-retgtk-model-layout: tests/gfx-retgtk-model-layout-peer.c
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
check-muw3lib: build/muw3
	@set -eu; for suite in mock core protocols services; do \
		./build/muw3 --weblet "library/muw3lib/tests/$$suite.lua"; \
	done
	python3 library/muw3lib/tests/loopback.py --muw3 build/muw3
check-tls: muw3 build/test-tls-crypto build/test-signature build/test-x509 build/test-tls
	./build/test-tls-crypto
	./build/test-signature
	./build/test-x509
	./build/test-tls
	python3 tests/test_tls_integration.py --muw3 build/muw3 --x509 build/test-x509 --signature build/test-signature
build/test-muw3-mune: LDLIBS += -pthread -ldl
build/test-opencode-client: LDLIBS += -pthread -ldl
build/test-api-%-impl.o: tests/api-kernels/%.c $(HEADERS) $(TEST_FIXTURES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/test-api-json-http build/test-api-sse-client build/test-api-websocket: build/%: tests/%.c build/test-api-implementation-impl.o build/test-api-dependencies-impl.o | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -pthread -ldl -o $@
all: check
check-shell:
	CC="$(CC)" sh tests/run_tests.sh
check-compiler:
	python3 tools/check_compiler.py --cc "$(CC)"
build:
	mkdir -p build
build/test-%: tests/test-%.c $(HEADERS) $(TEST_FIXTURES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
build/cdd-test: tests/cdd-test.c tests/cdd-implementation.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) $(LDLIBS) -o $@
build/media-%: tests/media-%.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
build/media-link: tests/media-link-a.c tests/media-link-b.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) $(LDLIBS) -o $@
build/media-pipeline: examples/media-pipeline.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDLIBS) -o $@
build/kernel-substrates: examples/kernel-substrates.c $(HEADERS) | build
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
build/cdd-pipeline: examples/cdd-pipeline.c $(HEADERS) | build
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
