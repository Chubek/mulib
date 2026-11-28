package parser

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"domscript/internal/diag"
)

func parseOK(t *testing.T, src string) string {
	t.Helper()
	diags := diag.NewSet("t.doml", src)
	prog := Parse(src, "t.doml", diags)
	if diags.HasErrors() {
		t.Fatalf("src %q: unexpected diagnostics:\n%s", src, diags.RenderAll())
	}
	return Dump(prog)
}

func parseOKXST(t *testing.T, src string) string {
	t.Helper()
	diags := diag.NewSet("t.doml", src)
	prog := Parse(src, "t.doml", diags, ParseOptions{XST: true})
	if diags.HasErrors() {
		t.Fatalf("src %q: unexpected diagnostics:\n%s", src, diags.RenderAll())
	}
	return Dump(prog)
}

func parseErr(t *testing.T, src string, wantSub string) {
	t.Helper()
	diags := diag.NewSet("t.doml", src)
	Parse(src, "t.doml", diags)
	if !diags.HasErrors() {
		t.Errorf("src %q: expected diagnostics, got none", src)
		return
	}
	if wantSub != "" && !strings.Contains(diags.RenderAll(), wantSub) {
		t.Errorf("src %q: diagnostics missing %q:\n%s", src, wantSub, diags.RenderAll())
	}
}

func TestGoldenStatements(t *testing.T) {
	cases := []struct{ src, want string }{
		{"let x = 10", "(let x 10)"},
		{"const x = 10", "(let x 10)"},
		{"x = x + 1", "(assign x (+ x 1))"},
		{"fn add(a, b) { a + b }", "(fn add (fn (a b) { (+ a b) }))"},
		{"let mul = fn(a, b) { a * b }", "(let mul (fn (a b) { (* a b) }))"},
		{"fn classes(base, rest..) { all }", "(fn classes (fn (base rest..) { all }))"},
		{"if x { 1 } else { 2 }", "(if x { 1 } else { 2 })"},
		{"if a { 1 } else if b { 2 } else { 3 }", "(if a { 1 } else (if b { 2 } else { 3 }))"},
		{"while c { f() }", "(while c { (f) })"},
		{"for x in xs { Print(x) }", "(for x in xs { (Print x) })"},
		{"return", "(return)"},
		{"return 5", "(return 5)"},
		{"break", "(break)"},
		{"continue", "(continue)"},
		{"let t = %{ :a => 1, \"k\" => 2 }", `(let t %{ (:a => 1) ("k" => 2)})`},
		{"let e = enum { A = Iota, B = Iota }", "(let e (enum (A Iota) (B Iota)))"},
		{"let o = ?42", "(let o (? 42))"},
		{"let n = ?", "(let n (?))"},
		{"let r = a..b", "(let r (.. a b))"},
		{"let q = node $ sel", "(let q ($ node sel))"},
		{"let neg = -x", "(let neg (- x))"},
		{"let b = not x", "(let b (not x))"},
		{"xs[0] = 5", "(assign (index xs 0) 5)"},
		{"t.field = 5", "(assign (field t field) 5)"},
		{"let u = ()", "(let u ())"},
		{"let s = :Point[3, 4]", "(let s :Point[3 4])"},
		{"let l = :Leaf[]", "(let l :Leaf[])"},
		{"let f = quicksort(data)", "(let f (quicksort data))"},
		{"module std.io", "(module std.io)"},
		{"program fizz", "(program fizz)"},
	}
	for _, tc := range cases {
		if got := parseOK(t, tc.src); got != tc.want {
			t.Errorf("src %q:\n got %s\nwant %s", tc.src, got, tc.want)
		}
	}
}

