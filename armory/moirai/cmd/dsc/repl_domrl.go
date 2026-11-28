//go:build domrl && cgo

package main

/*
#cgo CFLAGS: -I../../../domlibs/domrl/include
#include <domrl/domrl.h>
#include <stdlib.h>

void dsc_domrl_complete_go(char*, size_t, drl_completions*);
void dsc_domrl_highlight_go(char*, size_t, drl_span*, size_t*, size_t);

static void dsc_domrl_complete(drl_session* session, const drl_completion_ctx* context,
                               drl_completions* completions, void* userdata) {
  (void)session; (void)userdata;
  dsc_domrl_complete_go((char*)context->word, context->word_len, completions);
}

static void dsc_domrl_highlight(drl_session* session, const char* text, size_t length,
                                drl_span* spans, size_t* count, size_t maximum, void* userdata) {
  (void)session; (void)userdata;
  dsc_domrl_highlight_go((char*)text, length, spans, count, maximum);
}

static void dsc_domrl_set_completion(drl_session* session, int enabled) {
  drl_completer_remove(session, "domscript");
  if (enabled) drl_completer_add(session, "domscript", 10, dsc_domrl_complete, NULL);
}

static void dsc_domrl_set_highlight(drl_session* session, int enabled) {
  drl_highlighter_set(session, enabled ? dsc_domrl_highlight : NULL, NULL);
}

static void dsc_domrl_add_completion(drl_completions* completions, const char* text) {
  drl_completions_add(completions, text, text, "Domscript");
}

static void dsc_domrl_set_span(drl_span* spans, size_t* count, size_t maximum,
                               size_t start, size_t end, int color) {
  static const char* styles[] = { "", "operator", "string", "number", "keyword", "comment" };
  int style = 0;
  if (color == 2) style = 2;
  else if (color == 3 || color == 13) style = 3;
  else if (color == 5 || color == 6) style = 4;
  else if (color == 8) style = 5;
  else if (color == 1) style = 1;
  if (style && *count < maximum) {
    spans[*count].start = start;
    spans[*count].end = end;
    spans[*count].style = styles[style];
    (*count)++;
  }
}

static unsigned dsc_domrl_ansi(unsigned color) { return DRL_COLOR_ANSI(color); }
*/
import "C"

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"unsafe"
)

type domrlEditor struct {
	session  *C.drl_session
	history  string
	theme    string
}

func newREPLEditor(cfg replConfig) (replEditor, error) {
	var config C.drl_config
	C.drl_config_init(&config)
	config.history_max = 200
	if !cfg.Highlight {
		config.flags |= C.DRL_F_NO_COLOR
	}
	var session *C.drl_session
	if status := C.drl_session_new(&session, &config); status != C.DRL_OK || session == nil {
		return nil, fmt.Errorf("drl_session_new: %s", C.GoString(C.drl_status_str(status)))
	}
	e := &domrlEditor{session: session, history: cfg.History, theme: cfg.Theme}
	breaks := C.CString(" \t\n\"'`@$><=;|&{([])}")
	C.drl_set_word_break_chars(session, breaks)
	C.free(unsafe.Pointer(breaks))
	setDomRLStyles(session)
	e.SetAutocomplete(cfg.AutoComplete)
	e.SetHighlight(cfg.Highlight)
	if cfg.History != "" {
		_ = os.MkdirAll(filepath.Dir(cfg.History), 0700)
		history := C.CString(cfg.History)
		C.drl_history_load(session, history)
		C.free(unsafe.Pointer(history))
	}
	return e, nil
}

func setDomRLStyles(session *C.drl_session) {
	styles := []struct {
		name  string
		color C.uint
		attrs C.uint
	}{
		{"operator", C.dsc_domrl_ansi(1), 0},
		{"string", C.dsc_domrl_ansi(2), 0},
		{"number", C.dsc_domrl_ansi(13), 0},
		{"keyword", C.dsc_domrl_ansi(6), C.DRL_ATTR_BOLD},
		{"comment", C.dsc_domrl_ansi(8), C.DRL_ATTR_ITALIC},
	}
	for _, entry := range styles {
		name := C.CString(entry.name)
		style := C.drl_style{fg: entry.color, bg: C.DRL_COLOR_DEFAULT, attrs: entry.attrs}
		C.drl_theme_set_style(session, name, &style)
		C.free(unsafe.Pointer(name))
	}
}

//export dsc_domrl_complete_go
func dsc_domrl_complete_go(word *C.char, length C.size_t, completions *C.drl_completions) {
	prefix := C.GoStringN(word, C.int(length))
	for _, match := range completionMatches(prefix) {
		candidate := C.CString(match)
		C.dsc_domrl_add_completion(completions, candidate)
		C.free(unsafe.Pointer(candidate))
	}
}

//export dsc_domrl_highlight_go
func dsc_domrl_highlight_go(text *C.char, length C.size_t, spans *C.drl_span, count *C.size_t, maximum C.size_t) {
	line := C.GoStringN(text, C.int(length))
	colors := highlightREPLColors(line)
	byteOffsets := make([]int, 0, len(colors)+1)
	for offset := range line {
		byteOffsets = append(byteOffsets, offset)
	}
	byteOffsets = append(byteOffsets, len(line))
	*count = 0
	for start := 0; start < len(colors); {
		color := colors[start]
		end := start + 1
		for end < len(colors) && colors[end] == color {
			end++
		}
		if color != replColorDefault && start < len(byteOffsets) && end < len(byteOffsets) {
			C.dsc_domrl_set_span(spans, count, maximum,
				C.size_t(byteOffsets[start]), C.size_t(byteOffsets[end]), C.int(color))
		}
		start = end
	}
}

func (e *domrlEditor) ReadLine(prompt string) (string, bool) {
	promptC := C.CString(prompt)
	defer C.free(unsafe.Pointer(promptC))
	var line *C.char
	status := C.drl_session_read(e.session, promptC, &line)
	if status != C.DRL_OK || line == nil {
		return "", false
	}
	defer C.drl_free(unsafe.Pointer(line))
	return C.GoString(line), true
}

func (e *domrlEditor) AddHistory(line string) {
	if strings.TrimSpace(line) == "" {
		return
	}
	entry := C.CString(line)
	C.drl_history_add(e.session, entry)
	C.free(unsafe.Pointer(entry))
}

func (e *domrlEditor) SetAutocomplete(enabled bool) {
	C.dsc_domrl_set_completion(e.session, boolToCInt(enabled))
}

func (e *domrlEditor) SetHighlight(enabled bool) {
	C.dsc_domrl_set_highlight(e.session, boolToCInt(enabled))
}

func (e *domrlEditor) SetTheme(theme string) { e.theme = theme }

func (e *domrlEditor) Close() {
	if e.session == nil {
		return
	}
	if e.history != "" {
		_ = os.MkdirAll(filepath.Dir(e.history), 0700)
		history := C.CString(e.history)
		C.drl_history_save(e.session, history)
		C.free(unsafe.Pointer(history))
	}
	C.drl_session_free(e.session)
	e.session = nil
}

func (e *domrlEditor) Prompt() string {
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
