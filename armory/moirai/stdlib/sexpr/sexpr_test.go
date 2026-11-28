package sexpr

import (
	"testing"

	"domscript/internal/runtime"
)

func TestParseAndEncode(t *testing.T) {
	v, err := parse("(define answer 42 \"hello\" (quote value))")
	if err != nil {
		t.Fatal(err)
	}
	got, err := encode(v, false)
	if err != nil {
		t.Fatal(err)
	}
	const want = "(define answer 42 \"hello\" (quote value))"
	if got != want {
		t.Fatalf("encode = %q, want %q", got, want)
	}
}

func TestParseQuoteAndComments(t *testing.T) {
	v, err := parse("; comment\n'(a #t nil)")
	if err != nil {
		t.Fatal(err)
	}
	want := runtime.ListOf([]runtime.Value{
		runtime.Tag("quote"),
		runtime.ListOf([]runtime.Value{runtime.Tag("a"), runtime.Bool(true), runtime.Unit()}),
	})
	if !runtime.Equals(v, want) {
		t.Fatalf("parse = %s, want %s", runtime.Display(v), runtime.Display(want))
	}
}

func TestParseRejectsInvalidInput(t *testing.T) {
	for _, src := range []string{"(", "\"unterminated", "one two"} {
		if _, err := parse(src); err == nil {
			t.Errorf("parse(%q) succeeded", src)
		}
	}
}

func TestEncodeRejectsUnrepresentableValue(t *testing.T) {
	if _, err := encode(runtime.TableValue(runtime.NewTable()), false); err == nil {
		t.Fatal("encode(table) succeeded")
	}
}
