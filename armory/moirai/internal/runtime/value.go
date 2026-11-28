// Package runtime: value.go implements the Domscript value model.
//
// REPRESENTATION CHOICE. Values are a flat tagged struct, not an
// interface: every scalar field lives inline (b/i/f/s) and only compound
// values carry a pointer in ref. Type dispatch in the VM is a single
// integer compare on kind, which the interpreter does on every operand —
// a pointer/interface dispatch there would cost two dependent loads
// where this costs one byte compare. There is no nil anywhere: a Value
// with kind KindUnit is the unit, absence is the :None tagged list, and
// no constructor or accessor in this package produces or accepts a Go
// nil standing in for a Domscript value (Section 4.2).
package runtime

import (
	"fmt"
	"strconv"
	"strings"
)

// Kind enumerates the core types (Section 4.1). The first twelve ordinals
// match the C ABI's DomscriptType ordinals (Section 18.3) — that order is
// ABI. Later kinds (Character and beyond) are extensions with no C-ABI
// commitment yet.
type Kind uint8

const (
	KindUnit Kind = iota
	KindBool
	KindInt
	KindFloat
	KindString
	KindTag
	KindList
	KindTable
	KindFn
	KindIota
	KindNode
	KindSelector
	KindChar
	KindRegex
	KindGrammar
	KindAbsyn
	KindParser
	KindReader
)

func (k Kind) String() string {
	switch k {
	case KindUnit:
		return ":Unit"
	case KindBool:
		return ":Bool"
	case KindInt:
		return ":Int"
	case KindFloat:
		return ":Float"
	case KindString:
		return ":String"
	case KindTag:
		return ":Tag"
	case KindList:
		return ":List"
	case KindTable:
		return ":Table"
	case KindFn:
		return ":Fn"
	case KindIota:
		return ":Iota"
	case KindNode:
		return ":Node"
	case KindSelector:
		return ":Selector"
	case KindChar:
		return ":Character"
	case KindRegex:
		return ":Regex"
	case KindGrammar:
		return ":Grammar"
	case KindAbsyn:
		return ":Absyn"
	case KindParser:
		return ":Parser"
	case KindReader:
		return ":Reader"
	}
	return ":?"
}

// Value is one Domscript value.
type Value struct {
	kind Kind
	b    bool
	i    int64
	f    float64
	s    string // :String content, :Tag name, :Fn display name
	ref  any    // *List, *Table, *Fn, *Node, *Selector, *Regex, *Grammar, ...
}

// ------------------------------------------------------------------ scalars

func Unit() Value           { return Value{kind: KindUnit} }
func Bool(b bool) Value     { return Value{kind: KindBool, b: b} }
func Int(n int64) Value     { return Value{kind: KindInt, i: n} }
func Float(f float64) Value { return Value{kind: KindFloat, f: f} }
func Str(s string) Value    { return Value{kind: KindString, s: s} }
func Tag(name string) Value {
	return Value{kind: KindTag, s: name}
}

// Char creates a :Character value from a single-character string.
func Char(s string) Value { return Value{kind: KindChar, s: s} }

// CharValue returns the character's single-character string.
func (v Value) CharValue() string { return v.s }

// IotaValue is the built-in Iota value (Section 7).
func IotaValue() Value { return Value{kind: KindIota} }

func (v Value) Kind() Kind          { return v.kind }
func (v Value) BoolValue() bool     { return v.b }
func (v Value) IntValue() int64     { return v.i }
func (v Value) FloatValue() float64 { return v.f }
func (v Value) StrValue() string    { return v.s }

// Numeric returns the value as float64, promoting Int.
func (v Value) Numeric() float64 {
	if v.kind == KindInt {
		return float64(v.i)
	}
	return v.f
}

// ------------------------------------------------------------------- lists

// List is the mutable sequence; a non-empty Tag makes it a Tagged List
// (Section 6.4). A Tagged List reports :List from TypeOf.
type List struct {
	Tag   string // "" for a plain list
	Elems []Value
}

func ListOf(elems []Value) Value { return Value{kind: KindList, ref: &List{Elems: elems}} }
func TaggedOf(tag string, elems []Value) Value {
	return Value{kind: KindList, ref: &List{Tag: tag, Elems: elems}}
}

// EmptyList returns a fresh empty plain list value.
func EmptyList() Value { return Value{kind: KindList, ref: &List{}} }

func (v Value) List() *List { return v.ref.(*List) }
func (v Value) IsTaggedList() bool {
	l, ok := v.ref.(*List)
	return ok && l.Tag != ""
}
func (v Value) ListTag() string {
	if l, ok := v.ref.(*List); ok {
		return l.Tag
	}
	return ""
}

