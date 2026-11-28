// Package sema performs the static checks the spec mandates: name
// resolution against the symbol table, validation of `~` effect-operator
// placement, and pattern well-formedness for match constructs. Domscript
// is dynamically typed, so sema never rejects a program on type grounds
// the spec leaves to runtime.
package sema

import (
	"sort"

	"domscript/internal/ast"
	"domscript/internal/diag"
	"domscript/internal/token"
)

// Info carries the analysis result: the binding each identifier reference
// resolved to, keyed by AST node.
type Info struct {
	// Refs maps an *ast.Ident (or assign target) to its symbol.
	Refs map[any]*Symbol
	// Decls maps a declaring node to its symbol.
	Decls map[any]*Symbol
	// EffectSlots maps an *ast.Effect node to its synthetic binding slot
	// name (the reserved `~` result).
	nextID int
}

func newInfo() *Info {
	return &Info{Refs: make(map[any]*Symbol), Decls: make(map[any]*Symbol)}
}

func (in *Info) newSymbol(name string, kind SymKind, pos token.Pos) *Symbol {
	in.nextID++
	return &Symbol{ID: in.nextID, Name: name, Kind: kind, Pos: pos}
}

// builtins are the prelude names always in scope (Section 20.2); the host
// may extend or restrict by registering more.
var builtinNames = []string{
	"TypeOf", "TagOf", "PayloadOf",
	"Len", "Get", "Set",
	"Push", "Pop", "Keys", "Values",
	"Iota",
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
	"Fetch", "Sleep", "AfterFrame",
	"document",
}

// IsBuiltin reports whether name is a prelude binding.
func IsBuiltin(name string) bool {
	for _, b := range builtinNames {
		if b == name {
			return true
		}
	}
	return false
}

// BuiltinNames returns the sorted prelude name list (for docs and dsc).
func BuiltinNames() []string {
	names := append([]string(nil), builtinNames...)
	sort.Strings(names)
	return names
}

// Checker walks one program.
type Checker struct {
	diags  *diag.Set
	info   *Info
	global *Scope
	// effectDepth counts active effect-follower scopes in the current
	// function; fnDepth counts function literals entered since the
	// innermost effect was pushed (crossing one invalidates `~`).
	effectDepth     int
	fnDepthAtEffect []int
	loopDepth       int
	fnDepth         int // >0 inside a function body
}

// Check analyzes a program. builtinsExtra are host-registered native
// names also in scope; globalsExtra are top-level bindings earlier chunks
// created — assignable, unlike builtins. Returns nil info when errors
// were reported.
func Check(prog *ast.Program, builtinsExtra []string, globalsExtra []string, diags *diag.Set) *Info {
	c := &Checker{diags: diags, info: newInfo()}
	c.global = NewScope(nil)
	for _, b := range builtinNames {
		kind := SymBuiltin
		if b == "Iota" {
			kind = SymIota
		}
		if b == "document" {
			// The document root binding is host-provided state, not a
			// function: treat it like a global so scripts may capture it.
			kind = SymGlobal
		}
		c.global.Declare(c.info.newSymbol(b, kind, token.Pos{}))
	}
	for _, b := range builtinsExtra {
		if _, _, ok := c.global.Lookup(b); !ok {
			c.global.Declare(c.info.newSymbol(b, SymBuiltin, token.Pos{}))
		}
	}
	for _, g := range globalsExtra {
		if _, _, ok := c.global.Lookup(g); !ok {
			c.global.Declare(c.info.newSymbol(g, SymGlobal, token.Pos{}))
		}
	}
	// Hoist named fn declarations in every block so mutual recursion
	// resolves lexically (the binding is established at statement-list
	// start; using it before the fn statement executes is a runtime
	// use-before-initialization, not a resolution error).
	hoistDecls(c.global, prog.Stmts, c)
	for _, st := range prog.Stmts {
		c.checkStmt(st, c.global)
	}
	if diags.HasErrors() {
		return nil
	}
	return c.info
}

func hoistDecls(sc *Scope, stmts []ast.Stmt, c *Checker) {
	for _, st := range stmts {
		if d, ok := st.(*ast.ExprStmt); ok {
			if fd, ok := d.X.(*ast.FnDecl); ok {
				c.declare(sc, fd, fd.Name, fd.Pos)
			}
		}
	}
}

// declare introduces a binding for node in scope sc.
func (c *Checker) declare(sc *Scope, node any, name string, pos token.Pos) *Symbol {
	return c.declareWithMutability(sc, node, name, pos, true)
}

