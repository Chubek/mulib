// Package ast defines the abstract syntax tree of Domscript. The tree is
// faithful to the Section 19 grammar: no desugaring happens here — the
// parser produces what the user wrote, and lowering to IR happens later,
// so diagnostics and tooling see the source shape.
package ast

import "domscript/internal/token"

// Node is embedded by every AST node; Span covers the source extent.
type Node struct {
	Pos token.Pos
	End token.Pos
}

func (n *Node) Span() (token.Pos, token.Pos) { return n.Pos, n.End }

// SpanOf returns the extent of any node.
type Spanner interface{ Span() (token.Pos, token.Pos) }

// ------------------------------------------------------------------ program

type Program struct {
	Node
	Stmts []Stmt
}

type ProgramDecl struct {
	Node
	Name string
}

// ---------------------------------------------------------------- statements

type Stmt interface{ stmt() }

type Let struct {
	Node
	Name  string
	Value Expr
	// Mutable is false for `const` declarations and true for `let`.
	Mutable bool
}

type Module struct {
	Node
	Name string
}

// Import loads a library module into the global namespace. Alias is used by
// qualified imports (`import heap as h`); Library selects an alternate
// library root (`import heap form stdlib`). Symbols selects individual
// bindings (`import std.io : read_file, file_exists`); when non-empty only
// those names are bound instead of the whole export table.
type Import struct {
	Node
	Module  string
	Alias   string
	Library string
	Symbols []string
}

func (*Import) stmt() {}

// Dialect declares an XST dialect at the top of a file.
type Dialect struct {
	Node
	Name string
}

// Extend declares an opaque XST extension block.
type Extend struct {
	Node
	Using string
	Body  string
}

type Assign struct {
	Node
	Target Expr // Ident, Index, or Field
	Value  Expr
}

type ExprStmt struct {
	Node
	X Expr
}

// Effect is the `~` statement: capture the effect of Operand, bind the
// result to the reserved name `~` for exactly the following statement.
// Both spellings the spec shows parse to this node: prefix `~ expr`
// followed by the follower statement, and the infix form
// `RemoveChild(a, b) ~ match { ... }` of Section 10.
type Effect struct {
	Node
	Operand Expr
	Body    Stmt
}

type If struct {
	Node
	Cond Expr
	Then *Block
	Else Stmt // nil, *If, or *Block
}

type While struct {
	Node
	Cond Expr
	Body *Block
}

type For struct {
	Node
	Var  string
	Iter Expr
	Body *Block
}

type MatchArm struct {
	Node
	Pat  Pattern
	Body Expr // expression, or *Block
}

type Match struct {
	Node
	Scrutinee Expr
	Arms      []MatchArm
}

type On struct {
	Node
	Target Expr   // :Node or List of :Node
	Event  string // bare event name; denotes the Tag :name
	Params []Param
	Body   *Block
}

type Return struct {
	Node
	Value Expr // nil for bare `return`
}

type Break struct{ Node }
type Continue struct{ Node }

type BlockStmt struct {
	Node
	Block *Block
}

type Block struct {
	Node
	Stmts []Stmt
}

func (*Let) stmt()         {}
func (*Dialect) stmt()     {}
func (*Extend) stmt()      {}
func (*ProgramDecl) stmt() {}
func (*Module) stmt()      {}
func (*Assign) stmt()      {}
func (*ExprStmt) stmt()    {}
func (*Effect) stmt()      {}
func (*If) stmt()           {}
func (*While) stmt()       {}
func (*For) stmt()         {}
func (*Match) stmt()       {}
func (*On) stmt()          {}
func (*Return) stmt()      {}
func (*Break) stmt()       {}
func (*Continue) stmt()    {}
func (*BlockStmt) stmt()   {}
func (*GrammarDecl) stmt() {}
func (*AbsynDecl) stmt()   {}
func (*Block) stmt()       {} // bare block statement (Section 15)
func (*Block) expr()       {} // block as match-arm body (Section 16)
func (*On) expr()          {} // on yields a :Subscription in expr position (Section 11)

// --------------------------------------------------------------- expressions

type Expr interface{ expr() }

type Ident struct {
	Node
	Name string
}

type IntLit struct {
	Node
	Value int64
	Text  string
}

type FloatLit struct {
	Node
	Value float64
	Text  string
}

type StringLit struct {
	Node
	Value string // cooked value (escapes expanded, no interpolation)
	Raw   string // raw source between quotes (for tooling)
}

// InterpLit is a string with `$name` / `${expr}` interpolation. Parts
// alternate literal StringLit and interpolated Expr in source order;
// the compiler concatenates Display(part) for each part.
type InterpLit struct {
	Node
	Parts []Expr
}

