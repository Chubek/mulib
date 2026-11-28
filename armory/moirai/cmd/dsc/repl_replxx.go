//go:build !isocline && !domrl

package main

/*
#cgo CFLAGS: -I../../../third_party/replxx/include
#cgo CXXFLAGS: -std=c++11 -DREPLXX_STATIC -I../../../third_party/replxx/include -I../../../third_party/replxx/src
#cgo LDFLAGS: -lstdc++ -pthread
#include "replxx.h"
#include <errno.h>
#include <stdlib.h>

void dsc_replxx_complete_go(char*, replxx_completions*, int*, void*);
void dsc_replxx_hint_go(char*, replxx_hints*, int*, ReplxxColor*, void*);
void dsc_replxx_highlight_go(char*, ReplxxColor*, int, void*);

static void dsc_replxx_complete(const char* input, replxx_completions* completions, int* contextLen, void* userData) {
  dsc_replxx_complete_go((char*)input, completions, contextLen, userData);
}

static void dsc_replxx_hint(const char* input, replxx_hints* hints, int* contextLen, ReplxxColor* color, void* userData) {
  dsc_replxx_hint_go((char*)input, hints, contextLen, color, userData);
}

static void dsc_replxx_highlight(const char* input, ReplxxColor* colors, int size, void* userData) {
  dsc_replxx_highlight_go((char*)input, colors, size, userData);
}

static void dsc_replxx_set_completion(Replxx* rx, int enabled) {
  replxx_set_completion_callback(rx, enabled ? dsc_replxx_complete : NULL, NULL);
  replxx_set_hint_callback(rx, enabled ? dsc_replxx_hint : NULL, NULL);
}

static void dsc_replxx_set_highlight(Replxx* rx, int enabled) {
  replxx_set_highlighter_callback(rx, enabled ? dsc_replxx_highlight : NULL, NULL);
}

static char const* dsc_replxx_input(Replxx* rx, char const* prompt) {
  char const* result = NULL;
  do {
    result = replxx_input(rx, prompt);
  } while (result == NULL && errno == EAGAIN);
  return result;
}
*/
import "C"

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"unsafe"
)

type replxxEditor struct {
	rx      *C.Replxx
	history string
	theme   string
}

func newREPLEditor(cfg replConfig) (replEditor, error) {
	rx := C.replxx_init()
	if rx == nil {
		return nil, fmt.Errorf("replxx_init returned nil")
	}
	e := &replxxEditor{rx: rx, history: cfg.History, theme: cfg.Theme}
	C.replxx_install_window_change_handler(rx)
	C.replxx_set_max_history_size(rx, 200)
	C.replxx_set_unique_history(rx, 1)
	C.replxx_set_completion_count_cutoff(rx, 128)
	C.replxx_set_complete_on_empty(rx, 0)
	C.replxx_set_double_tab_completion(rx, 0)
	C.replxx_set_immediate_completion(rx, 1)
	C.replxx_set_max_hint_rows(rx, 4)
	C.replxx_set_hint_delay(rx, 100)
	C.replxx_enable_bracketed_paste(rx)
	breaks := C.CString(" \t\n\"'`@$><=;|&{([])}")
	C.replxx_set_word_break_characters(rx, breaks)
	C.free(unsafe.Pointer(breaks))
	if cfg.History != "" {
		_ = os.MkdirAll(filepath.Dir(cfg.History), 0700)
		history := C.CString(cfg.History)
		C.replxx_history_load(rx, history)
		C.free(unsafe.Pointer(history))
	}
	e.SetAutocomplete(cfg.AutoComplete)
	e.SetHighlight(cfg.Highlight)
	return e, nil
}

//export dsc_replxx_complete_go
func dsc_replxx_complete_go(input *C.char, completions *C.replxx_completions, contextLen *C.int, userData unsafe.Pointer) {
	prefix := completionPrefix(C.GoString(input))
	*contextLen = C.int(len([]rune(prefix)))
	for _, match := range completionMatches(prefix) {
		cs := C.CString(match)
		C.replxx_add_color_completion(completions, cs, C.ReplxxColor(replColorCyan))
		C.free(unsafe.Pointer(cs))
	}
}

//export dsc_replxx_hint_go
func dsc_replxx_hint_go(input *C.char, hints *C.replxx_hints, contextLen *C.int, color *C.ReplxxColor, userData unsafe.Pointer) {
	prefix := completionPrefix(C.GoString(input))
	if prefix == "" {
		return
	}
	*contextLen = C.int(len([]rune(prefix)))
	*color = C.ReplxxColor(replColorGray)
	for _, match := range completionMatches(prefix) {
		if match == prefix {
			continue
		}
		cs := C.CString(match)
		C.replxx_add_hint(hints, cs)
		C.free(unsafe.Pointer(cs))
		return
	}
}

//export dsc_replxx_highlight_go
func dsc_replxx_highlight_go(input *C.char, colors *C.ReplxxColor, size C.int, userData unsafe.Pointer) {
	if size <= 0 {
		return
	}
	spans := highlightREPLColors(C.GoString(input))
	out := unsafe.Slice(colors, int(size))
	for i := range out {
		color := replColorDefault
		if i < len(spans) {
			color = spans[i]
		}
		out[i] = C.ReplxxColor(color)
	}
}

func (e *replxxEditor) ReadLine(prompt string) (string, bool) {
	promptC := C.CString(prompt)
	defer C.free(unsafe.Pointer(promptC))
	result := C.dsc_replxx_input(e.rx, promptC)
	if result == nil {
		return "", false
	}
	return C.GoString(result), true
}

func (e *replxxEditor) AddHistory(line string) {
	if strings.TrimSpace(line) == "" {
		return
	}
	lineC := C.CString(line)
	C.replxx_history_add(e.rx, lineC)
	C.free(unsafe.Pointer(lineC))
}

func (e *replxxEditor) SetAutocomplete(enabled bool) {
	C.dsc_replxx_set_completion(e.rx, boolToCInt(enabled))
}

func (e *replxxEditor) SetHighlight(enabled bool) {
	C.dsc_replxx_set_highlight(e.rx, boolToCInt(enabled))
}

func (e *replxxEditor) SetTheme(theme string) {
	e.theme = theme
}

func (e *replxxEditor) Close() {
	if e.rx == nil {
		return
	}
	if e.history != "" {
		_ = os.MkdirAll(filepath.Dir(e.history), 0700)
		history := C.CString(e.history)
		C.replxx_history_save(e.rx, history)
		C.free(unsafe.Pointer(history))
	}
	C.replxx_end(e.rx)
	e.rx = nil
}

func (e *replxxEditor) Prompt() string {
	if strings.EqualFold(e.theme, "compact") {
		return "\x1b[1;32mdom\x1b[0m>"
	}
	return "\x1b[1;32mdom\x1b[0m> "
}

func boolToCInt(v bool) C.int {
	if v {
		return 1
	}
	return 0
}
