// Package runtime: grammar.go implements the libglr-backed grammar,
// absyn (ASDL), reader, and typed-error value model.
//
// The design mirrors dependencies/libglr/include/glr/grammar.h: a Grammar
// owns symbols (terminals from `keyword`/`lexeme`, nonterminals from
// `rule`) and productions (rule alternatives with EBNF `?`/`*`/`+`
// suffixes and `(...)` groups). Scannerless terminals carry either a
// literal spelling (`keyword NULL ::= "null"`) or a POSIX-extended pattern
// (`lexeme NUMBER ::= /.../ `, matched with Go RE2 like libglr's
// scannerless patterns) or a Domscript custom lexer
// (`lexeme STRING ::= fn(reader) {...}` invoked with a *Reader).
//
// Absyn (`absyn tJSON { ... }`, also spelled `asdl`) mirrors libglr's
// S-expression AST building (query.h): constructors become tagged lists
// (`:Null[]`, `:Bool[true]`, ...) so existing match/IsSome machinery works.
package runtime

import (
	"fmt"
	"regexp"
	"strings"
	"unicode/utf8"
)

// ------------------------------------------------------------------ regex

// Regex is a compiled `/pattern/` literal.
type Regex struct {
	Pattern string
	Re      *regexp.Regexp
}

// NewRegex compiles pattern (anchored at the current position by callers).
func NewRegex(pattern string) (*Regex, error) {
	// Anchor at the start; callers slice input at Pos and take the leftmost
	// match. Wrap in (?:...) so `a|b` anchors as a unit.
	re, err := regexp.Compile("^(?:" + pattern + ")")
	if err != nil {
		return nil, err
	}
	return &Regex{Pattern: pattern, Re: re}, nil
}

// RegexOf wraps r as a :Regex value.
func RegexOf(r *Regex) Value {
	return Value{kind: KindRegex, s: r.Pattern, ref: r}
}

func (v Value) Regex() *Regex { return v.ref.(*Regex) }

// ----------------------------------------------------------------- grammar

// LexemeKind classifies a lexeme definition.
type LexemeKind uint8

const (
	LexLiteral LexemeKind = iota // keyword literal (also used for keywords)
	LexPattern                   // /regex/
	LexCustom                    // fn(reader)
)

// Lexeme is one `keyword`/`lexeme` terminal.
type Lexeme struct {
	Name    string
	Kind    LexemeKind
	Literal string // keyword spelling
	Pattern string // regex source
	Re      *regexp.Regexp
	Fn      *Fn // custom lexer (Domscript function value)
}

// RuleElemKind mirrors ast.RuleElemKind without importing ast.
type RuleElemKind uint8

const (
	RuleRef RuleElemKind = iota
	RuleLit
	RuleChar
	RuleRegex
	RuleGroup
)

// RuleElem is one element of a rule alternative.
type RuleElem struct {
	Kind    RuleElemKind
	Name    string // Ref name or Lit literal
	Char    string // Char single character
	Pattern string // Regex pattern
	Suffix  byte   // 0, '?', '*', '+'
	Group   [][]RuleElem
}

// Rule is one `rule NAME ::= alt1 | alt2 | ...;`.
type Rule struct {
	Name string
	Alts [][]RuleElem
}

// Grammar is one `grammar Name { ... }` declaration.
type Grammar struct {
	Name     string
	Keywords map[string]*Lexeme // by terminal name
	Lexemes  map[string]*Lexeme
	Rules    map[string]*Rule
	Order    []string // rule declaration order (start = first rule)
}

// GrammarOf wraps g as a :Grammar value.
func GrammarOf(g *Grammar) Value {
	return Value{kind: KindGrammar, s: g.Name, ref: g}
}

func (v Value) Grammar() *Grammar { return v.ref.(*Grammar) }

// StartRule returns the first declared rule (the start symbol).
func (g *Grammar) StartRule() *Rule {
	if g == nil || len(g.Order) == 0 {
		return nil
	}
	return g.Rules[g.Order[0]]
}

// ------------------------------------------------------------------- absyn

// AbsynField is `type name` in a constructor.
type AbsynField struct {
	Type string
	Name string
}

// AbsynCtor is one constructor (`Null`, `Bool(bool value)`, ...).
type AbsynCtor struct {
	Name   string
	Fields []AbsynField
}

// AbsynType is `name ::= ctor1 | ctor2 | ...;`.
type AbsynType struct {
	Name  string
	Ctors []AbsynCtor
}

// Absyn is one `absyn Name { ... }` declaration.
type Absyn struct {
	Name  string
	Types map[string]*AbsynType
	Order []string
}

// AbsynOf wraps a as a :Absyn value.
func AbsynOf(a *Absyn) Value {
	return Value{kind: KindAbsyn, s: a.Name, ref: a}
}

func (v Value) Absyn() *Absyn { return v.ref.(*Absyn) }

// MakeAbsynValue builds a constructor value: `:Name[args...]`.
func MakeAbsynValue(ctor string, args []Value) Value {
	return TaggedOf(ctor, args)
}

// ------------------------------------------------------------------ parser

// Parser wraps a Grammar for DeriveParser/CreateReader.
type Parser struct {
	Grammar *Grammar
	Trivia  string // skipped literal between tokens (default whitespace)
}

// ParserOf wraps p as a :Parser value.
func ParserOf(p *Parser) Value {
	name := ""
	if p.Grammar != nil {
		name = p.Grammar.Name
	}
	return Value{kind: KindParser, s: name, ref: p}
}

func (v Value) Parser() *Parser { return v.ref.(*Parser) }

// ------------------------------------------------------------------ reader