// CharLit is a Scheme-like character literal (`#\,`, `#\newline`).
// Value holds the single character as a string (one rune).
type CharLit struct {
	Node
	Value string
}

// RegexLit is a `/pattern/` literal (grammar lexeme patterns and
// runtime regex values). Pattern excludes the delimiting slashes.
type RegexLit struct {
	Node
	Pattern string
}

type NullLit struct {
	Node
}

type BoolLit struct {
	Node
	Value bool
}

type UnitLit struct{ Node }

type TagLit struct {
	Node
	Name string // without the leading ':'
}

type SelectorLit struct {
	Node
	Src string
}

type ListLit struct {
	Node
	Elems []Expr
}

type TableEntry struct {
	Node
	Key   Expr
	Value Expr
}

type TableLit struct {
	Node
	Entries []TableEntry
}

type TaggedLit struct {
	Node
	Tag   string
	Elems []Expr // :Tag alone (empty payload) has nil Elems
}

// ------------------------------------------------------------ node literals

type NodePart interface{ nodePart() }

type NodeElement struct {
	Node
	Name      string
	Attrs     []NodeAttr
	Body      []NodePart
	SelfClose bool
}

type NodeAttr struct {
	Node
	Name    string
	Literal string // literal value, when Value is nil
	Value   Expr   // spliced attribute value
}

type NodeText struct {
	Node
	Text string
}

type NodeSplice struct {
	Node
	X Expr
}

type NodeSpread struct {
	Node
	X Expr
}

type NodeComment struct {
	Node
	Text string
}

type NodeLit struct {
	Node
	Body []NodePart
}

func (*NodeElement) nodePart() {}
func (*NodeText) nodePart()    {}
func (*NodeSplice) nodePart()  {}
func (*NodeSpread) nodePart()  {}
func (*NodeComment) nodePart() {}

// ---------------------------------------------------------------- functions

type Param struct {
	Node
	Name string
	Rest bool // trailing `rest..` parameter collecting remaining args
}

type FnLit struct {
	Node
	Params []Param
	Body   *Block
}

// FnDecl is the named function form usable as an expression (Section 19
// Primary includes FnDecl); it binds Name in the enclosing scope and
// evaluates to the closure.
type FnDecl struct {
	Node
	Name string
	Fn   *FnLit
}

// EnumLit is an enum block; each member's Expr sees `Iota` as the member's
// auto-number (Section 7), which lowering substitutes statically.
type EnumLit struct {
	Node
	Members []EnumMember
}

type EnumMember struct {
	Node
	Name  string
	Value Expr
}

// ---------------------------------------------------------------- operators

type BinOp int

const (
	OpAdd BinOp = iota
	OpSub
	OpMul
	OpDiv
	OpMod
	OpLt
	OpLe
	OpGt
	OpGe
	OpEq
	OpNeq
	OpAnd
	OpOr
	OpQuery // $
	OpRange // ..
)

var binOpNames = map[BinOp]string{
	OpAdd: "+", OpSub: "-", OpMul: "*", OpDiv: "/", OpMod: "%",
	OpLt: "<", OpLe: "<=", OpGt: ">", OpGe: ">=", OpEq: "==", OpNeq: "!=",
	OpAnd: "and", OpOr: "or", OpQuery: "$", OpRange: "..",
}

func (o BinOp) String() string { return binOpNames[o] }

type Binary struct {
	Node
	Op   BinOp
	L, R Expr
}

// Unary covers `not x`, `-x`, `?x` (Some), and the expression-position
// effect operand `~ x`.
type UnaryOp int

const (
	OpNot UnaryOp = iota
	OpNeg
	OpSome // ?x
	OpEff  // ~x in expression position
)

type Unary struct {
	Node
	Op UnaryOp
	X  Expr
}

// OptNone is the bare `?`.
type OptNone struct{ Node }

// EffectResult is the `~` primary: the result bound by the innermost
// enclosing effect statement, legal only inside the follower statement.
type EffectResult struct{ Node }

type Call struct {
	Node
	Fn   Expr
	Args []Expr
}

type Index struct {
	Node
	X Expr
	I Expr
}

type Field struct {
	Node
	X    Expr
	Name string
}

