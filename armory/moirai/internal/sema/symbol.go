// Package sema: symbol.go defines the symbol table and scope machinery.
// Scoping is lexical (Section 12): a block introduces a scope, functions
// close over their defining environment, and a new let in an inner scope
// shadows the outer binding without affecting it.
package sema

import "domscript/internal/token"

// SymKind classifies what a name denotes.
type SymKind uint8

const (
	SymVar     SymKind = iota // let binding, parameter, or function name
	SymBuiltin                // prelude/native name registered by the host
	SymIota                   // the built-in Iota value
	SymGlobal                 // top-level binding from an earlier chunk (Section 17)
)

func (k SymKind) String() string {
	switch k {
	case SymVar:
		return "variable"
	case SymBuiltin:
		return "builtin"
	case SymIota:
		return "iota"
	case SymGlobal:
		return "global"
	}
	return "?"
}

// Symbol is one binding. ID is unique per state-analysis; the compiler
// maps IDs to environment slots.
type Symbol struct {
	ID    int
	Name  string
	Kind  SymKind
	Pos   token.Pos // declaration position
	Mut   bool      // false for immutable declarations (const)
	IsFn  bool      // bound by an fn declaration (mutual-recursion hoisting)
	Slot  int       // per-function slot, filled by the compiler
	Depth int       // environment-chain depth from the function base
}

// Scope is one lexical scope frame.
type Scope struct {
	parent     *Scope
	symbols    map[string]*Symbol
	fnBoundary bool // true for function-body scopes: closures capture here
}

// NewScope creates a child scope.
func NewScope(parent *Scope) *Scope {
	return &Scope{parent: parent, symbols: make(map[string]*Symbol, 4)}
}

// Lookup finds a symbol in this scope or an enclosing one.
func (s *Scope) Lookup(name string) (*Symbol, int, bool) {
	depth := 0
	for sc := s; sc != nil; sc = sc.parent {
		if sym, ok := sc.symbols[name]; ok {
			return sym, depth, true
		}
		depth++
	}
	return nil, 0, false
}

// LookupLocal finds a symbol in exactly this scope (shadowing check).
func (s *Scope) LookupLocal(name string) (*Symbol, bool) {
	sym, ok := s.symbols[name]
	return sym, ok
}

// Declare introduces a new symbol in this scope.
func (s *Scope) Declare(sym *Symbol) *Symbol {
	if s.symbols == nil {
		s.symbols = make(map[string]*Symbol, 4)
	}
	s.symbols[sym.Name] = sym
	return sym
}

// Parent returns the enclosing scope.
func (s *Scope) Parent() *Scope { return s.parent }

// Symbols lists the symbols of this scope in insertion-independent order
// (sorted for determinism).
func (s *Scope) SymbolNames() []string {
	names := make([]string, 0, len(s.symbols))
	for n := range s.symbols {
		names = append(names, n)
	}
	// small n: insertion-sort by name for stable output
	for i := 1; i < len(names); i++ {
		for j := i; j > 0 && names[j] < names[j-1]; j-- {
			names[j], names[j-1] = names[j-1], names[j]
		}
	}
	return names
}

// Get returns the symbol bound to name in this scope only.
func (s *Scope) Get(name string) *Symbol { return s.symbols[name] }
