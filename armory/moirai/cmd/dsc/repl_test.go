package main

import (
	"path/filepath"
	"testing"
)

func TestREPLConfigRoundTrip(t *testing.T) {
	p := filepath.Join(t.TempDir(), "REPL.ini")
	want := replConfig{Theme: "compact", AutoComplete: false, Highlight: true, History: "hist"}
	if err := saveREPLConfig(p, want); err != nil {
		t.Fatal(err)
	}
	got := loadREPLConfig(p)
	if got != want {
		t.Fatalf("config=%+v want %+v", got, want)
	}
}

func TestDefaultREPLConfigUsesXDG(t *testing.T) {
	t.Setenv("XDG_CONFIG_HOME", "/tmp/example-config")
	if got, want := defaultREPLConfig(), filepath.Join("/tmp/example-config", "domscript", "REPL.ini"); got != want {
		t.Fatalf("path=%q want %q", got, want)
	}
}

func TestCompleteREPLDirectiveIncludesConfig(t *testing.T) {
	matches := completeREPLDirective("%config")
	if len(matches) != 1 || matches[0] != "%config" {
		t.Fatalf("completion = %v, want [%%config]", matches)
	}
}

func TestCompletionPrefix(t *testing.T) {
	if got, want := completionPrefix("let va"), "va"; got != want {
		t.Fatalf("prefix=%q want %q", got, want)
	}
	if got, want := completionPrefix("%h"), "%h"; got != want {
		t.Fatalf("directive prefix=%q want %q", got, want)
	}
}

func TestHighlightREPLColors(t *testing.T) {
	colors := highlightREPLColors(`let x = "hi" // ok`)
	if len(colors) == 0 {
		t.Fatal("no colors")
	}
	if colors[0] != replColorMagenta {
		t.Fatalf("keyword color=%d want %d", colors[0], replColorMagenta)
	}
}