func (c *Checker) declareWithMutability(sc *Scope, node any, name string, pos token.Pos, mutable bool) *Symbol {
	if prev, ok := sc.LookupLocal(name); ok && prev.Kind != SymBuiltin {
		// let x = 1; let x = "s" in the SAME scope is shadowing-per-scope;
		// the spec shows shadowing across scopes. Same-scope rebind is
		// accepted as a fresh binding (documented in NOTES.md).
		_ = prev
	}
	sym := c.info.newSymbol(name, SymVar, pos)
	sym.Mut = mutable
	sc.Declare(sym)
	c.info.Decls[node] = sym
	return sym
}

// ------------------------------------------------------------- statements

func (c *Checker) checkStmt(st ast.Stmt, sc *Scope) {
	switch s := st.(type) {
	case *ast.Import:
		return
	case *ast.Dialect:
		return
	case *ast.Module, *ast.ProgramDecl:
		return
	case *ast.GrammarDecl:
		c.checkGrammarDecl(s, sc)
		return
	case *ast.AbsynDecl:
		return
	case *ast.Extend:
		return
	case *ast.Let:
		c.checkExpr(s.Value, sc)
		c.info.Decls[s] = c.declareWithMutability(sc, s, s.Name, s.Pos, s.Mutable)

	case *ast.Assign:
		c.checkExpr(s.Value, sc)
		switch tgt := s.Target.(type) {
		case *ast.Ident:
			sym, _, ok := sc.Lookup(tgt.Name)
			if !ok {
				c.errf(tgt.Pos, "assignment to undeclared name %q", tgt.Name)
				return
			}
			if sym.Kind == SymBuiltin {
				c.errf(tgt.Pos, "cannot assign to builtin %q", tgt.Name)
				return
			}
			if !sym.Mut {
				c.errf(tgt.Pos, "cannot assign to immutable name %q", tgt.Name)
				return
			}
			c.info.Refs[tgt] = sym
		case *ast.Index:
			c.checkExpr(tgt.X, sc)
			c.checkExpr(tgt.I, sc)
		case *ast.Field:
			c.checkExpr(tgt.X, sc)
		default:
			c.errf(s.Pos, "invalid assignment target")
		}

	case *ast.ExprStmt:
		c.checkExpr(s.X, sc)

	case *ast.Effect:
		c.checkExpr(s.Operand, sc)
		// The follower statement sees `~` bound to the result; a function
		// literal inside the follower runs later, when the binding is gone.
		c.effectDepth++
		c.fnDepthAtEffect = append(c.fnDepthAtEffect, c.fnDepth)
		c.checkStmt(s.Body, sc)
		c.fnDepthAtEffect = c.fnDepthAtEffect[:len(c.fnDepthAtEffect)-1]
		c.effectDepth--

	case *ast.If:
		c.checkExpr(s.Cond, sc)
		inner := NewScope(sc)
		for _, st := range s.Then.Stmts {
			c.checkStmt(st, inner)
		}
		if s.Else != nil {
			if blk, ok := s.Else.(*ast.Block); ok {
				einner := NewScope(sc)
				for _, st := range blk.Stmts {
					c.checkStmt(st, einner)
				}
			} else {
				c.checkStmt(s.Else, sc)
			}
		}

	case *ast.While:
		c.checkExpr(s.Cond, sc)
		c.loopDepth++
		inner := NewScope(sc)
		for _, st := range s.Body.Stmts {
			c.checkStmt(st, inner)
		}
		c.loopDepth--

	case *ast.For:
		c.checkExpr(s.Iter, sc)
		c.loopDepth++
		inner := NewScope(sc)
		c.info.Decls[s] = c.declare(inner, s, s.Var, s.Pos)
		for _, st := range s.Body.Stmts {
			c.checkStmt(st, inner)
		}
		c.loopDepth--

	case *ast.Match:
		c.checkMatch(s, sc)

	case *ast.On:
		c.checkExpr(s.Target, sc)
		if len(s.Params) > 1 {
			c.errf(s.Pos, "an on handler receives exactly one event Table parameter")
		}
		inner := NewScope(sc)
		inner.fnBoundary = true
		c.fnDepth++
		for i := range s.Params {
			prm := &s.Params[i]
			c.info.Decls[prm] = c.declare(inner, prm, prm.Name, prm.Pos)
		}
		for _, st := range s.Body.Stmts {
			c.checkStmt(st, inner)
		}
		c.fnDepth--

	case *ast.Return:
		if c.fnDepth == 0 {
			c.errf(s.Pos, "return outside of a function")
		}
		if s.Value != nil {
			c.checkExpr(s.Value, sc)
		}

	case *ast.Break:
		if c.loopDepth == 0 {
			c.errf(s.Pos, "break outside of a loop")
		}

	case *ast.Continue:
		if c.loopDepth == 0 {
			c.errf(s.Pos, "continue outside of a loop")
		}

	case *ast.BlockStmt:
		inner := NewScope(sc)
		hoistDecls(inner, s.Block.Stmts, c)
		for _, st := range s.Block.Stmts {
			c.checkStmt(st, inner)
		}

	case *ast.Block:
		inner := NewScope(sc)
		hoistDecls(inner, s.Stmts, c)
		for _, st := range s.Stmts {
			c.checkStmt(st, inner)
		}

	default:
		pos, _ := st.(interface{ Span() (token.Pos, token.Pos) }).Span()
		c.errf(pos, "unsupported statement")
	}
}

