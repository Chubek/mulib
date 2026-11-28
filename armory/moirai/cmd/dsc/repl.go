//go:build isocline

package main

/*
#cgo CFLAGS: -I../../../third_party/isocline/include
#include "isocline.h"
#include <stdlib.h>
void dsc_complete(void*, char*);
void dsc_highlight(void*, char*, void*);
static void dsc_setup(void) {
  ic_set_default_completer((ic_completer_fun_t*)dsc_complete, NULL);
  ic_set_default_highlighter((ic_highlight_fun_t*)dsc_highlight, NULL);
}
*/
import "C"

import (
	"strings"
	"unsafe"
)

type isoclineEditor struct {
	theme string
}

func newREPLEditor(cfg replConfig) (replEditor, error) {
	C.dsc_setup()
	e := &isoclineEditor{theme: cfg.Theme}
	e.SetAutocomplete(cfg.AutoComplete)
	e.SetHighlight(cfg.Highlight)
	if cfg.History != "" {
		history := C.CString(cfg.History)
		C.ic_set_history(history, 200)
		C.free(unsafe.Pointer(history))
	}
	e.SetTheme(cfg.Theme)
	return e, nil
}

//export dsc_complete
func dsc_complete(env unsafe.Pointer, prefix *C.char) {
	p := completionPrefix(C.GoString(prefix))
	for _, match := range completionMatches(p) {
		cs := C.CString(match)
		C.ic_add_completion((*C.ic_completion_env_t)(env), cs)
		C.free(unsafe.Pointer(cs))
	}
}

//export dsc_highlight
func dsc_highlight(env unsafe.Pointer, input *C.char, arg unsafe.Pointer) {
	colors := highlightREPLColors(C.GoString(input))
	start := -1
	color := replColorDefault
	for i, c := range colors {
		if c != color {
			if start >= 0 && color != replColorDefault {
				isoclineHighlight(env, start, i, color)
			}
			start, color = i, c
		}
	}
	if start >= 0 && color != replColorDefault {
		isoclineHighlight(env, start, len(colors), color)
	}
}

func isoclineHighlight(env unsafe.Pointer, start, end, color int) {
	style := isoclineStyle(color)
	if style == "" {
		return
	}
	cs := C.CString(style)
	C.ic_highlight((*C.ic_highlight_env_t)(env), C.long(start), C.long(end-start), cs)
	C.free(unsafe.Pointer(cs))
}

func isoclineStyle(color int) string {
	switch color {
	case replColorGreen:
		return "string"
	case replColorBrown, replColorBrightMagenta:
		return "number"
	case replColorMagenta, replColorCyan:
		return "keyword"
	case replColorGray:
		return "comment"
	case replColorRed:
		return "error"
	default:
		return ""
	}
}

func (e *isoclineEditor) ReadLine(prompt string) (string, bool) {
	promptC := C.CString(prompt)
	defer C.free(unsafe.Pointer(promptC))
	p := C.ic_readline(promptC)
	if p == nil {
		return "", false
	}
	defer C.free(unsafe.Pointer(p))
	return C.GoString(p), true
}

func (e *isoclineEditor) AddHistory(string) {}

func (e *isoclineEditor) SetAutocomplete(enabled bool) {
	C.ic_enable_completion_preview(C.bool(enabled))
}

func (e *isoclineEditor) SetHighlight(enabled bool) {
	C.ic_enable_highlight(C.bool(enabled))
}

func (e *isoclineEditor) SetTheme(theme string) {
	e.theme = theme
	marker := "> "
	if strings.EqualFold(theme, "compact") {
		marker = ">"
	}
	cs := C.CString(marker)
	defer C.free(unsafe.Pointer(cs))
	C.ic_set_prompt_marker(cs, nil)
}

func (e *isoclineEditor) Close() {}

func (e *isoclineEditor) Prompt() string {
	if strings.EqualFold(e.theme, "compact") {
		return "dom>"
	}
	return "dom> "
}
