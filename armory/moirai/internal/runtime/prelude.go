// Package runtime: prelude.go installs the Section 20.2 prelude into a
// state's global environment as native functions. Every name sema takes
// for granted is bound here, so a fresh state can evaluate any program
// that stays inside the spec'd surface; the host may re-register any of
// these names (RegisterNative replaces the binding and un-marks the
// prelude entry) or extend the set with its own natives.
//
// Error convention: every native returns a Go error for misuse (wrong
// operand kinds, out-of-range indices) — the VM surfaces it as a raise
// catchable by `~` (Section 10), never as a crash. Lookups that can
// legitimately come up empty return :None (Section 4.2).
package runtime

import (
	"errors"
	"fmt"
	"sort"
)

// preludeNameSet is the fixed 20.2 name list; installPrelude binds each
// entry. Kept as a slice (order matters for docs) plus a set for lookup.
var preludeOrder = []string{
	"TypeOf", "TagOf", "PayloadOf",
	"Len", "Get", "Set",
	"Push", "Pop", "Keys", "Values",
	"IsSome", "IsNone", "Unwrap", "UnwrapOr",
	"IsSuccess", "IsError",
	"Raise", "Error",
	"IOError", "FileError", "ValueError", "TypeError", "ParseError",
	"GrammarError", "LexError", "SyntaxError", "IndexError", "KeyError",
	"AttributeError", "RuntimeError", "AssertionError",
	"Head", "Advance",
	"DeriveParser", "DeriveTree", "DeriveVisitor", "CreateReader",
	"CreateElement", "CreateText", "CreateComment", "CreateFragment",
	"NodeKind", "TagName", "SameNode",
	"Attrs", "GetAttr", "SetAttr", "RemoveAttr",
	"Children", "Parent",
	"AppendChild", "RemoveChild", "ReplaceChild",
	"TextOf",
	"Query", "QueryOne", "Matches",
	"Off", "Emit",
	"Print",
	"Fetch", "Sleep", "AfterFrame", // asynchronous defaults (below)
}

// PreludeNames returns the sorted prelude name list (dsc docs, sema extras).
func PreludeNames() []string {
	out := append([]string(nil), preludeOrder...)
	sort.Strings(out)
	return out
}