// ------------------------------------------------------------ expressions

func (c *Checker) checkExpr(e ast.Expr, sc *Scope) {
	switch x := e.(type) {
	case nil:
		return
	case *ast.Ident:
		sym, _, ok := sc.Lookup(x.Name)
		if !ok {
			c.errf(x.Pos, "undefined name %q", x.Name)
			return
		}
		c.info.Refs[x] = sym

	case *ast.IntLit, *ast.FloatLit, *ast.StringLit, *ast.BoolLit,
		*ast.UnitLit, *ast.TagLit, *ast.OptNone, *ast.NullLit,
		*ast.CharLit, *ast.RegexLit:
		return

	case *ast.InterpLit:
		for _, part := range x.Parts {
			// Literal chunks are StringLits (no check needed beyond
			// recursion, which is a no-op for them).
			c.checkExpr(part, sc)
		}
		return

	case *ast.SelectorLit:
		// Selector syntax is validated at evaluation time (Section 8):
		// an invalid selector raises catchable by ~.

	case *ast.ListLit:
		for _, el := range x.Elems {
			c.checkExpr(el, sc)
		}

	case *ast.TableLit:
		for _, en := range x.Entries {
			// Bare identifiers in table keys are shorthand for string keys
			// (used by standard-library export tables).
			if _, bare := en.Key.(*ast.Ident); !bare {
				c.checkExpr(en.Key, sc)
			}
			c.checkExpr(en.Value, sc)
		}

	case *ast.TaggedLit:
		for _, el := range x.Elems {
			c.checkExpr(el, sc)
		}

	case *ast.NodeLit:
		c.checkNodeParts(x.Body, sc)

	case *ast.FnLit:
		inner := NewScope(sc)
		inner.fnBoundary = true
		c.fnDepth++
		for i := range x.Params {
			prm := &x.Params[i]
			c.info.Decls[prm] = c.declare(inner, prm, prm.Name, prm.Pos)
		}
		hoistDecls(inner, x.Body.Stmts, c)
		for _, st := range x.Body.Stmts {
			c.checkStmt(st, inner)
		}
		c.fnDepth--

	case *ast.FnDecl:
		// The name binding is declared by the enclosing statement list
		// (hoisted); the closure body sees it.
		if _, ok := c.info.Decls[x]; !ok {
			c.declare(sc, x, x.Name, x.Pos)
		}
		c.checkExpr(x.Fn, sc)

	case *ast.EnumLit:
		for _, m := range x.Members {
			c.checkExpr(m.Value, sc)
		}

	case *ast.Binary:
		c.checkExpr(x.L, sc)
		c.checkExpr(x.R, sc)

	case *ast.Unary:
		c.checkExpr(x.X, sc)

	case *ast.EffectResult:
		// Legal only inside the follower statement of an enclosing ~,
		// without crossing a function boundary.
		if c.effectDepth == 0 || c.fnDepth != c.fnDepthAtEffect[len(c.fnDepthAtEffect)-1] {
			c.errf(x.Pos, "`~` is only bound inside the single statement following an effect")
		}

	case *ast.Call:
		c.checkExpr(x.Fn, sc)
		for _, a := range x.Args {
			c.checkExpr(a, sc)
		}

	case *ast.Index:
		c.checkExpr(x.X, sc)
		c.checkExpr(x.I, sc)

	case *ast.Field:
		c.checkExpr(x.X, sc)

	case *ast.If:
		c.checkStmt(x, sc)

	case *ast.Match:
		c.checkMatch(x, sc)

	case *ast.On:
		c.checkStmt(x, sc)

	default:
		pos, _ := e.(interface{ Span() (token.Pos, token.Pos) }).Span()
		c.errf(pos, "unsupported expression")
	}
}

