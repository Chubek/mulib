package transpile

import (
	"strings"
	"testing"
)

func TestJavaScriptCoreSyntax(t *testing.T) {
	got, err := JavaScript(`
let add = fn(a, b) { return a + b }
let values = [1, 2, 3]
if true { add(1, 2) } else { 0 }
`, "test.doml")
	if err != nil {
		t.Fatal(err)
	}
	for _, want := range []string{
		"let add = function(a, b)",
		"return (a + b);",
		"let values = [1, 2, 3];",
		"if (true)",
	} {
		if !strings.Contains(got, want) {
			t.Errorf("output missing %q:\n%s", want, got)
		}
	}
}

func TestJavaScriptRejectsInvalidSource(t *testing.T) {
	if _, err := JavaScript("let =", "test.doml"); err == nil {
		t.Fatal("expected syntax error")
	}
}