// installPrelude binds every prelude name in st's global environment.
// Iota is a value, not a function (Section 7); everything else is a
// synchronous native except Fetch/Sleep/AfterFrame, which register the
// built-in asynchronous defaults (resolved by AutoResolveDefaults when
// no host scheduler takes over).
func (st *State) installPrelude() {
	st.preludeNames = make(map[string]bool, len(preludeOrder))
	for _, name := range preludeOrder {
		st.preludeNames[name] = true
	}

	// Section 7: Iota is a value with two roles; the callable role is
	// dispatched by the VM's call path (KindIota), the value role by name.
	st.BindGlobal("Iota", IotaValue())

	reg := func(name string, fn GoNative) {
		st.BindGlobal(name, FnOf(&Fn{Name: name, Native: fn, NParams: -1}))
	}

	// ------------------------------------------------------------ types
	reg("TypeOf", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "TypeOf", 1, args); err != nil {
			return Unit(), err
		}
		// TypeName carries a leading ":" (`:String`); Tag adds its own,
		// so strip one to avoid `::String`.
		name := TypeName(args[0])
		if len(name) > 0 && name[0] == ':' {
			name = name[1:]
		}
		return Tag(name), nil
	})
	reg("TagOf", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "TagOf", 1, args); err != nil {
			return Unit(), err
		}
		if tag := args[0].ListTag(); tag != "" {
			return TaggedOf("Some", []Value{Tag(tag)}), nil
		}
		return TaggedOf("None", nil), nil
	})
	reg("PayloadOf", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "PayloadOf", 1, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() == KindList && args[0].ListTag() != "" {
			return ListOf(args[0].List().Elems), nil
		}
		return EmptyList(), nil
	})

	// ------------------------------------------------------ collections
	reg("Len", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Len", 1, args); err != nil {
			return Unit(), err
		}
		switch args[0].Kind() {
		case KindList, KindTable, KindString, KindChar:
			return Int(int64(args[0].Len())), nil
		case KindNode:
			kids, err := st.ChildrenOf(args[0])
			if err != nil {
				return Unit(), err
			}
			return Int(int64(len(kids.List().Elems))), nil
		}
		return Unit(), fmt.Errorf("Len on %s", TypeName(args[0]))
	})
	reg("Get", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Get", 2, args); err != nil {
			return Unit(), err
		}
		x, k := args[0], args[1]
		switch x.Kind() {
		case KindList:
			i, err := intIndex("Get", k)
			if err != nil {
				return Unit(), err
			}
			elems := x.List().Elems
			if i < 0 || i >= len(elems) {
				return TaggedOf("None", nil), nil
			}
			return TaggedOf("Some", []Value{elems[i]}), nil
		case KindString:
			i, err := intIndex("Get", k)
			if err != nil {
				return Unit(), err
			}
			s := x.StrValue()
			if i < 0 || i >= len(s) {
				return TaggedOf("None", nil), nil
			}
			return TaggedOf("Some", []Value{Str(string(s[i]))}), nil
		case KindChar:
			i, err := intIndex("Get", k)
			if err != nil {
				return Unit(), err
			}
			if i != 0 {
				return TaggedOf("None", nil), nil
			}
			return TaggedOf("Some", []Value{x}), nil
		case KindReader:
			i, err := intIndex("Get", k)
			if err != nil {
				return Unit(), err
			}
			if ch, ok := x.Reader().Peek(i); ok {
				return TaggedOf("Some", []Value{Char(ch)}), nil
			}
			return TaggedOf("None", nil), nil
		case KindTable:
			v, ok := x.Table().Get(k)
			if !ok {
				return TaggedOf("None", nil), nil
			}
			return TaggedOf("Some", []Value{v}), nil
		}
		return Unit(), fmt.Errorf("Get on %s", TypeName(x))
	})
	reg("Set", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Set", 3, args); err != nil {
			return Unit(), err
		}
		x, k, v := args[0], args[1], args[2]
		switch x.Kind() {
		case KindTable:
			if err := x.Table().Set(k, v); err != nil {
				return Unit(), err
			}
			return x, nil
		case KindList:
			i, err := intIndex("Set", k)
			if err != nil {
				return Unit(), err
			}
			elems := x.List().Elems
			if i < 0 || i >= len(elems) {
				return Unit(), fmt.Errorf("Set index %d out of bounds (len %d)", i, len(elems))
			}
			elems[i] = v
			return x, nil
		}
		return Unit(), fmt.Errorf("Set on %s", TypeName(x))
	})
	reg("Push", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Push", 2, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindList {
			return Unit(), fmt.Errorf("Push expects a :List, got %s", TypeName(args[0]))
		}
		l := args[0].List()
		l.Elems = append(l.Elems, args[1])
		return args[0], nil
	})
	reg("Pop", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Pop", 1, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindList {
			return Unit(), fmt.Errorf("Pop expects a :List, got %s", TypeName(args[0]))
		}
		l := args[0].List()
		if len(l.Elems) == 0 {
			return TaggedOf("None", nil), nil
		}
		last := l.Elems[len(l.Elems)-1]
		l.Elems = l.Elems[:len(l.Elems)-1]
		return last, nil
	})
	reg("Keys", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Keys", 1, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindTable {
			return Unit(), fmt.Errorf("Keys expects a :Table, got %s", TypeName(args[0]))
		}
		return ListOf(args[0].Table().Keys()), nil
	})
	reg("Values", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Values", 1, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindTable {
			return Unit(), fmt.Errorf("Values expects a :Table, got %s", TypeName(args[0]))
		}
		return ListOf(args[0].Table().Values()), nil
	})

	// -------------------------------------------------------- optionals
	reg("IsSome", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "IsSome", 1, args); err != nil {
			return Unit(), err
		}
		return Bool(args[0].ListTag() == "Some"), nil
	})
	reg("IsNone", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "IsNone", 1, args); err != nil {
			return Unit(), err
		}
		return Bool(args[0].ListTag() == "None"), nil
	})
	reg("Unwrap", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Unwrap", 1, args); err != nil {
			return Unit(), err
		}
		l := args[0].ListTag()
		// `:Some[v]` (optionals) and `:Success[v]` (effect payloads) both
		// unwrap; the latter is used by the `~ Err(...)` desugaring.
		if l != "Some" && l != "Success" {
			return Unit(), fmt.Errorf("Unwrap on %s (only :Some/:Success unwrap)", displayOrType(args[0]))
		}
		elems := args[0].List().Elems
		if len(elems) != 1 {
			return Unit(), fmt.Errorf("Unwrap on malformed :%s", l)
		}
		return elems[0], nil
	})
	reg("UnwrapOr", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "UnwrapOr", 2, args); err != nil {
			return Unit(), err
		}
		if args[0].ListTag() == "Some" {
			elems := args[0].List().Elems
			if len(elems) == 1 {
				return elems[0], nil
			}
		}
		return args[1], nil
	})
	reg("IsSuccess", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "IsSuccess", 1, args); err != nil {
			return Unit(), err
		}
		return Bool(IsSuccessValue(args[0])), nil
	})
	reg("IsError", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "IsError", 1, args); err != nil {
			return Unit(), err
		}
		return Bool(IsErrorValue(args[0])), nil
	})

	// ------------------------------------------------------ typed errors
	// `Raise(v)` carries v as a typed Go error for `~` boundaries.
	reg("Raise", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Raise", 1, args); err != nil {
			return Unit(), err
		}
		return Unit(), RaiseError(args[0])
	})
	for _, ename := range ErrorConstructors {
		name := ename // capture
		reg(name, func(st *State, args []Value) (Value, error) {
			return MakeErrorValue(name, args), nil
		})
	}

	// ------------------------------------------------- reader primitives
	// Custom lexeme functions receive a :Reader. `Head` peeks, `Advance`
	// consumes one rune; both also exist as `reader.head`/`reader.advance`
	// methods (see VM field handling).
	reg("Head", func(st *State, args []Value) (Value, error) {
		if len(args) != 1 {
			return Unit(), fmt.Errorf("Head expects 1 arg, got %d", len(args))
		}
		switch args[0].Kind() {
		case KindReader:
			if ch, ok := args[0].Reader().Head(); ok {
				return Char(ch), nil
			}
			return TaggedOf("None", nil), nil
		case KindString:
			s := args[0].StrValue()
			if s == "" {
				return TaggedOf("None", nil), nil
			}
			return Char(s[:1]), nil
		case KindList:
			elems := args[0].List().Elems
			if len(elems) == 0 {
				return TaggedOf("None", nil), nil
			}
			return elems[0], nil
		}
		return Unit(), fmt.Errorf("Head on %s", TypeName(args[0]))
	})
	reg("Advance", func(st *State, args []Value) (Value, error) {
		if len(args) != 1 {
			return Unit(), fmt.Errorf("Advance expects 1 arg, got %d", len(args))
		}
		if args[0].Kind() != KindReader {
			return Unit(), fmt.Errorf("Advance expects a :Reader, got %s", TypeName(args[0]))
		}
		args[0].Reader().Advance()
		return Unit(), nil
	})

	// ------------------------------------------------- grammar natives
	// These mirror dependencies/libglr's grammar/parser/forest pipeline
	// (grammar.h, parser.h, forest.h) in pure Go; see internal/grammar.
	reg("DeriveParser", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "DeriveParser", 1, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindTag {
			return Unit(), fmt.Errorf("DeriveParser expects a :Tag grammar name, got %s", TypeName(args[0]))
		}
		g, ok := st.Grammars[args[0].StrValue()]
		if !ok {
			return Unit(), &TypedError{Tag: "GrammarError", Msg: "unknown grammar :" + args[0].StrValue(), Val: TaggedOf("GrammarError", []Value{Str("unknown grammar :" + args[0].StrValue())})}
		}
		return ParserOf(&Parser{Grammar: g}), nil
	})
	reg("DeriveTree", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "DeriveTree", 1, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindTag {
			return Unit(), fmt.Errorf("DeriveTree expects a :Tag absyn name, got %s", TypeName(args[0]))
		}
		a, ok := st.Absyns[args[0].StrValue()]
		if !ok {
			return Unit(), &TypedError{Tag: "GrammarError", Msg: "unknown absyn :" + args[0].StrValue(), Val: TaggedOf("GrammarError", []Value{Str("unknown absyn :" + args[0].StrValue())})}
		}
		// Trees are the Absyn value itself (constructors via `:Ctor[...]`).
		return AbsynOf(a), nil
	})
	reg("DeriveVisitor", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "DeriveVisitor", 1, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindTag {
			return Unit(), fmt.Errorf("DeriveVisitor expects a :Tag absyn name, got %s", TypeName(args[0]))
		}
		a, ok := st.Absyns[args[0].StrValue()]
		if !ok {
			return Unit(), &TypedError{Tag: "GrammarError", Msg: "unknown absyn :" + args[0].StrValue(), Val: TaggedOf("GrammarError", []Value{Str("unknown absyn :" + args[0].StrValue())})}
		}
		t := NewTable()
		_ = t.Set(Tag("absyn"), AbsynOf(a))
		_ = t.Set(Tag("visit"), Str("visitor"))
		return TableValue(t), nil
	})
	reg("CreateReader", func(st *State, args []Value) (Value, error) {
		if len(args) != 4 {
			return Unit(), fmt.Errorf("CreateReader expects 4 args (parser, tree, visitor, input), got %d", len(args))
		}
		var input string
		switch args[3].Kind() {
		case KindString:
			input = args[3].StrValue()
		case KindReader:
			input = args[3].Reader().Rest()
		default:
			return Unit(), fmt.Errorf("CreateReader input must be :String, got %s", TypeName(args[3]))
		}
		t := NewTable()
		_ = t.Set(Tag("parser"), args[0])
		_ = t.Set(Tag("tree"), args[1])
		_ = t.Set(Tag("visitor"), args[2])
		_ = t.Set(Tag("input"), Str(input))
		_ = t.Set(Tag("pos"), Int(0))
		_ = t.Set(Str("reader"), ReaderOf(&Reader{Input: input}))
		return TableValue(t), nil
	})

	// ------------------------------------------------------------- nodes
	reg("CreateElement", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "CreateElement", 1, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindTag {
			return Unit(), fmt.Errorf("CreateElement expects a :Tag, got %s", TypeName(args[0]))
		}
		return st.CreateElement(args[0].StrValue()), nil
	})
	reg("CreateText", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "CreateText", 1, args); err != nil {
			return Unit(), err
		}
		return st.CreateText(Display(args[0])), nil
	})
	reg("CreateComment", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "CreateComment", 1, args); err != nil {
			return Unit(), err
		}
		return st.CreateComment(Display(args[0])), nil
	})
	reg("CreateFragment", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "CreateFragment", 0, args); err != nil {
			return Unit(), err
		}
		return st.CreateFragment(), nil
	})
	reg("NodeKind", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "NodeKind", 1, args); err != nil {
			return Unit(), err
		}
		k, err := st.NodeKindOf(args[0])
		if err != nil {
			return Unit(), err
		}
		return Tag(k.TagName()), nil
	})
	reg("TagName", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "TagName", 1, args); err != nil {
			return Unit(), err
		}
		return st.TagNameOf(args[0])
	})
	reg("SameNode", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "SameNode", 2, args); err != nil {
			return Unit(), err
		}
		if args[0].Kind() != KindNode || args[1].Kind() != KindNode {
			return Unit(), fmt.Errorf("SameNode expects two :Node values")
		}
		return Bool(Equals(args[0], args[1])), nil
	})
	reg("Attrs", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Attrs", 1, args); err != nil {
			return Unit(), err
		}
		return st.AttrsOf(args[0])
	})
	reg("GetAttr", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "GetAttr", 2, args); err != nil {
			return Unit(), err
		}
		return st.GetAttr(args[0], Display(args[1]))
	})
	reg("SetAttr", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "SetAttr", 3, args); err != nil {
			return Unit(), err
		}
		if err := st.SetAttr(args[0], Display(args[1]), args[2]); err != nil {
			return Unit(), err
		}
		return args[0], nil
	})
	reg("RemoveAttr", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "RemoveAttr", 2, args); err != nil {
			return Unit(), err
		}
		if err := st.RemoveAttr(args[0], Display(args[1])); err != nil {
			return Unit(), err
		}
		return args[0], nil
	})
	reg("Children", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Children", 1, args); err != nil {
			return Unit(), err
		}
		return st.ChildrenOf(args[0])
	})
	reg("Parent", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Parent", 1, args); err != nil {
			return Unit(), err
		}
		return st.ParentOf(args[0])
	})
	reg("AppendChild", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "AppendChild", 2, args); err != nil {
			return Unit(), err
		}
		if err := st.AppendChild(args[0], args[1]); err != nil {
			return Unit(), err
		}
		return args[1], nil
	})
	reg("RemoveChild", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "RemoveChild", 2, args); err != nil {
			return Unit(), err
		}
		if err := st.RemoveChild(args[0], args[1]); err != nil {
			return Unit(), err
		}
		return Unit(), nil
	})
	reg("ReplaceChild", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "ReplaceChild", 3, args); err != nil {
			return Unit(), err
		}
		if err := st.ReplaceChild(args[0], args[1], args[2]); err != nil {
			return Unit(), err
		}
		return args[2], nil
	})
	reg("TextOf", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "TextOf", 1, args); err != nil {
			return Unit(), err
		}
		return st.TextOfNode(args[0])
	})

	// ---------------------------------------------------------- queries
	reg("Query", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Query", 2, args); err != nil {
			return Unit(), err
		}
		sel, err := selArg(args[1])
		if err != nil {
			return Unit(), err
		}
		return st.QueryAll(args[0], sel)
	})
	reg("QueryOne", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "QueryOne", 2, args); err != nil {
			return Unit(), err
		}
		sel, err := selArg(args[1])
		if err != nil {
			return Unit(), err
		}
		return st.QueryFirst(args[0], sel)
	})
	reg("Matches", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Matches", 2, args); err != nil {
			return Unit(), err
		}
		sel, err := selArg(args[1])
		if err != nil {
			return Unit(), err
		}
		return st.MatchesSelector(args[0], sel)
	})

	// ----------------------------------------------------------- events
	reg("Off", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Off", 1, args); err != nil {
			return Unit(), err
		}
		if err := st.Off(args[0]); err != nil {
			return Unit(), err
		}
		return Unit(), nil
	})
	reg("Emit", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Emit", 3, args); err != nil {
			return Unit(), err
		}
		if args[1].Kind() != KindTag {
			return Unit(), fmt.Errorf("Emit expects a :Tag event name, got %s", TypeName(args[1]))
		}
		if err := st.Emit(args[0], args[1].StrValue(), args[2]); err != nil {
			return Unit(), err
		}
		return Unit(), nil
	})

	// ------------------------------------------------------------ print
	reg("Print", func(st *State, args []Value) (Value, error) {
		if err := arity(st, "Print", 1, args); err != nil {
			return Unit(), err
		}
		st.Print(args[0])
		return Unit(), nil
	})

	// ------------------------------------------- asynchronous defaults
	// Without a host scheduler these park the coroutine and wait for
	// AutoResolveDefaults (or a host ResolveAsync) to complete them:
	//
	//   Fetch      -> :Error[:Some["fetch: no network host is registered"]]
	//   Sleep      -> resolves immediately (no host clock to wait on)
	//   AfterFrame -> resolves immediately
	st.defaultAsync = make(map[uint64]string)
	async := func(name string, start func(st *State, args []Value) error) {
		st.BindGlobal(name, FnOf(&Fn{Name: name, NParams: -1,
			NativeAsync: func(s *State, args []Value) (uint64, error) {
				tok := s.NewAsyncToken()
				if err := start(s, args); err != nil {
					return 0, err
				}
				s.defaultAsync[tok] = name
				return tok, nil
			}}))
	}
	async("Fetch", func(s *State, args []Value) error {
		if len(args) != 1 {
			return fmt.Errorf("Fetch expects 1 arg, got %d", len(args))
		}
		return nil
	})
	async("Sleep", func(s *State, args []Value) error {
		if len(args) != 1 || args[0].Kind() != KindInt {
			return fmt.Errorf("Sleep expects an :Int millisecond count")
		}
		return nil
	})
	async("AfterFrame", func(s *State, args []Value) error {
		if len(args) != 0 {
			return fmt.Errorf("AfterFrame expects no args")
		}
		return nil
	})
}