// Len returns the length: list elements, table entries, string bytes, or
// node children (the handle table answers for nodes).
func (v Value) Len() int {
	switch v.kind {
	case KindList:
		return len(v.ref.(*List).Elems)
	case KindTable:
		return v.ref.(*Table).Len()
	case KindString:
		return len(v.s)
	case KindChar:
		return 1
	case KindNode:
		n := v.ref.(*Node)
		if n.tbl == nil {
			return 0
		}
		e, ok := n.tbl.table[n.id]
		if !ok || e.External || e.Data == nil {
			return 0
		}
		return len(e.Data.Children)
	}
	return 0
}

// ------------------------------------------------------------------ tables

// Table is the unordered map from hashable keys (scalars and Tags) to
// values. Iteration order is insertion order — the spec leaves Keys'
// order unspecified; a deterministic order keeps Print and tests stable.
type Table struct {
	entries []tableEntry
	index   map[string]int
}

type tableEntry struct {
	key Value
	val Value
}

func NewTable() *Table { return &Table{index: make(map[string]int)} }

func TableOf(pairs [][2]Value) Value {
	t := NewTable()
	for _, p := range pairs {
		t.Set(p[0], p[1])
	}
	return Value{kind: KindTable, ref: t}
}

// TableValue wraps an existing table as a :Table Value.
func TableValue(t *Table) Value { return Value{kind: KindTable, ref: t} }

func (v Value) Table() *Table { return v.ref.(*Table) }

// keyID canonicalizes a hashable key. Int and Float compare equal
// numerically (Section 4.3), so they share one numeric namespace.
func keyID(k Value) (string, bool) {
	switch k.kind {
	case KindInt:
		return "n" + strconv.FormatFloat(float64(k.i), 'g', 17, 64), true
	case KindFloat:
		return "n" + strconv.FormatFloat(k.f, 'g', 17, 64), true
	case KindString:
		return "s" + k.s, true
	case KindChar:
		return "c" + k.s, true
	case KindTag:
		return "t" + k.s, true
	case KindBool:
		if k.b {
			return "b1", true
		}
		return "b0", true
	}
	return "", false
}

// Set inserts or replaces. Returns an error when the key is not hashable
// (Lists, Tables, Nodes are invalid keys — Section 6.2).
func (t *Table) Set(key, val Value) error {
	id, ok := keyID(key)
	if !ok {
		return fmt.Errorf("%s is not a valid table key (only scalars and Tags)", TypeName(key))
	}
	if i, exists := t.index[id]; exists {
		t.entries[i].val = val
		return nil
	}
	if t.index == nil {
		t.index = make(map[string]int)
	}
	t.index[id] = len(t.entries)
	t.entries = append(t.entries, tableEntry{key: key, val: val})
	return nil
}

// Get returns (value, true) or (Unit, false).
func (t *Table) Get(key Value) (Value, bool) {
	id, ok := keyID(key)
	if !ok || t.index == nil {
		return Unit(), false
	}
	if i, exists := t.index[id]; exists {
		return t.entries[i].val, true
	}
	return Unit(), false
}

// Delete removes a key; reports whether it was present.
func (t *Table) Delete(key Value) bool {
	id, ok := keyID(key)
	if !ok || t.index == nil {
		return false
	}
	i, exists := t.index[id]
	if !exists {
		return false
	}
	t.entries = append(t.entries[:i], t.entries[i+1:]...)
	delete(t.index, id)
	for k, v := range t.index {
		if v > i {
			t.index[k] = v - 1
		}
	}
	return true
}

// Clear removes all entries.
func (t *Table) Clear() {
	t.entries = nil
	t.index = make(map[string]int)
}

// Len returns the entry count.
func (t *Table) Len() int { return len(t.entries) }

// Keys returns the keys in insertion order.
func (t *Table) Keys() []Value {
	ks := make([]Value, len(t.entries))
	for i, e := range t.entries {
		ks[i] = e.key
	}
	return ks
}

// Values returns the values in insertion order.
func (t *Table) Values() []Value {
	vs := make([]Value, len(t.entries))
	for i, e := range t.entries {
		vs[i] = e.val
	}
	return vs
}

// Entries iterates in insertion order.
func (t *Table) Entries(f func(k, v Value)) {
	for _, e := range t.entries {
		f(e.key, e.val)
	}
}

// ----------------------------------------------------------------- fns

