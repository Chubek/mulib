package sema

import (
	"os"
	"path/filepath"
	"testing"

	"domscript/internal/diag"
	"domscript/internal/parser"
)

func check(t *testing.T, src string) *Info {
	t.Helper()
	diags := diag.NewSet("t.doml", src)
	prog := parser.Parse(src, "t.doml", diags)
	if diags.HasErrors() {
		t.Fatalf("parse errors for %q:\n%s", src, diags.RenderAll())
	}
	info := Check(prog, nil, nil, diags)
	if diags.HasErrors() {
		t.Fatalf("sema errors for %q:\n%s", src, diags.RenderAll())
	}
	if info == nil {
		t.Fatal("nil info without errors")
	}
	return info
}

func checkXST(t *testing.T, src string) *Info {
	t.Helper()
	diags := diag.NewSet("t.doml", src)
	prog := parser.Parse(src, "t.doml", diags, parser.ParseOptions{XST: true})
	if diags.HasErrors() {
		t.Fatalf("parse errors for %q:\n%s", src, diags.RenderAll())
	}
	info := Check(prog, nil, nil, diags)
	if diags.HasErrors() {
		t.Fatalf("sema errors for %q:\n%s", src, diags.RenderAll())
	}
	if info == nil {
		t.Fatal("nil info without errors")
	}
	return info
}

func checkErr(t *testing.T, src, wantSub string) {
	t.Helper()
	diags := diag.NewSet("t.doml", src)
	prog := parser.Parse(src, "t.doml", diags)
	if diags.HasErrors() {
		t.Fatalf("parse errors for %q:\n%s", src, diags.RenderAll())
	}
	Check(prog, nil, nil, diags)
	if !diags.HasErrors() {
		t.Fatalf("src %q: expected sema error, got none", src)
	}
	if wantSub != "" && !contains(diags.RenderAll(), wantSub) {
		t.Fatalf("src %q: missing %q in:\n%s", src, wantSub, diags.RenderAll())
	}
}

func contains(s, sub string) bool {
	return len(sub) == 0 || (len(s) >= len(sub) && indexOf(s, sub) >= 0)
}

func indexOf(s, sub string) int {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return i
		}
	}
	return -1
}

func TestPositiveResolution(t *testing.T) {
	check(t, "let x = 10\nlet y = x + 1\nPrint(y)")
	check(t, "fn add(a, b) { a + b }\nlet s = add(1, 2)")
	check(t, "let xs = [1, 2]\nlet a = Unwrap(Get(xs, 0))")
	check(t, "let r = 1..5\nlet i = Iota(3)")
	check(t, "fn outer() { fn inner(v) { v + 1 }\ninner }\nlet f = outer()")
	check(t, "let t = %{ :a => 1 }\nlet v = t.a")
	// Shadowing across scopes.
	check(t, "let x = 1\nif true { let x = 2\nPrint(x) }\nPrint(x)")
	// Mutual recursion resolves via hoisting.
	check(t, "fn is_even(n) { if n == 0 { true } else { is_odd(n - 1) } }\nfn is_odd(n) { if n == 0 { false } else { is_even(n - 1) } }")
	// Match arm bindings visible in arm body only.
	check(t, `match Get([1], 0) { :Some[v] => Print(v) :None => Print(0) }`)
	// Effect result in follower statement.
	check(t, "let u = \"k\"\n~ Fetch(u)\nmatch ~ { :Success[?] => 1 }")
	check(t, "let a = CreateElement(:div)\nlet b = CreateElement(:p)\nRemoveChild(a, b) ~ match { :Error[?] => 1 }")
	// on handler.
	check(t, "on document click(e) { Print(e) }")
	// Loop variable.
	check(t, "for x in [1, 2] { Print(x) }")
	checkXST(t, "dialect \"sql-dsl\"\nextend using parser { let x = 1 }")
}

func TestNegativeResolution(t *testing.T) {
	checkErr(t, "Print(undeclared_name)", "undefined name")
	checkErr(t, "x = 5", "undeclared")
	checkErr(t, "Len = 5", "builtin")
	// ~ outside a follower.
	checkErr(t, "let r = ~", "only bound inside")
	checkErr(t, "match ~ { _ => 1 }", "only bound inside")
	// ~ not visible inside a function literal in the follower.
	checkErr(t, "~ f()\nlet g = fn() { ~ }", "only bound inside")
	// Pattern issues.
	checkErr(t, "match x { [a, a] => 1 }", "more than once")
	checkErr(t, "break", "outside of a loop")
	checkErr(t, "continue", "outside of a loop")
	checkErr(t, "return 5", "outside of a function")
}

func TestExtraBuiltins(t *testing.T) {
	diags := diag.NewSet("t.doml", "HostFn(1)")
	prog := parser.Parse("HostFn(1)", "t.doml", diags)
	if diags.HasErrors() {
		t.Fatal(diags.RenderAll())
	}
	if info := Check(prog, []string{"HostFn"}, nil, diags); info == nil || diags.HasErrors() {
		t.Fatalf("host builtin unresolved:\n%s", diags.RenderAll())
	}
}

// TestExamplesCheck runs the acceptance examples through sema.
func TestExamplesCheck(t *testing.T) {
	for _, name := range []string{
		"astar.doml", "bfs.doml", "knapsack.doml", "mergesort.doml",
		"quicksort.doml", "sudoko.doml", "tictactoe.doml", "todo_widget.doml",
	} {
		src, err := os.ReadFile(filepath.Join("..", "..", "examples", name))
		if err != nil {
			t.Fatal(err)
		}
		diags := diag.NewSet(name, string(src))
		prog := parser.Parse(string(src), name, diags)
		if diags.HasErrors() {
			t.Errorf("%s: parse:\n%s", name, diags.RenderAll())
			continue
		}
		if Check(prog, nil, nil, diags) == nil {
			t.Errorf("%s: sema:\n%s", name, diags.RenderAll())
		}
	}
}