func TestGoldenPrecedence(t *testing.T) {
	cases := []struct{ src, want string }{
		{"a + b * c", "(+ a (* b c))"},
		{"(a + b) * c", "(* (+ a b) c)"},
		{"a or b and c == d < e $ f..g", "(or a (and b (== c (< d ($ e (.. f g))))))"},
		{"1 + 2 + 3", "(+ (+ 1 2) 3)"},
		{"a - -b", "(- a (- b))"},
		{"not a == b", "(== (not a) b)"},
		{"f(a)(b)", "((f a) b)"},
		{"a.b.c", "(field (field a b) c)"},
		{"x[i][j]", "(index (index x i) j)"},
	}
	for _, tc := range cases {
		if got := parseOK(t, tc.src); got != tc.want {
			t.Errorf("src %q:\n got %s\nwant %s", tc.src, got, tc.want)
		}
	}
}

func TestEffectForms(t *testing.T) {
	// Prefix form with follower statement.
	if got := parseOK(t, "~ Fetch(url)\nmatch ~ { :Error[?m] => 1 }"); got !=
		"(effect (Fetch url) (match (~) (:Error[(? m)] => 1)))" {
		t.Errorf("prefix effect: %s", got)
	}
	// Infix form shown in Section 10.
	if got := parseOK(t, "RemoveChild(list, item) ~ match { :Success[?] => Print(1) }"); got !=
		"(effect (RemoveChild list item) (match (~) (:Success[(?)] => (Print 1))))" {
		t.Errorf("infix effect: %s", got)
	}
	// Expression-position effect operand.
	if got := parseOK(t, "let r = ~ Fetch(u)"); got != "(let r (~ (Fetch u)))" {
		t.Errorf("expr effect: %s", got)
	}
	// Nested ~ binds the innermost for its follower.
	parseOK(t, "~ ~ f()\nmatch ~ { _ => 1 }")
}

func TestGoldenMatch(t *testing.T) {
	if got := parseOK(t, `match shape {
        :Circle[_, r] => "circle"
        :Rect[w, h] => { "rect" }
        [a, b, ..rest] => a
        _ => :Error[:Unknown]
    }`); got != `(match shape (:Circle[_ r] => "circle") (:Rect[w h] => { "rect" }) ([a b ..rest] => a) (_ => :Error[:Unknown]))` {
		t.Errorf("match: %s", got)
	}
	// Optional patterns on results.
	if got := parseOK(t, "match ~ { :Success[?] => 1 }"); got != "(match (~) (:Success[(?)] => 1))" {
		t.Errorf("opt pattern: %s", got)
	}
	if got := parseOK(t, "match x { :Some[v] => v }"); got != "(match x (:Some[v] => v))" {
		t.Errorf("some pattern: %s", got)
	}
	if got := parseOK(t, "match x { :None => 0 }"); got != "(match x (:None => 0))" {
		t.Errorf("none pattern: %s", got)
	}
}

func TestGoldenNodeLit(t *testing.T) {
	if got := parseOK(t, `%<div class="card" data-id={id}><h2>{title}</h2>{..extra}</div>%>`); got !=
		`%<`+"<div class=\"card\" data-id={id}>"+`<h2>{title}</h2>{..extra}`+"</div>"+`%>` {
		t.Errorf("node lit: %s", got)
	}
	if got := parseOK(t, "%<{a}{b}%>"); got != `%<{a}{b}%>` {
		t.Errorf("fragment: %s", got)
	}
	if got := parseOK(t, "%<input disabled={flag}/>%>"); got != `%<`+`<input disabled={flag}/>`+`%>` {
		t.Errorf("self-close: %s", got)
	}
	if got := parseOK(t, "%<p>text <!-- c --> end</p>%>"); got != `%<`+`<p>`+`"text "`+`<!-- c -->`+`" end"`+`</p>`+`%>` {
		t.Errorf("comment/text: %s", got)
	}
}

func TestXSTStatements(t *testing.T) {
	if got := parseOKXST(t, "dialect \"sql-dsl\"\nextend using parser { let x = 1 }"); got !=
		"(dialect \"sql-dsl\")(extend using parser { let x = 1 })" {
		t.Fatalf("xst: %s", got)
	}
	parseErr(t, "dialect \"sql-dsl\"", "requires -X")
}