func (*Ident) expr()        {}
func (*IntLit) expr()       {}
func (*FloatLit) expr()     {}
func (*StringLit) expr()    {}
func (*InterpLit) expr()    {}
func (*CharLit) expr()      {}
func (*RegexLit) expr()     {}
func (*NullLit) expr()      {}
func (*BoolLit) expr()      {}
func (*UnitLit) expr()      {}
func (*TagLit) expr()       {}
func (*SelectorLit) expr()  {}
func (*ListLit) expr()      {}
func (*TableLit) expr()      {}
func (*TaggedLit) expr()    {}
func (*NodeLit) expr()      {}
func (*FnLit) expr()        {}
func (*FnDecl) expr()       {}
func (*EnumLit) expr()      {}
func (*Binary) expr()       {}
func (*Unary) expr()        {}
func (*OptNone) expr()      {}
func (*EffectResult) expr() {}
func (*Call) expr()         {}
func (*Index) expr()        {}
func (*Field) expr()        {}
func (*If) expr()           {} // if/match are expressions (Sections 14/15
func (*Match) expr()        {} // and the worked examples)

// ----------------------------------------------------------------- patterns

type Pattern interface{ pattern() }

type PatWildcard struct{ Node }

// PatLit matches by value equality; Lit is a literal expression node
// (IntLit, FloatLit, StringLit, BoolLit, UnitLit, TagLit).
type PatLit struct {
	Node
	Lit Expr
}

type PatBind struct {
	Node
	Name string
}

// PatList matches a List (or Tagged List) of at least len(Elems) elements
// when Rest is set, exactly len(Elems) otherwise; Rest binds the tail.
type PatList struct {
	Node
	Elems []Pattern
	Rest  *string
}

// PatTagged matches a Tag value or a Tagged List carrying Tag. With
// Brackets the payload patterns apply to the payload list.
type PatTagged struct {
	Node
	Tag      string
	Elems    []Pattern
	Brackets bool
}

// PatOptAny is the bare `?` inside a pattern: matches :Some[anything] or
// :None (the "don't care about the optional payload" form the spec's
// :Success[?] examples require).
type PatOptAny struct{ Node }

// PatSome is `?pat`: matches :Some[x] with x matching Pat.
type PatSome struct {
	Node
	Pat Pattern
}

func (*PatLit) pattern()      {}
func (*PatBind) pattern()     {}
func (*PatList) pattern()     {}
func (*PatTagged) pattern()   {}
func (*PatOptAny) pattern()   {}
func (*PatSome) pattern()     {}
func (*PatWildcard) pattern() {}

// ------------------------------------------------------- grammar and absyn
// GrammarDecl (`grammar Name { ... }`) and AbsynDecl (`absyn`/`asdl`
// `Name { ... }`) lower to libglr-backed runtime objects (see
// internal/grammar). The AST preserves source shape; the runtime holds
// the compiled form.

// GrammarDecl is one `grammar Name { keyword/lexeme/rule* }` block.
type GrammarDecl struct {
	Node
	Name     string
	Keywords []GrammarKeyword
	Lexemes  []GrammarLexeme
	Rules    []GrammarRule
}

// GrammarKeyword is `keyword NAME ::= "literal";`.
type GrammarKeyword struct {
	Node
	Name    string
	Literal string
}

// GrammarLexeme is `lexeme NAME ::= /regex/;`, `lexeme NAME ::= "lit";`
// or `lexeme NAME ::= fn...;`.
type GrammarLexeme struct {
	Node
	Name    string
	Regex   *string // non-nil for `/.../` patterns
	Literal *string // non-nil for `"..."` literal spellings
	Fn      Expr    // non-nil for `fn(reader) {...}` custom lexers
}

// GrammarRule is `rule NAME ::= alternatives;`.
type GrammarRule struct {
	Node
	Name string
	Alts [][]RuleElem // alternatives; each is a sequence
}

// RuleElem is one element of a rule alternative with an EBNF suffix.
type RuleElem struct {
	Node
	Kind   RuleElemKind
	Name   string // Ref: symbol name; Lit: string literal
	Char   string // Char: single character; GroupAlts when KindRuleGroup
	Suffix byte   // 0, '?', '*', '+'
	Group  [][]RuleElem // KindRuleGroup: grouped alternatives
}

// RuleElemKind enumerates rule element shapes.
type RuleElemKind uint8

const (
	KindRuleRef RuleElemKind = iota
	KindRuleLit
	KindRuleChar
	KindRuleRegex
	KindRuleGroup
)

// AbsynDecl is one `absyn Name { typedef* }` (spelled `asdl` as well).
type AbsynDecl struct {
	Node
	Name  string
	Types []AbsynType
}

// AbsynType is `name ::= constructors;`.
type AbsynType struct {
	Node
	Name  string
	Ctors []AbsynCtor
}

// AbsynCtor is one constructor: `Null`, `Bool(bool value)`, etc.
type AbsynCtor struct {
	Node
	Name   string
	Fields []AbsynField
}

// AbsynField is `type name` in a constructor, e.g. `value* elements`.
type AbsynField struct {
	Node
	Type string
	Name string
}