// GoNative is a synchronous native function; a non-nil error surfaces as
// :Error at the caller's ~ boundary.
type GoNative func(st *State, args []Value) (Value, error)

// GoAsyncStart begins a host operation, returning a token the host later
// resolves via ResolveAsync.
type GoAsyncStart func(st *State, args []Value) (token uint64, err error)

// Fn is a function value: Domscript code interpreted by the VM (Code
// non-nil) or a native (Native/NativeAsync set).
type Fn struct {
	Name        string
	NParams     int
	Rest        bool
	ParamNames  []string
	Native      GoNative     // synchronous native
	NativeAsync GoAsyncStart // asynchronous native
	Code        any          // *ir.Func (set by exec; opaque here)
	Env         *Env         // captured defining environment (Domscript fns)
}

func FnOf(f *Fn) Value  { return Value{kind: KindFn, s: f.Name, ref: f} }
func (v Value) Fn() *Fn { return v.ref.(*Fn) }

// ---------------------------------------------------------------- selectors

// Selector is a compiled CSS-like selector (Section 8). Src is the source
// text; equality is by Src (Section 4.3).
type Selector struct {
	Src  string
	Prog *SelProg
}

func SelectorOf(sel *Selector) Value {
	return Value{kind: KindSelector, s: sel.Src, ref: sel}
}
func (v Value) Selector() *Selector { return v.ref.(*Selector) }

// ------------------------------------------------------------------- envs

// Env is one function activation's variable frame. Closures capture the
// defining Env pointer, so assignments through a closure are visible to
// every scope sharing the frame (todo_widget's counter relies on this).
type Env struct {
	parent *Env
	vals   []Value
	set    []bool
	// index names slots; only the global frame uses it (host bindings and
	// cross-chunk top-level lets resolve by name there).
	index map[string]int
}

// NewEnv creates a frame with n slots chained to parent.
func NewEnv(parent *Env, n int) *Env {
	return &Env{parent: parent, vals: make([]Value, n), set: make([]bool, n)}
}

// Slot reads a slot; ok=false when the binding was never assigned
// (use-before-initialization).
func (e *Env) Slot(i int) (Value, bool) {
	if i < 0 || i >= len(e.vals) || !e.set[i] {
		return Unit(), false
	}
	return e.vals[i], true
}

// SetSlot writes a slot, growing the frame when a later chunk extends it.
func (e *Env) SetSlot(i int, v Value) {
	for i >= len(e.vals) {
		e.vals = append(e.vals, Unit())
		e.set = append(e.set, false)
	}
	e.vals[i] = v
	e.set[i] = true
}

// Grow extends a frame to n slots (top-level chunk compilation).
func (e *Env) Grow(n int) {
	for len(e.vals) < n {
		e.vals = append(e.vals, Unit())
		e.set = append(e.set, false)
	}
}

// Walk returns the env d hops up the chain (0 = this frame).
func (e *Env) Walk(d int) *Env {
	for ; d > 0 && e != nil; d-- {
		e = e.parent
	}
	return e
}

func (e *Env) Size() int { return len(e.vals) }

// ------------------------------------------------------------------ equality

// Equals implements Section 4.3: scalars (with Int/Float numeric
// cross-comparison) by value, Lists and Tables structurally, Fn and Node
// by identity, Selector by source string.
func Equals(a, b Value) bool {
	if a.kind == KindInt && b.kind == KindFloat || a.kind == KindFloat && b.kind == KindInt {
		return a.Numeric() == b.Numeric()
	}
	// Characters compare equal to single-character strings (so lexeme
	// helpers mixing `Head(reader)` characters and `"x"` literals work).
	if a.kind == KindChar && b.kind == KindString || a.kind == KindString && b.kind == KindChar {
		return a.s == b.s
	}
	if a.kind != b.kind {
		return false
	}
	switch a.kind {
	case KindUnit:
		return true
	case KindBool:
		return a.b == b.b
	case KindInt:
		return a.i == b.i
	case KindFloat:
		return a.f == b.f
	case KindString:
		return a.s == b.s
	case KindChar:
		return a.s == b.s
	case KindTag:
		return a.s == b.s
	case KindSelector:
		return a.s == b.s
	case KindRegex:
		return a.s == b.s
	case KindIota:
		return true
	case KindList:
		al, bl := a.ref.(*List), b.ref.(*List)
		if al.Tag != bl.Tag || len(al.Elems) != len(bl.Elems) {
			return false
		}
		for i := range al.Elems {
			if !Equals(al.Elems[i], bl.Elems[i]) {
				return false
			}
		}
		return true
	case KindTable:
		at, bt := a.ref.(*Table), b.ref.(*Table)
		if at.Len() != bt.Len() {
			return false
		}
		ok := true
		at.Entries(func(k, v Value) {
			if !ok {
				return
			}
			bv, found := bt.Get(k)
			if !found || !Equals(v, bv) {
				ok = false
			}
		})
		return ok
	case KindFn:
		return a.ref == b.ref
	case KindNode:
		// Identity is the handle (id within one state's table), not the
		// wrapper pointer: the same handle re-wrapped must compare equal.
		an, bn := a.ref.(*Node), b.ref.(*Node)
		return an.id == bn.id && an.tbl == bn.tbl
	case KindGrammar, KindAbsyn, KindParser, KindReader:
		return a.ref == b.ref
	}
	return false
}