// PendingDefaultTokens lists outstanding default-async tokens, sorted.
func (st *State) PendingDefaultTokens() []uint64 {
	var toks []uint64
	for tok := range st.defaultAsync {
		if _, parked := st.pending[tok]; parked {
			toks = append(toks, tok)
		}
	}
	sort.Slice(toks, func(i, j int) bool { return toks[i] < toks[j] })
	return toks
}

// AutoResolveDefaults completes every outstanding default-asynchronous
// effect (Section 20.2's Fetch/Sleep/AfterFrame) so a host without its
// own scheduler — dsc, tests — can drain suspended coroutines via Pump.
func (st *State) AutoResolveDefaults() {
	for _, tok := range st.PendingDefaultTokens() {
		name := st.defaultAsync[tok]
		delete(st.defaultAsync, tok)
		switch name {
		case "Fetch":
			st.ResolveAsync(tok, Unit(), errors.New("fetch: no network host is registered"))
		default: // Sleep, AfterFrame: nothing to wait for without a host
			st.ResolveAsync(tok, Unit(), nil)
		}
	}
}

// ------------------------------------------------------------- helpers

func arity(st *State, name string, n int, args []Value) error {
	if len(args) != n {
		return fmt.Errorf("%s expects %d arg(s), got %d", name, n, len(args))
	}
	return nil
}

func intIndex(name string, v Value) (int, error) {
	if v.Kind() != KindInt {
		return 0, fmt.Errorf("%s index must be :Int, got %s", name, TypeName(v))
	}
	return int(v.IntValue()), nil
}

func selArg(v Value) (*Selector, error) {
	if v.Kind() != KindSelector {
		return nil, fmt.Errorf("expected a :Selector, got %s", TypeName(v))
	}
	return v.Selector(), nil
}

func displayOrType(v Value) string {
	if v.Kind() == KindList && v.ListTag() == "None" {
		return ":None"
	}
	return ":" + TypeName(v)
}
