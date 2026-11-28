// Package token defines the lexical tokens of Domscript, including the
// markup-mode tokens produced between %< ... %> fences (Section 19).
package token

import "fmt"

// Pos is a source position. Off is the byte offset from the start of the
// file; Line and Col are 1-based (Col counts bytes, not runes).
type Pos struct {
	File string
	Line int
	Col  int
	Off  int
}

func (p Pos) String() string {
	if p.File == "" {
		return fmt.Sprintf("%d:%d", p.Line, p.Col)
	}
	return fmt.Sprintf("%s:%d:%d", p.File, p.Line, p.Col)
}

// Kind enumerates every terminal of the Section 19 grammar plus the markup
// terminals the lexer produces inside a %< ... %> fence.
type Kind uint8

const (
	EOF     Kind = iota
	Newline      // significant line break (statement terminator candidate)

	Ident
	Int
	Float
	String
	Char     // #\x, #\newline, ... (Scheme-like character literal)
	Regex    // /pattern/ (grammar lexeme pattern; also usable as expression)
	Tag      // :Name or :"arbitrary text"
	Selector // $"css-like-selector"

	// Keywords (Section 3.5).
	KwConst
	KwLet
	KwFn
	KwIf
	KwElse
	KwWhile
	KwFor
	KwIn
	KwMatch
	KwReturn
	KwBreak
	KwContinue
	KwEnum
	KwAnd
	KwOr
	KwNot
	KwTrue
	KwFalse
	KwNull
	KwModule
	KwProgram
	KwOn
	KwImport
	KwForm
	KwAs

	// Operators and punctuation (Section 3.7).
	Assign    // =
	PlusAssign  // +=
	MinusAssign // -=
	StarAssign  // *=
	SlashAssign // /=
	PercentAssign // %=
	Eq        // ==
	Neq       // !=
	Lt        // <
	Le        // <=
	Gt        // >
	Ge        // >=
	Plus      // +
	Minus     // -
	Star      // *
	Slash     // /
	Percent   // %
	Question  // ?
	Tilde     // ~
	Pipe      // |  (grammar alternative separator)
	Define    // ::= (grammar production separator)
	DotDot    // ..
	Dot       // .
	Dollar    // $
	LParen    // (
	RParen    // )
	LBracket  // [
	RBracket  // ]
	LBrace    // {
	RBrace    // }
	TableOpen // %{
	NodeOpen  // %<
	NodeClose // %>
	Comma     // ,
	Semi      // ;
	Colon     // :
	FatArrow  // =>
	ThinArrow // ->  (reserved by Section 3.7; no grammar production uses it)

	// Markup-mode tokens (only produced inside %< ... %>).
	MOpenName    // <name          Text = element name
	MCloseName   // </name         Text = element name
	MGT          // >              end of open tag (children follow)
	MSelfClose   // />
	MAttrName    // name           attribute name (before '=')
	MEq          // =              attribute equals
	MString      // "literal"      attribute literal value
	MText        // text run       Text = verbatim content
	MSpliceOpen  // {              splice (expression mode resumes)
	MSpreadOpen  // {..            spread splice
	MSpliceClose // }              end of splice (markup mode resumes)
	MComment     // <!-- ... -->   Text = comment body
)

var kindNames = map[Kind]string{
	EOF: "end of file", Newline: "newline",
	Ident: "identifier", Int: "int literal", Float: "float literal",
	String: "string literal", Char: "character literal", Regex: "regex literal",
	Tag: "tag literal", Selector: "selector literal",
	KwLet: "let", KwFn: "fn", KwIf: "if", KwElse: "else", KwWhile: "while",
	KwFor: "for", KwIn: "in", KwMatch: "match", KwReturn: "return",
	KwBreak: "break", KwContinue: "continue", KwEnum: "enum", KwAnd: "and",
	KwOr: "or", KwNot: "not", KwTrue: "true", KwFalse: "false", KwNull: "null",
	KwModule: "module", KwProgram: "program", KwOn: "on", KwConst: "const",
	KwImport: "import", KwForm: "form", KwAs: "as",
	Assign: "=", PlusAssign: "+=", MinusAssign: "-=", StarAssign: "*=",
	SlashAssign: "/=", PercentAssign: "%=",
	Eq: "==", Neq: "!=", Lt: "<", Le: "<=", Gt: ">", Ge: ">=",
	Plus: "+", Minus: "-", Star: "*", Slash: "/", Percent: "%",
	Question: "?", Tilde: "~", Pipe: "|", Define: "::=",
	DotDot: "..", Dot: ".", Dollar: "$",
	LParen: "(", RParen: ")", LBracket: "[", RBracket: "]",
	LBrace: "{", RBrace: "}", TableOpen: "%{", NodeOpen: "%<", NodeClose: "%>",
	Comma: ",", Semi: ";", Colon: ":", FatArrow: "=>", ThinArrow: "->",
	MOpenName: "<name", MCloseName: "</name", MGT: ">", MSelfClose: "/>",
	MAttrName: "attribute name", MEq: "=", MString: "markup string",
	MText: "markup text", MSpliceOpen: "{", MSpreadOpen: "{..",
	MSpliceClose: "}", MComment: "markup comment",
}

func (k Kind) String() string {
	if s, ok := kindNames[k]; ok {
		return s
	}
	return fmt.Sprintf("token(%d)", uint8(k))
}

// Token is one lexeme with its source span.
type Token struct {
	Kind Kind
	Text string // literal text: identifier name, string contents, tag name, ...
	Pos  Pos
	End  Pos // position just past the lexeme
}

func (t Token) String() string {
	if t.Text != "" {
		return fmt.Sprintf("%s %q", t.Kind, t.Text)
	}
	return t.Kind.String()
}

var keywords = map[string]Kind{
	"let": KwLet, "fn": KwFn, "if": KwIf, "else": KwElse,
	"while": KwWhile, "for": KwFor, "in": KwIn, "match": KwMatch,
	"return": KwReturn, "break": KwBreak, "continue": KwContinue,
	"enum": KwEnum, "and": KwAnd, "or": KwOr, "not": KwNot,
	"module": KwModule, "program": KwProgram, "const": KwConst,
	"true": KwTrue, "false": KwFalse, "null": KwNull, "on": KwOn, "import": KwImport,
	"form": KwForm, "as": KwAs,
}

// Keyword resolves an identifier-looking word to its keyword kind, or
// Ident if it is not reserved.
func Keyword(word string) Kind {
	if k, ok := keywords[word]; ok {
		return k
	}
	return Ident
}

// CanEndExpr reports whether a token of this kind can legitimately end an
// expression. The lexer suppresses a line break that follows a token which
// cannot (Section 3.3: a statement continues onto the next line if the line
// ends inside an open bracket or immediately after a binary operator).
func (k Kind) CanEndExpr() bool {
	switch k {
	case Ident, Int, Float, String, Char, Regex, Tag, Selector,
		KwTrue, KwFalse, KwNull,
		RParen, RBracket, RBrace, NodeClose, Tilde, Question,
		MSpliceClose, KwReturn, KwBreak, KwContinue:
		return true
	}
	return false
}
