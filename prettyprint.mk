BUILD_DIR ?= build/prettyprint
ON1XC ?= on1x/build/on1xc
PYTHON ?= python3
CLANG ?= clang
ON1X_BIND ?= on1x/build/on1x-bind
EXOTRACT ?= on1x/build/libstone-moonstone/exotract/exotract

KERNELS := $(wildcard include/mulib-pretty*) $(addprefix include/mulib-,$(addsuffix .h,linebreak unibreak uniproc dynload utf8))
LDLIBS += -lm -ldl
BRIDGE := on1x/stdlib/prettyprint/runtime.c
SAMPLE := examples/prettyprint-sample.ppdoc
C11 := -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude

.PHONY: all test test-on1x test-exotract test-contract test-kernel-discovery
all: $(BUILD_DIR)/ppfmt $(BUILD_DIR)/prettyprint-demo

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/ppfmt: tools/ppfmt.c $(KERNELS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C11) "$<" $(LDFLAGS) $(LDLIBS) -o "$@"

$(BUILD_DIR)/prettyprint-demo: examples/prettyprint-demo.c $(KERNELS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C11) "$<" $(LDFLAGS) $(LDLIBS) -o "$@"

$(BUILD_DIR)/test_prettydoc: tests/test_prettydoc.c tests/prettyprint-implementation.c $(KERNELS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C11) -UNDEBUG -DPRETTYPRINT_TEST_SEPARATE $(filter %.c,$^) $(LDFLAGS) $(LDLIBS) -o "$@"

$(BUILD_DIR)/test_prettybridge: tests/test_prettybridge.c tests/prettyprint-implementation.c $(BRIDGE) on1x/stdlib/prettyprint/runtime.h $(KERNELS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C11) -UNDEBUG -DPRETTYPRINT_TEST_SEPARATE $(filter %.c,$^) $(LDFLAGS) $(LDLIBS) -o "$@"

test: all $(BUILD_DIR)/test_prettydoc $(BUILD_DIR)/test_prettybridge
	"$(BUILD_DIR)/test_prettydoc"
	"$(BUILD_DIR)/test_prettybridge"
	"$(BUILD_DIR)/prettyprint-demo" > "$(BUILD_DIR)/demo.out"
	$(PYTHON) tests/check_prettyprint.py cli "$(BUILD_DIR)/ppfmt" "$(SAMPLE)"
	$(PYTHON) tests/check_prettyprint.py ppg "$(BUILD_DIR)/ppfmt"

test-on1x: | $(BUILD_DIR)
	$(PYTHON) tests/check_prettyprint.py on1x "$(BUILD_DIR)" "$(ON1XC)" "$(CC)"

test-exotract: | $(BUILD_DIR)
	$(PYTHON) tests/check_prettyprint.py archive "$(BUILD_DIR)" "$(ON1XC)" "$(CLANG)" "$(EXOTRACT)"

test-contract: | $(BUILD_DIR)
	$(PYTHON) tests/check_prettyprint.py contract "$(BUILD_DIR)" "$(ON1X_BIND)" "$(CLANG)"

test-kernel-discovery: | $(BUILD_DIR)
	$(PYTHON) tests/check_prettyprint.py discovery "$(BUILD_DIR)" "$(ON1XC)" "$(CLANG)" "$(EXOTRACT)"
