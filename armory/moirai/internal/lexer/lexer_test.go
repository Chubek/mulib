package lexer

import (
	"fmt"
	"testing"

	"domscript/internal/diag"
	"domscript/internal/token"
)

// short maps token kinds to compact names so golden expectations stay readable.
var short = map[token.Kind]string{
	token.EOF: "$", token.Newline: "NL",
	token.Ident: "id", token.Int: "int", token.Float: "flt", token.String: "str",
	token.Tag: "tag", token.Selector: "sel",
	token.KwLet: "let", token.KwFn: "fn", token.KwIf: "if", token.KwElse: "else",
	token.KwWhile: "while", token.KwFor: "for", token.KwIn: "in", token.KwMatch: "match",
	token.KwReturn: "ret", token.KwBreak: "brk", token.KwContinue: "cnt", token.KwEnum: "enum",
	token.KwAnd: "and", token.KwOr: "or", token.KwNot: "not", token.KwTrue: "true",
	token.KwFalse: "false", token.KwOn: "on", token.KwModule: "module", token.KwProgram: "program",
	token.KwConst: "const",
	token.Assign: "=", token.Eq: "==", token.Neq: "!=", token.Lt: "<", token.Le: "<=",
	token.Gt: ">", token.Ge: ">=", token.Plus: "+", token.Minus: "-", token.Star: "*",
	token.Slash: "/", token.Percent: "%", token.Question: "?", token.Tilde: "~",
	token.DotDot: "..", token.Dot: ".", token.Dollar: "$",
	token.LParen: "(", token.RParen: ")", token.LBracket: "[", token.RBracket: "]",
	token.LBrace: "{", token.RBrace: "}", token.TableOpen: "%{", token.NodeOpen: "%<",
	token.NodeClose: "%>", token.Comma: ",", token.Semi: ";", token.Colon: ":",
	token.FatArrow: "=>", token.ThinArrow: "->",
	token.MOpenName: "(<", token.MCloseName: "(</", token.MGT: ">", token.MSelfClose: "/>",
	token.MAttrName: "attr", token.MEq: "=", token.MString: "mstr", token.MText: "text",
	token.MSpliceOpen: "{", token.MSpreadOpen: "{..", token.MSpliceClose: "}",
	token.MComment: "cmt",
}

func kinds(ts []token.Token) string {
	out := ""
	for i, t := range ts {
		if i > 0 {
			out += " "
		}
		if s, ok := short[t.Kind]; ok {
			out += s
		} else {
			out += t.Kind.String()
		}
		if t.Kind == token.MOpenName || t.Kind == token.MCloseName {
			out += ":" + t.Text
		}
	}
	return out
}

func check(t *testing.T, src, want string) []token.Token {
	t.Helper()
	diags := diag.NewSet("t.doml", src)
	ts := Scan(src, "t.doml", diags)
	if diags.HasErrors() {
		t.Errorf("src %q: unexpected diagnostics:\n%s", src, diags.RenderAll())
		return ts
	}
	if got := kinds(ts); got != want {
		t.Errorf("src %q:\n got %s\nwant %s", src, got, want)
	}
	return ts
}

func TestGoldenScalar(t *testing.T) {
	check(t, "let x = 10", "let id = int $")
	check(t, `let s = "hi"`, "let id = str $")
	check(t, "let t = :Red", "let id = tag $")
	check(t, `let q = $"div.card"`, "let id = sel $")
	check(t, "x = x + 1", "id = id + int $")
	check(t, "a and b or not c", "id and id or not id $")
	check(t, "0xFF 0b1010 0o17 1_000_000", "int int int int $")
	check(t, "3.14 1e-9 6.022e23", "flt flt flt $")
	check(t, "1..5", "int .. int $")
	check(t, "a $ s", "id $ id $")
	check(t, "f(a, b)", "id ( id , id ) $")
	check(t, "xs[0]", "id [ int ] $")
	check(t, "x.field", "id . id $")
	check(t, "()", "( ) $")
	check(t, "a == b != c <= d >= e < f > g",
		"id == id != id <= id >= id < id > id $")
	check(t, "%{ :a => 1 }", "%{ tag => int } $")
	check(t, `:"arbitrary text"`, "tag $")
}

func TestGoldenNewlines(t *testing.T) {
	// Newlines are emitted as terminators and collapse; suppressed inside
	// brackets and after binary operators.
	check(t, "let x = 10\nlet y = 20\nlet z = x +\n  y\nlet w = [1,\n 2]\n",
		"let id = int NL let id = int NL let id = id + id NL let id = [ int , int ] NL $")
	// A call paren does not swallow the next statement (continuation only
	// after operators or inside brackets).
	check(t, "let a = b\n(x)", "let id = id NL ( id ) $")
}

func TestGoldenMarkup(t *testing.T) {
	check(t, `%<div class="card"><h2>{title}</h2><!-- note --></div>%>`,
		"%< (<:div attr = mstr > (<:h2 > { id } (</:h2 cmt (</:div %> $")
}

func TestMarkupNestedFence(t *testing.T) {
	// A splice containing another node literal: nesting via the mode stack.
	check(t, `%<ul>{[%<li>{x}</li>%>]}</ul>%>`,
		"%< (<:ul > { [ %< (<:li > { id } (</:li %> ] } (</:ul %> $")
}

func TestMarkupSpreadAndText(t *testing.T) {
	check(t, "%<p>A paragraph {1 + 1} splice and {..extra} spread.</p>%>",
		"%< (<:p > text { int + int } text {.. id } text (</:p %> $")
}

func TestMarkupSelfClosingAndBoolAttr(t *testing.T) {
	check(t, `%<input disabled={flag} placeholder="name"/>%>`,
		"%< (<:input attr = { id } attr = mstr /> %> $")
}

func TestSpliceAttrValue(t *testing.T) {
	check(t, `%<div class={cls} data-id={id}>text</div>%>`,
		"%< (<:div attr = { id } attr = { id } > text (</:div %> $")
}

func TestFragmentFence(t *testing.T) {
	check(t, `%<{a}{b}%>`, "%< { id } { id } %> $")
}

func TestDiagnostics(t *testing.T) {
	cases := []string{
		"let x = @",                // invalid character
		`let s = "abc`,             // unterminated string
		"%<div>text",               // missing %>
		"%>",                       // stray fence end
		"let x = 0x",               // malformed based literal
		`let s = "a\q"`,            // unknown escape
		string([]byte{0xFF, 0xFE}), // invalid UTF-8
		// mismatched close-tags are a parser-level check, not a lexer one
	}
	for _, src := range cases {
		diags := diag.NewSet("t.doml", src)
		Scan(src, "t.doml", diags)
		if !diags.HasErrors() {
			t.Errorf("src %q: expected diagnostics, got none", src)
		}
	}
}

func TestTokenPositions(t *testing.T) {
	ts := check(t, "let x = 10\nlet y = x", "let id = int NL let id = id $")
	var let2 token.Token
	for _, tok := range ts {
		if tok.Kind == token.KwLet && tok.Pos.Line == 2 {
			let2 = tok
		}
	}
	if let2.Pos.Col != 1 || let2.Pos.Off != 11 {
		t.Errorf("second let position = %+v, want col 1 off 11", let2.Pos)
	}
	if fmt.Sprint(let2.Pos) != "t.doml:2:1" {
		t.Errorf("position string = %q", fmt.Sprint(let2.Pos))
	}
}
