// Package diag is the shared diagnostics facility of Domscript. Every
// user-facing error in the system — lexer, parser, sema, loader, and
// runtime error reporting — flows through here; no other package formats
// positioned errors for presentation.
package diag

import (
	"fmt"
	"strings"

	"domscript/internal/token"
)

// Severity of a diagnostic.
type Severity uint8

const (
	Error Severity = iota
	Warning
	Note
)

func (s Severity) String() string {
	switch s {
	case Error:
		return "error"
	case Warning:
		return "warning"
	default:
		return "note"
	}
}

// Span is a half-open source range [Start, End).
type Span struct {
	Start token.Pos
	End   token.Pos
}

func (s Span) String() string { return s.Start.String() }

// Diagnostic is one positioned message with optional follow-up notes.
type Diagnostic struct {
	Sev    Severity
	Span   Span
	Msg    string
	Notes  []string
	source string // full source text of the file, for rendering
}

// Note appends a note rendered after the main message.
func (d *Diagnostic) Note(msg string) *Diagnostic {
	d.Notes = append(d.Notes, msg)
	return d
}

// WithSource attaches the file's source text so Render can show the line.
func (d *Diagnostic) WithSource(src string) *Diagnostic {
	d.source = src
	return d
}

// Error makes Diagnostic usable as a Go error inside tooling. It renders
// the one-line form; use Render for the caret form.
func (d *Diagnostic) Error() string {
	return fmt.Sprintf("%s: %s: %s", d.Span.Start, d.Sev, d.Msg)
}

// Render formats the diagnostic with its source line and a caret span:
//
//	file.doml:3:9: error: unexpected ')'
//	    3 | let x = (1 + )
//	      |         ^
func (d *Diagnostic) Render() string {
	var b strings.Builder
	fmt.Fprintf(&b, "%s: %s: %s", d.Span.Start, d.Sev, d.Msg)
	if d.source == "" {
		for _, n := range d.Notes {
			fmt.Fprintf(&b, "\n  note: %s", n)
		}
		return b.String()
	}
	lines := strings.Split(d.source, "\n")
	ln := d.Span.Start.Line
	if ln >= 1 && ln <= len(lines) {
		text := lines[ln-1]
		fmt.Fprintf(&b, "\n\t%4d | %s", ln, text)
		caretCol := d.Span.Start.Col
		spanLen := d.Span.End.Col - caretCol
		if d.Span.End.Line != d.Span.Start.Line || spanLen < 1 {
			spanLen = 1
		}
		// Columns are byte-based; the gutter prefix is "\t%4d | ".
		pad := caretCol - 1
		if pad > len(text) {
			pad = len(text)
		}
		fmt.Fprintf(&b, "\n\t     | %s%s", strings.Repeat(" ", pad), strings.Repeat("^", spanLen))
	}
	for _, n := range d.Notes {
		fmt.Fprintf(&b, "\n\t  note: %s", n)
	}
	return b.String()
}

// Set collects diagnostics for one compilation unit.
type Set struct {
	Diags  []Diagnostic
	source string
	file   string
}

// NewSet creates a set bound to one source file (used for caret rendering).
func NewSet(file, source string) *Set {
	return &Set{source: source, file: file}
}

// Add records a diagnostic; span may be zero for unpositioned messages.
func (s *Set) Add(sev Severity, span Span, format string, args ...any) *Diagnostic {
	if span.Start.File == "" {
		span.Start.File = s.file
	}
	d := &Diagnostic{Sev: sev, Span: span, Msg: fmt.Sprintf(format, args...), source: s.source}
	s.Diags = append(s.Diags, *d)
	return d
}

// Errorf records an error diagnostic and returns it.
func (s *Set) Errorf(span Span, format string, args ...any) *Diagnostic {
	return s.Add(Error, span, format, args...)
}

// Warnf records a warning diagnostic.
func (s *Set) Warnf(span Span, format string, args ...any) *Diagnostic {
	return s.Add(Warning, span, format, args...)
}

// HasErrors reports whether any recorded diagnostic is an error.
func (s *Set) HasErrors() bool {
	for i := range s.Diags {
		if s.Diags[i].Sev == Error {
			return true
		}
	}
	return false
}

// Error implements the Go error interface: the one-line form of every
// diagnostic, or nil when none recorded.
func (s *Set) Error() error {
	if !s.HasErrors() {
		return nil
	}
	var parts []string
	for i := range s.Diags {
		if s.Diags[i].Sev == Error {
			parts = append(parts, s.Diags[i].Error())
		}
	}
	return fmt.Errorf("%s", strings.Join(parts, "\n"))
}

// RenderAll renders every diagnostic (errors and warnings) in order.
func (s *Set) RenderAll() string {
	var parts []string
	for i := range s.Diags {
		parts = append(parts, s.Diags[i].Render())
	}
	return strings.Join(parts, "\n")
}

// ErrorWithSource is a Go error carrying a whole diagnostic set, so API
// callers can recover rendering context from a failed Eval.
type ErrorWithSource struct {
	Set *Set
}

func (e *ErrorWithSource) Error() string { return e.Set.Error().Error() }

// NewErrorWithSource wraps a set as an error.
func NewErrorWithSource(s *Set) *ErrorWithSource { return &ErrorWithSource{Set: s} }