// Reader is the mutable cursor handed to custom lexeme functions and to
// CreateReader results. Input is the full text; Pos is the byte offset.
type Reader struct {
	Input string
	Pos   int
}

// ReaderOf wraps r as a :Reader value.
func ReaderOf(r *Reader) Value {
	return Value{kind: KindReader, ref: r}
}

func (v Value) Reader() *Reader { return v.ref.(*Reader) }

// Rest returns the unread suffix.
func (r *Reader) Rest() string {
	if r.Pos < 0 {
		r.Pos = 0
	}
	if r.Pos >= len(r.Input) {
		return ""
	}
	return r.Input[r.Pos:]
}

// Head returns the character at Pos as a one-character string.
func (r *Reader) Head() (string, bool) {
	rest := r.Rest()
	if rest == "" {
		return "", false
	}
	_, size := utf8.DecodeRuneInString(rest)
	if size <= 0 {
		return "", false
	}
	return rest[:size], true
}

// Peek returns the character at Pos+off (off in runes).
func (r *Reader) Peek(off int) (string, bool) {
	rest := r.Rest()
	if rest == "" {
		return "", false
	}
	i := 0
	for n := 0; n < off; n++ {
		_, size := utf8.DecodeRuneInString(rest[i:])
		if size <= 0 || i+size >= len(rest) {
			return "", false
		}
		i += size
	}
	if i >= len(rest) {
		return "", false
	}
	_, size := utf8.DecodeRuneInString(rest[i:])
	return rest[i : i+size], true
}

// Advance moves Pos forward by one rune; reports whether there was input.
func (r *Reader) Advance() bool {
	rest := r.Rest()
	if rest == "" {
		return false
	}
	_, size := utf8.DecodeRuneInString(rest)
	r.Pos += size
	return true
}

// ------------------------------------------------------------- typed errors

// TypedError is a Go error carrying a Domscript tagged error value
// (`:IOError["..."]`, `:ParseError["..."]`, ...). Effect boundaries catch
// it and re-expose the tagged value (instead of collapsing to `:Error`).
type TypedError struct {
	Tag string
	Msg string
	Val Value
}

func (e *TypedError) Error() string {
	if e.Msg != "" {
		return e.Tag + ": " + e.Msg
	}
	return e.Tag
}

// NewTypedError builds `:Tag[msg]` (or `:Tag[]` when msg is empty) and its
// Go carrier.
func NewTypedError(tag, msg string) (Value, *TypedError) {
	var v Value
	if msg == "" {
		v = TaggedOf(tag, nil)
	} else {
		v = TaggedOf(tag, []Value{Str(msg)})
	}
	return v, &TypedError{Tag: tag, Msg: msg, Val: v}
}

// IsErrorTag reports whether tag is an error tag (`Error` or `*Error`).
func IsErrorTag(tag string) bool {
	return tag == "Error" || (strings.HasSuffix(tag, "Error") && len(tag) > 5)
}

// IsErrorValue reports whether v is `:Error[...]` or `:XError[...]`.
func IsErrorValue(v Value) bool {
	if v.Kind() != KindList {
		return false
	}
	return IsErrorTag(v.ListTag())
}

// IsSuccessValue reports whether v is `:Success[...]`.
func IsSuccessValue(v Value) bool {
	if v.Kind() != KindList {
		return false
	}
	return v.ListTag() == "Success"
}

// ErrorMessage extracts a display message from an error value.
func ErrorMessage(v Value) string {
	if v.Kind() != KindList {
		return Display(v)
	}
	elems := v.List().Elems
	if len(elems) == 0 {
		return v.ListTag()
	}
	if len(elems) == 1 {
		return Display(elems[0])
	}
	parts := make([]string, len(elems))
	for i, e := range elems {
		parts[i] = Display(e)
	}
	return strings.Join(parts, ", ")
}

// TypedErrorValue converts a Go error to the tagged value an effect
// boundary should bind to `~`: typed errors keep their tag, everything
// else becomes `:Error[msg]`.
func TypedErrorValue(err error) Value {
	if te, ok := err.(*TypedError); ok {
		return te.Val
	}
	return TaggedOf("Error", []Value{Str(err.Error())})
}

// ErrorConstructors lists the `*Error(msg)` natives installed by the
// prelude (plus generic `Error`).
var ErrorConstructors = []string{
	"Error",
	"IOError",
	"FileError",
	"ValueError",
	"TypeError",
	"ParseError",
	"GrammarError",
	"LexError",
	"SyntaxError",
	"IndexError",
	"KeyError",
	"AttributeError",
	"RuntimeError",
	"AssertionError",
}

// RaiseError returns a Go error that carries v as a typed error. Plain
// strings become `:Error[msg]`; tagged error values keep their tag.
func RaiseError(v Value) error {
	if v.Kind() == KindList && IsErrorTag(v.ListTag()) {
		return &TypedError{Tag: v.ListTag(), Msg: ErrorMessage(v), Val: v}
	}
	if v.Kind() == KindString {
		_, te := NewTypedError("Error", v.StrValue())
		return te
	}
	// Any other value is wrapped as `:Error[repr]`.
	wrapped := TaggedOf("Error", []Value{Str(Display(v))})
	return &TypedError{Tag: "Error", Msg: Display(v), Val: wrapped}
}

// MakeErrorValue builds `:Tag[payload]` for an error constructor call.
func MakeErrorValue(tag string, args []Value) Value {
	if len(args) == 0 {
		return TaggedOf(tag, nil)
	}
	if len(args) == 1 {
		return TaggedOf(tag, []Value{args[0]})
	}
	return TaggedOf(tag, args)
}

var _ = fmt.Sprintf