func (c *Checker) checkNodeParts(parts []ast.NodePart, sc *Scope) {
	for _, part := range parts {
		switch x := part.(type) {
		case *ast.NodeElement:
			for _, a := range x.Attrs {
				if a.Value != nil {
					c.checkExpr(a.Value, sc)
				}
			}
			c.checkNodeParts(x.Body, sc)
		case *ast.NodeSplice:
			c.checkExpr(x.X, sc)
		case *ast.NodeSpread:
			c.checkExpr(x.X, sc)
		}
	}
}

// --------------------------------------------------------------- match

func (c *Checker) checkGrammarDecl(g *ast.GrammarDecl, sc *Scope) {
	seen := map[string]bool{}
	for _, k := range g.Keywords {
		if seen[k.Name] {
			c.errf(k.Pos, "duplicate terminal %q in grammar %q", k.Name, g.Name)
		}
		seen[k.Name] = true
	}
	for _, l := range g.Lexemes {
		if seen[l.Name] {
			c.errf(l.Pos, "duplicate terminal %q in grammar %q", l.Name, g.Name)
		}
		seen[l.Name] = true
		if l.Fn != nil {
			c.checkExpr(l.Fn, sc)
		}
	}
	defined := map[string]bool{}
	for n := range seen {
		defined[n] = true
	}
	for _, r := range g.Rules {
		if defined[r.Name] && !seen[r.Name] {
			// Rule redefinition (allowed? report).
			c.errf(r.Pos, "duplicate rule %q in grammar %q", r.Name, g.Name)
		}
		defined[r.Name] = true
	}
	// Undefined references are reported (helps catch typos like INT vs
	// NUMBER); char/string literals and groups need no check.
	for _, r := range g.Rules {
		for _, alt := range r.Alts {
			c.checkRuleElems(alt, defined, g.Name)
		}
	}
}

func (c *Checker) checkRuleElems(elems []ast.RuleElem, defined map[string]bool, gname string) {
	for _, e := range elems {
		switch e.Kind {
		case ast.KindRuleRef:
			if !defined[e.Name] {
				c.errf(e.Pos, "undefined symbol %q in grammar %q", e.Name, gname)
			}
		case ast.KindRuleGroup:
			for _, alt := range e.Group {
				c.checkRuleElems(alt, defined, gname)
			}
		}
	}
}

func (c *Checker) checkMatch(m *ast.Match, sc *Scope) {
	c.checkExpr(m.Scrutinee, sc)
	for _, arm := range m.Arms {
		inner := NewScope(sc)
		bound := map[string]bool{}
		c.checkPattern(arm.Pat, inner, bound)
		if blk, ok := arm.Body.(*ast.Block); ok {
			hoistDecls(inner, blk.Stmts, c)
			for _, st := range blk.Stmts {
				c.checkStmt(st, inner)
			}
		} else {
			c.checkExpr(arm.Body, inner)
		}
	}
}

func (c *Checker) checkPattern(p ast.Pattern, sc *Scope, bound map[string]bool) {
	declarePat := func(node any, name string, pos token.Pos) {
		if bound[name] {
			c.errf(pos, "pattern binds %q more than once", name)
			return
		}
		bound[name] = true
		c.info.Decls[node] = c.declare(sc, node, name, pos)
	}
	switch x := p.(type) {
	case *ast.PatWildcard, *ast.PatOptAny:
		return
	case *ast.PatLit:
		switch x.Lit.(type) {
		case *ast.IntLit, *ast.FloatLit, *ast.StringLit, *ast.BoolLit,
			*ast.UnitLit, *ast.TagLit, *ast.CharLit, *ast.NullLit:
			return
		default:
			c.errf(x.Pos, "pattern literal must be an int, float, string, char, bool, unit, null, or tag")
		}
	case *ast.PatBind:
		declarePat(x, x.Name, x.Pos)
	case *ast.PatSome:
		c.checkPattern(x.Pat, sc, bound)
	case *ast.PatList:
		for _, e := range x.Elems {
			c.checkPattern(e, sc, bound)
		}
		if x.Rest != nil {
			declarePat(x, *x.Rest, x.Pos)
		}
	case *ast.PatTagged:
		for _, e := range x.Elems {
			c.checkPattern(e, sc, bound)
		}
	default:
		pos, _ := p.(interface{ Span() (token.Pos, token.Pos) }).Span()
		c.errf(pos, "unsupported pattern")
	}
}

func (c *Checker) errf(pos token.Pos, format string, args ...any) {
	c.diags.Errorf(diag.Span{Start: pos, End: pos}, format, args...)
}