// TypeName returns the type tag name of v (TypeOf).
func TypeName(v Value) string {
	if v.kind == KindTag {
		return ":Tag"
	}
	return v.Kind().String()
}

// ---------------------------------------------------------------- rendering

// Display renders v for display: Print(x) prints this; strings print bare.
func Display(v Value) string { return render(v, false) }

// Repr renders v for containers: strings are quoted inside aggregates.
func Repr(v Value) string { return render(v, true) }

func render(v Value, quoted bool) string {
	switch v.kind {
	case KindUnit:
		return "()"
	case KindBool:
		if v.b {
			return "true"
		}
		return "false"
	case KindInt:
		return strconv.FormatInt(v.i, 10)
	case KindFloat:
		return strconv.FormatFloat(v.f, 'g', -1, 64)
	case KindString:
		if quoted {
			return strconv.Quote(v.s)
		}
		return v.s
	case KindTag:
		return ":" + v.s
	case KindChar:
		return renderChar(v.s, quoted)
	case KindRegex:
		if quoted {
			return "/" + v.s + "/"
		}
		return "/" + v.s + "/"
	case KindGrammar:
		if g, ok := v.ref.(*Grammar); ok && g != nil && g.Name != "" {
			return "<grammar " + g.Name + ">"
		}
		return "<grammar>"
	case KindAbsyn:
		if a, ok := v.ref.(*Absyn); ok && a != nil && a.Name != "" {
			return "<absyn " + a.Name + ">"
		}
		return "<absyn>"
	case KindParser:
		return "<parser>"
	case KindReader:
		if r, ok := v.ref.(*Reader); ok && r != nil {
			return "<reader pos=" + strconv.Itoa(r.Pos) + ">"
		}
		return "<reader>"
	case KindIota:
		return "<iota>"
	case KindFn:
		if v.s != "" {
			return "<fn " + v.s + ">"
		}
		return "<fn>"
	case KindSelector:
		return "$" + strconv.Quote(v.s)
	case KindList:
		l := v.ref.(*List)
		var sb strings.Builder
		if l.Tag != "" {
			sb.WriteString(":" + l.Tag)
		}
		sb.WriteString("[")
		for i, e := range l.Elems {
			if i > 0 {
				sb.WriteString(", ")
			}
			sb.WriteString(render(e, true))
		}
		sb.WriteString("]")
		return sb.String()
	case KindTable:
		t := v.ref.(*Table)
		var sb strings.Builder
		sb.WriteString("%{")
		first := true
		t.Entries(func(k, val Value) {
			if !first {
				sb.WriteString(", ")
			}
			first = false
			sb.WriteString(render(k, true))
			sb.WriteString(" => ")
			sb.WriteString(render(val, true))
		})
		sb.WriteString("}")
		return sb.String()
	case KindNode:
		n := v.ref.(*Node)
		return n.display()
	}
	return "?"
}

// renderChar renders a :Character. Bare (Display) form is the character
// itself (so `"a" + #\b` and interpolation splice naturally); quoted
// (Repr) form is the Scheme literal (`#\,`, `#\newline`, ...).
func renderChar(s string, quoted bool) string {
	if !quoted {
		return s
	}
	switch s {
	case "\n":
		return `#\newline`
	case " ":
		return `#\space`
	case "\t":
		return `#\tab`
	case "\r":
		return `#\return`
	case "\x07":
		return `#\bell`
	case "\x0c":
		return `#\feed`
	case "\x1b":
		return `#\escape`
	case "\x00":
		return `#\null`
	}
	// Single printable ASCII (including `,"`, `\`, `/`, brackets) uses
	// the short `#\x` form; anything else falls back to quoted string.
	if len(s) == 1 {
		return `#\` + s
	}
	return strconv.Quote(s)
}