func TestModuleAndDottedImport(t *testing.T) {
	if got := parseOK(t, "import std.io"); got != "(import std.io)" {
		t.Errorf("import: got %s want %s", got, "(import std.io)")
	}
	if got := parseOK(t, "import std.io as s"); got != "(import std.io as s)" {
		t.Errorf("dotted import: got %s want %s", got, "(import std.io as s)")
	}
	if got := parseOK(t, "import std.io : s"); got != "(import std.io as s)" {
		t.Errorf("dotted import with colon: got %s want %s", got, "(import std.io as s)")
	}
	parseErr(t, "import io", "requires a dotted namespace")
	if got := parseOK(t, "module std.io"); got != "(module std.io)" {
		t.Errorf("module declaration: got %s want %s", got, "(module std.io)")
	}
	parseErr(t, "program foo.bar", "cannot be namespaced")
}

func TestOnStatement(t *testing.T) {
	if got := parseOK(t, "on button click(e) { Print(1) }"); got != "(on button click { (Print 1) })" {
		t.Errorf("on: %s", got)
	}
	if got := parseOK(t, "let sub = on input keydown(e) { 1 }"); got != "(let sub (on input keydown { 1 }))" {
		t.Errorf("on expr: %s", got)
	}
}

func TestErrorRecovery(t *testing.T) {
	// Both errors surface in one pass.
	diags := diag.NewSet("t.doml", "let = 5\nlet y =\nlet z = 3\n")
	Parse("let = 5\nlet y =\nlet z = 3\n", "t.doml", diags)
	if !diags.HasErrors() {
		t.Fatal("expected diagnostics")
	}
	n := 0
	for _, d := range diags.Diags {
		if d.Sev.String() == "error" {
			n++
		}
	}
	if n < 2 {
		t.Errorf("expected at least 2 errors, got %d:\n%s", n, diags.RenderAll())
	}

	parseErr(t, "let x 10", "expected '='")
	parseErr(t, "%<div>text</span>%>", "mismatched close tag")
	parseErr(t, "if x { 1 ", "unclosed block")
	parseErr(t, "let x = 1 +", "expected expression")
	parseErr(t, "f(a, ", "")
	parseErr(t, "for x of xs { }", "'in'")
	parseErr(t, "match x { [a,,b] => 1 }", "expected pattern")
}

func TestTrailingCommas(t *testing.T) {
	parseOK(t, "f(a, b,)")
	parseOK(t, "let xs = [1, 2, 3,]")
	parseOK(t, "let t = %{ :a => 1, }")
	parseOK(t, "fn f(a, b,) { 1 }")
	parseOK(t, "match x { :Tag[a,] => 1 }")
}

func TestIfAndMatchAsExpressions(t *testing.T) {
	// Required by the examples: parenthesized if inside an expression.
	if got := parseOK(t, "let v = (if dx < 0 { -dx } else { dx }) + 1"); got != "(let v (+ (if (< dx 0) { (- dx) } else { dx }) 1))" {
		t.Errorf("if expr: %s", got)
	}
	if got := parseOK(t, "let b = match Get(t, k) { :None => true, :Some[old] => false }"); got !=
		"(let b (match (Get t k) (:None => true) (:Some[old] => false)))" {
		t.Errorf("match expr: %s", got)
	}
}

// TestExamplesParse runs the acceptance-test examples through the parser.
func TestExamplesParse(t *testing.T) {
	examples := []string{
		"astar.doml", "bfs.doml", "knapsack.doml", "mergesort.doml",
		"quicksort.doml", "sudoko.doml", "tictactoe.doml", "todo_widget.doml",
	}
	for _, name := range examples {
		path := filepath.Join("..", "..", "examples", name)
		src, err := os.ReadFile(path)
		if err != nil {
			t.Fatalf("reading %s: %v", name, err)
		}
		diags := diag.NewSet(name, string(src))
		Parse(string(src), name, diags)
		if diags.HasErrors() {
			t.Errorf("%s: parse errors:\n%s", name, diags.RenderAll())
		}
	}
}
