# Linux ELF runtime libraries and tools; no Windows SDK or Wine dependency.
CC ?= cc
AR ?= ar
PREFIX ?= /usr/local
DESTDIR ?=
BUILD ?= build
INCLUDE ?= ../../include
CFLAGS ?= -O2 -g
SHIRAZ_CPPFLAGS = -D_POSIX_C_SOURCE=200809L -I$(INCLUDE)
WARN = -std=c11 -Wall -Wextra -Wpedantic -Werror
RUNTIME = $(BUILD)/runtime.o $(BUILD)/kernel32.o $(BUILD)/registry.o
HEADERS = $(wildcard $(INCLUDE)/mulib-*.h) internal.h

.PHONY: all static shared tools check install clean
all: static shared tools
static: $(BUILD)/libshiraz.a $(BUILD)/libshiraz-win32.a
shared: $(BUILD)/libshiraz.so $(BUILD)/libshiraz-win32.so
tools: $(BUILD)/shiraz $(BUILD)/shiraz-reg

$(BUILD):
	mkdir -p "$@"
$(BUILD)/%.o: %.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(SHIRAZ_CPPFLAGS) $(CFLAGS) $(WARN) -fPIC -c $< -o $@
$(BUILD)/libshiraz.a: $(BUILD)/core.o
	$(AR) rcs $@ $^
$(BUILD)/libshiraz-win32.a: $(RUNTIME)
	$(AR) rcs $@ $^
$(BUILD)/libshiraz.so: $(BUILD)/core.o
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libshiraz.so -Wl,--no-undefined -o $@ $^
$(BUILD)/libshiraz-win32.so: $(RUNTIME) $(BUILD)/libshiraz.so
	$(CC) $(LDFLAGS) -shared -Wl,-soname,libshiraz-win32.so -Wl,--no-undefined -Wl,-rpath,'$$ORIGIN' -o $@ $(RUNTIME) -L$(BUILD) -lshiraz
$(BUILD)/shiraz: cli.c $(BUILD)/libshiraz-win32.a $(BUILD)/libshiraz.a
	$(CC) $(CPPFLAGS) $(SHIRAZ_CPPFLAGS) $(CFLAGS) $(WARN) -DSHIRAZ_INCLUDE_DIR='"$(abspath $(INCLUDE))"' -DSHIRAZ_LIBRARY_DIR='"$(abspath $(BUILD))"' $(LDFLAGS) -o $@ $^
$(BUILD)/shiraz-reg: reg.c $(BUILD)/libshiraz-win32.a $(BUILD)/libshiraz.a
	$(CC) $(CPPFLAGS) $(SHIRAZ_CPPFLAGS) $(CFLAGS) $(WARN) $(LDFLAGS) -o $@ $^
check: all
	python3 ../../tools/check_shiraz.py --shiraz $(BUILD)/shiraz --registry $(BUILD)/shiraz-reg
install: all
	install -d "$(DESTDIR)$(PREFIX)/lib" "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(PREFIX)/include"
	install -m 644 $(BUILD)/*.a $(BUILD)/*.so "$(DESTDIR)$(PREFIX)/lib/"
	install -m 644 $(addprefix $(INCLUDE)/mulib-,shiraz.h pe.h coff.h endian.h strbuf.h) "$(DESTDIR)$(PREFIX)/include/"
	$(CC) $(CPPFLAGS) $(SHIRAZ_CPPFLAGS) $(CFLAGS) $(WARN) -DSHIRAZ_INCLUDE_DIR='"$(PREFIX)/include"' -DSHIRAZ_LIBRARY_DIR='"$(PREFIX)/lib"' $(LDFLAGS) -o $(BUILD)/shiraz-installed cli.c $(BUILD)/libshiraz-win32.a $(BUILD)/libshiraz.a
	install -m 755 $(BUILD)/shiraz-installed "$(DESTDIR)$(PREFIX)/bin/shiraz"
	install -m 755 $(BUILD)/shiraz-reg "$(DESTDIR)$(PREFIX)/bin/"
clean:
	rm -rf "$(BUILD)"
