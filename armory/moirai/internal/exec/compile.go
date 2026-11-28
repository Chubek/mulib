// Package exec: compile.go lowers a sema-checked AST to ir.Func bytecode.
//
// The mapping from source to machine is fixed by three decisions made
// once, here:
//
//   - Scopes are flattened per activation: every let/param/pattern-bind
//     in one function gets its own Env slot, and a reference resolves to
//     (activation hops, slot). Sema already resolved every Ident to a
//     symbol, so lowering only allocates slots and consults that mapping.
//   - The chunk's activation uses the state's global Env (top-level lets
//     ARE the globals of Section 17), with slots allocated through a
//     name-seeded allocator so later chunks and host lookups agree.
//   - Values flow on one stack; every construct is compiled to leave
//     exactly the values its consumer expects, which is what lets error
//     unwinding truncate the stack to an effect handler's saved depth.
//
// Lowering runs after a clean sema, so anything inconsistent down here is
// a compiler bug: those surface as compileError and stop the chunk.
package exec

import (
	"fmt"

	"domscript/internal/ast"
	"domscript/internal/diag"
	"domscript/internal/ir"
	"domscript/internal/runtime"
	"domscript/internal/sema"
	"domscript/internal/token"
)

// compileError is an internal lowering failure (never a user diagnostic —
// sema has already run).
type compileError struct{ msg string }

func (e *compileError) Error() string { return "compile: " + e.msg }

func cfail(format string, args ...any) {
	panic(&compileError{msg: fmt.Sprintf(format, args...)})
}

// binding is where one sema symbol lives: slot in activation act's Env.
type binding struct {
	act  int
	slot int
}

type loopCtx struct {
	breaks  []int // OpJump indices to patch to the end label
	conts   []int // OpJump indices to patch to the head label
	hasIter bool  // `for` loop: break must also pop the iterator
}

type compiler struct {
	info  *sema.Info
	st    *runtime.State
	fns   []*ir.Func // activation stack; [0] is the chunk
	binds map[int]*binding

	// Global-slot allocator for the chunk activation: seeded from the
	// state's global name index so cross-chunk references hit the same
	// slots the host sees. nameReg mirrors which slots are name-registered
	// (top-level lets) versus anonymous block locals.
	gslots  map[string]int
	topNext int

	// nesting of non-function blocks inside the chunk activation: only
	// depth 0 declares name-registered globals.
	scopeDepth int

	loops  []loopCtx
	effect []int // active `~` result slots (innermost last)

	// enum-block iota counter (Section 7)
	iotaNext int
	inEnum   int

	line int32
}

// Compile lowers a checked program. gstlots seeds the chunk's global slot
// map (a copy of the state's global name index). optimize=true folds
// constants (used unless --no-optimize).
func Compile(prog *ast.Program, info *sema.Info, st *runtime.State, optimize bool) (f *ir.Func, err error) {
	defer func() {
		if r := recover(); r != nil {
			if ce, ok := r.(*compileError); ok {
				f, err = nil, ce
				return
			}
			panic(r)
		}
	}()
	c := &compiler{
		info:   info,
		st:     st,
		fns:    []*ir.Func{{Name: "<chunk>", NLocals: 0}},
		binds:  make(map[int]*binding),
		gslots: map[string]int{},
	}
	for name, slot := range st.GlobalIndex() {
		c.gslots[name] = slot
		if slot >= c.topNext {
			c.topNext = slot + 1
		}
	}
	chunk := c.fns[0]
	c.compileBlockInto(chunk, prog.Stmts, true)
	chunk.Code = append(chunk.Code, ir.Instr{Op: ir.OpReturn})
	chunk.Line = append(chunk.Line, c.line)
	// The chunk activation's Env is the state's global frame: every slot
	// the allocator handed out (named and anonymous block locals alike)
	// must be counted so Validate's slot-range check sees them.
	chunk.NLocals = c.topNext
	chunkGlobalReg[chunk] = c.gslots
	ir.Validate(chunk)
	if optimize {
		ir.Optimize(chunk)
	} else {
		ir.Rewrite(chunk)
	}
	return chunk, nil
}

// ChunkGlobals reports the name->slot map the compiled chunk wants
// installed into the state's global env before it runs.
func ChunkGlobals(f *ir.Func) map[string]int { return chunkGlobalReg[f] }

var chunkGlobalReg = map[*ir.Func]map[string]int{}

// ---------------------------------------------------------------- emitting

func (c *compiler) fn() *ir.Func { return c.fns[len(c.fns)-1] }

func (c *compiler) emit(op ir.Op, a, b, d int64) int {
	f := c.fn()
	f.Code = append(f.Code, ir.Instr{Op: op, A: a, B: b, C: d})
	f.Line = append(f.Line, c.line)
	return len(f.Code) - 1
}

func (c *compiler) here() int { return len(c.fn().Code) }

func (c *compiler) patch(at, target int) { c.fn().Code[at].A = int64(target) }

func (c *compiler) kconst(v runtime.Value) int64 {
	f := c.fn()
	for i, e := range f.Consts {
		if runtime.Equals(e, v) {
			return int64(i)
		}
	}
	f.Consts = append(f.Consts, v)
	return int64(len(f.Consts) - 1)
}

// ------------------------------------------------------------- allocation

// allocLocal claims a slot in the current activation.
func (c *compiler) allocLocal() int {
	f := c.fn()
	s := f.NLocals
	f.NLocals = s + 1
	return s
}

// declareChunk registers a binding in the chunk activation. Named
// (top-level) bindings publish their slot; nested block bindings stay
// anonymous.
func (c *compiler) declareChunk(sym *sema.Symbol, named bool) *binding {
	slot := -1
	if named {
		if s, ok := c.gslots[sym.Name]; ok {
			slot = s
		} else {
			slot = c.topNext
			c.topNext++
			c.gslots[sym.Name] = slot
		}
	} else {
		slot = c.topNext
		c.topNext++
	}
	b := &binding{act: 0, slot: slot}
	c.binds[sym.ID] = b
	return b
}

func (c *compiler) declareLocal(sym *sema.Symbol) *binding {
	b := &binding{act: len(c.fns) - 1, slot: c.allocLocal()}
	c.binds[sym.ID] = b
	return b
}

func (c *compiler) declare(sym *sema.Symbol) *binding {
	if b, ok := c.binds[sym.ID]; ok {
		return b
	}
	if len(c.fns) == 1 {
		return c.declareChunk(sym, c.scopeDepth == 0)
	}
	return c.declareLocal(sym)
}

func (c *compiler) symOf(node any) *sema.Symbol {
	sym, ok := c.info.Refs[node]
	if !ok {
		cfail("no resolved symbol for reference")
	}
	return sym
}

func (c *compiler) declSym(node any) *sema.Symbol {
	sym, ok := c.info.Decls[node]
	if !ok {
		cfail("no declared symbol for binding")
	}
	return sym
}

func (c *compiler) loadBinding(sym *sema.Symbol) {
	switch sym.Kind {
	case sema.SymBuiltin, sema.SymGlobal, sema.SymIota:
		slot, ok := c.gslots[sym.Name]
		if !ok {
			slot = c.topNext
			c.topNext++
			c.gslots[sym.Name] = slot
		}
		// Builtins and globals live in the chunk's global frame. From
		// nested function activations that frame is reached by walking
		// past every active function frame.
		c.emit(ir.OpLoad, int64(len(c.fns)-1), int64(slot), 0)
	default:
		b, ok := c.binds[sym.ID]
		if !ok {
			cfail("unallocated binding for %q", sym.Name)
		}
		c.emit(ir.OpLoad, int64(len(c.fns)-1-b.act), int64(b.slot), 0)
	}
}

// ------------------------------------------------------------- statements

// stmtYields reports whether a statement form produces a value (Sections
// 13/15: expressions in tail position).
func stmtYields(s ast.Stmt) bool {
	switch x := s.(type) {
	case *ast.ExprStmt:
		return true
	case *ast.If:
		return true
	case *ast.Match:
		return true
	case *ast.Block:
		return true
	case *ast.On:
		return true
	case *ast.Effect:
		return stmtYields(x.Body)
	}
	return false
}

// compileBlockInto compiles stmts into f; wantValue selects tail-value
// semantics (block value = last expression, Section 13).
func (c *compiler) compileBlockInto(f *ir.Func, stmts []ast.Stmt, wantValue bool) {
	// Sema hoists named function declarations, so lowering must allocate
	// their bindings before compiling any function body in this statement
	// list. This supports recursion and mutual recursion.
	for _, s := range stmts {
		if es, ok := s.(*ast.ExprStmt); ok {
			if fd, ok := es.X.(*ast.FnDecl); ok {
				c.declare(c.declSym(fd))
			}
		}
	}
	last := len(stmts) - 1
	for i, s := range stmts {
		tail := wantValue && i == last
		if tail && !stmtYields(s) {
			// The block's value is () when the last statement isn't an
			// expression.
			c.compileStmt(s, false)
			c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
			break
		}
		c.compileStmt(s, tail)
	}
	if len(stmts) == 0 && wantValue {
		c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
	}
}

func (c *compiler) compileBlock(b *ast.Block, wantValue bool) {
	c.compileBlockInto(c.fn(), b.Stmts, wantValue)
}

func (c *compiler) compileStmt(s ast.Stmt, wantValue bool) {
	if s == nil {
		if wantValue {
			c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
		}
		return
	}
	pos, _ := s.(interface{ Span() (token.Pos, token.Pos) }).Span()
	c.line = int32(pos.Line)
	c.compileStmt1(s, wantValue)
}

func (c *compiler) compileStmt1(s ast.Stmt, wantValue bool) {
	switch x := s.(type) {
	case *ast.Import:
		if wantValue {
			c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
		}
		return
	case *ast.Module, *ast.ProgramDecl, *ast.Dialect, *ast.Extend,
		*ast.GrammarDecl, *ast.AbsynDecl:
		c.compileDeclStmt(x, wantValue)
		return
	case *ast.Let:
		c.compileExpr(x.Value)
		c.declare(c.declSym(x))
		c.storeDecl(x)

	case *ast.Assign:
		c.compileAssign(x)

	case *ast.ExprStmt:
		c.compileExpr(x.X)
		if !wantValue {
			c.emit(ir.OpPop, 0, 0, 0)
		}

	case *ast.Effect:
		c.compileEffect(x, wantValue)

	case *ast.If:
		c.compileIf(x, wantValue)

	case *ast.While:
		c.compileWhile(x)
		if wantValue {
			c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
		}

	case *ast.For:
		c.compileFor(x)
		if wantValue {
			c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
		}

	case *ast.Match:
		c.compileMatch(x)
		if !wantValue {
			c.emit(ir.OpPop, 0, 0, 0)
		}

	case *ast.On:
		c.compileOn(x)
		if !wantValue {
			c.emit(ir.OpPop, 0, 0, 0)
		}

	case *ast.Return:
		if x.Value != nil {
			c.compileExpr(x.Value)
		} else {
			c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
		}
		c.emit(ir.OpReturn, 0, 0, 0)

	case *ast.Break:
		if len(c.loops) == 0 {
			cfail("break outside a loop") // sema rejects; defensive
		}
		l := &c.loops[len(c.loops)-1]
		if l.hasIter {
			c.emit(ir.OpPopIter, 0, 0, 0)
		}
		l.breaks = append(l.breaks, c.emit(ir.OpJump, 0, 0, 0))

	case *ast.Continue:
		if len(c.loops) == 0 {
			cfail("continue outside a loop")
		}
		l := &c.loops[len(c.loops)-1]
		l.conts = append(l.conts, c.emit(ir.OpJump, 0, 0, 0))

	case *ast.BlockStmt:
		c.withScope(func() { c.compileBlock(x.Block, wantValue) })

	case *ast.Block:
		c.withScope(func() { c.compileBlock(x, wantValue) })

	default:
		cfail("unsupported statement %T", s)
	}
}

// withScope runs f at one deeper block nesting (chunk activation only
// tracks top-level-ness for global registration).
func (c *compiler) withScope(f func()) {
	c.scopeDepth++
	f()
	c.scopeDepth--
}

// compileDeclStmt lowers inert declarations. Grammar declarations also
// evaluate custom lexeme functions into hidden chunk globals
// (`__grammar_G_L`) for the exec layer's post-run registration.
func (c *compiler) compileDeclStmt(s ast.Stmt, wantValue bool) {
	if g, ok := s.(*ast.GrammarDecl); ok {
		for i := range g.Lexemes {
			l := &g.Lexemes[i]
			if l.Fn == nil {
				continue
			}
			hidden := "__grammar_" + g.Name + "_" + l.Name
			if fnLit, ok := l.Fn.(*ast.FnLit); ok {
				idx := c.compileFnLit(fnLit, hidden)
				c.emit(ir.OpMakeFn, int64(idx), 0, 0)
			} else {
				c.compileExpr(l.Fn)
			}
			slot, ok := c.gslots[hidden]
			if !ok {
				slot = c.topNext
				c.topNext++
				c.gslots[hidden] = slot
			}
			c.emit(ir.OpStore, int64(len(c.fns)-1), int64(slot), 0)
		}
	}
	if wantValue {
		c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
	}
}

func (c *compiler) storeDecl(node any) {
	sym := c.declSym(node)
	b := c.binds[sym.ID]
	c.emit(ir.OpStore, int64(len(c.fns)-1-b.act), int64(b.slot), 0)
}

func (c *compiler) compileAssign(x *ast.Assign) {
	switch tgt := x.Target.(type) {
	case *ast.Ident:
		c.compileExpr(x.Value)
		sym := c.symOf(tgt)
		if !sym.Mut {
			cfail("assignment to immutable name %q", sym.Name)
		}
		switch sym.Kind {
		case sema.SymBuiltin:
			cfail("assignment to builtin") // sema rejects; defensive
		case sema.SymGlobal:
			slot, ok := c.gslots[sym.Name]
			if !ok {
				slot = c.topNext
				c.topNext++
				c.gslots[sym.Name] = slot
			}
			c.emit(ir.OpStore, int64(len(c.fns)-1), int64(slot), 0)
		default:
			b, ok := c.binds[sym.ID]
			if !ok {
				cfail("unallocated assignment target %q", sym.Name)
			}
			c.emit(ir.OpStore, int64(len(c.fns)-1-b.act), int64(b.slot), 0)
		}

	case *ast.Index:
		c.compileExpr(tgt.X)
		c.compileExpr(tgt.I)
		c.compileExpr(x.Value)
		c.emit(ir.OpSetIndex, 0, 0, 0)

	case *ast.Field:
		c.compileExpr(tgt.X)
		c.compileExpr(x.Value)
		c.emit(ir.OpSetField, c.kconst(runtime.Str(tgt.Name)), 0, 0)

	default:
		cfail("invalid assignment target")
	}
}

// compileEffect lowers `~ operand` + follower (Section 10). Both the
// success and the error path store the wrapped :Success/:Error into a
// fresh slot the follower's `~` references read, and both FALL THROUGH
// to the follower — the follower runs with `~` bound either way; only
// its bound value differs. The statement's value is the follower's.
func (c *compiler) compileEffect(x *ast.Effect, wantValue bool) {
	slot := c.allocLocal()
	catch := c.emit(ir.OpEffectBegin, 0, int64(slot), 0)
	c.compileExpr(x.Operand)
	c.emit(ir.OpEffectEnd, int64(slot), 0, 0)
	c.emit(ir.OpPop, 0, 0, 0)
	join := c.emit(ir.OpJump, 0, 0, 0)
	c.patch(catch, c.here())
	c.emit(ir.OpEffectCatch, int64(slot), 0, 0)
	c.emit(ir.OpPop, 0, 0, 0)
	// join: the follower, reached from both paths.
	c.patch(join, c.here())
	c.effect = append(c.effect, slot)
	c.compileStmt(x.Body, wantValue)
	c.effect = c.effect[:len(c.effect)-1]
}

func (c *compiler) compileIf(x *ast.If, wantValue bool) {
	c.compileExpr(x.Cond)
	elseJ := c.emit(ir.OpJumpFalse, 0, 0, 0)
	c.withScope(func() { c.compileBlock(x.Then, wantValue) })
	end := c.emit(ir.OpJump, 0, 0, 0)
	c.patch(elseJ, c.here())
	switch e := x.Else.(type) {
	case nil:
		if wantValue {
			c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
		}
	case *ast.If:
		c.compileIf(e, wantValue)
	case *ast.Block:
		c.withScope(func() { c.compileBlock(e, wantValue) })
	default:
		cfail("unsupported else branch %T", x.Else)
	}
	c.patch(end, c.here())
}

func (c *compiler) compileWhile(x *ast.While) {
	head := c.here()
	c.compileExpr(x.Cond)
	exit := c.emit(ir.OpJumpFalse, 0, 0, 0)
	c.loops = append(c.loops, loopCtx{})
	c.withScope(func() { c.compileBlock(x.Body, false) })
	l := c.loops[len(c.loops)-1]
	c.loops = c.loops[:len(c.loops)-1]
	for _, j := range l.conts {
		c.patch(j, head)
	}
	c.emit(ir.OpJump, int64(head), 0, 0)
	// Breaks land after the back-jump: patching them before it would
	// route the break straight into the loop head again.
	for _, j := range l.breaks {
		c.patch(j, c.here())
	}
	c.patch(exit, c.here())
}

func (c *compiler) compileFor(x *ast.For) {
	c.compileExpr(x.Iter)
	c.emit(ir.OpForInit, 0, 0, 0)
	sym := c.declSym(x)
	head := c.here()
	slot := 0
	if len(c.fns) == 1 {
		b := c.declareChunk(sym, false)
		slot = b.slot
	} else {
		slot = c.declareLocal(sym).slot
	}
	next := c.emit(ir.OpForNext, int64(slot), 0, 0)
	c.loops = append(c.loops, loopCtx{hasIter: true})
	c.withScope(func() { c.compileBlock(x.Body, false) })
	l := c.loops[len(c.loops)-1]
	c.loops = c.loops[:len(c.loops)-1]
	for _, j := range l.conts {
		c.patch(j, head)
	}
	c.emit(ir.OpJump, int64(head), 0, 0)
	// Breaks already popped their iterator (OpPopIter); they land where
	// exhaustion lands, after the back-jump.
	for _, j := range l.breaks {
		c.patch(j, c.here())
	}
	c.fn().Code[next].B = int64(c.here()) // exhaustion target: operand B
}

// compileMatch lowers one match. The scrutinee stays on the stack while
// arms test (OpMatch re-tests on fail); a successful arm consumes it and
// leaves the arm body's value, which is the match's value (Section 15).
func (c *compiler) compileMatch(x *ast.Match) {
	c.compileExpr(x.Scrutinee)
	ends := []int{}
	for i := range x.Arms {
		arm := &x.Arms[i]
		pat := c.compilePattern(arm.Pat)
		fail := c.emit(ir.OpMatch, int64(pat), 0, 0)
		if blk, ok := arm.Body.(*ast.Block); ok {
			c.withScope(func() { c.compileBlock(blk, true) })
		} else {
			c.compileExpr(arm.Body)
		}
		ends = append(ends, c.emit(ir.OpJump, 0, 0, 0))
		// Fail target is operand B (patch would clobber the pattern
		// index in A).
		c.fn().Code[fail].B = int64(c.here())
	}
	// No arm matched: raise (Section 16).
	c.emit(ir.OpRaise, c.kconst(runtime.Str("match: no arm matched the value")), 0, 0)
	for _, j := range ends {
		c.patch(j, c.here())
	}
}

func (c *compiler) compileOn(x *ast.On) {
	c.compileExpr(x.Target)
	handler := c.compileHandler(x.Params, x.Body, "<on "+x.Event+">")
	c.emit(ir.OpMakeFn, int64(handler), 0, 0)
	c.emit(ir.OpSubscribe, c.kconst(runtime.Str(x.Event)), 0, 0)
}

// compileHandler compiles an `on` body as a nested activation and returns
// its index in the current func's closure pool.
func (c *compiler) compileHandler(params []ast.Param, body *ast.Block, name string) int {
	parent := c.fn()
	fn := &ir.Func{Name: name}
	parent.Funcs = append(parent.Funcs, fn)
	c.fns = append(c.fns, fn)
	for i := range params {
		p := &params[i]
		sym := c.declSym(p)
		b := c.declareLocal(sym)
		_ = b
		fn.Params = append(fn.Params, p.Name)
		if p.Rest {
			fn.Rest = true
		}
	}
	c.compileBlock(body, true)
	c.emit(ir.OpReturn, 0, 0, 0)
	c.fns = c.fns[:len(c.fns)-1]
	return len(parent.Funcs) - 1
}

// ------------------------------------------------------------ expressions

func (c *compiler) compileExpr(e ast.Expr) {
	if e == nil {
		cfail("nil expression")
	}
	pos, _ := e.(interface{ Span() (token.Pos, token.Pos) }).Span()
	c.line = int32(pos.Line)
	c.compileExpr1(e)
}

func (c *compiler) compileExpr1(e ast.Expr) {
	switch x := e.(type) {
	case *ast.IntLit:
		c.emit(ir.OpConst, c.kconst(runtime.Int(x.Value)), 0, 0)
	case *ast.FloatLit:
		c.emit(ir.OpConst, c.kconst(runtime.Float(x.Value)), 0, 0)
	case *ast.StringLit:
		c.emit(ir.OpConst, c.kconst(runtime.Str(x.Value)), 0, 0)
	case *ast.InterpLit:
		for _, part := range x.Parts {
			c.compileExpr(part)
		}
		c.emit(ir.OpInterp, int64(len(x.Parts)), 0, 0)
	case *ast.CharLit:
		c.emit(ir.OpConst, c.kconst(runtime.Char(x.Value)), 0, 0)
	case *ast.RegexLit:
		c.emit(ir.OpMakeRegex, c.kconst(runtime.Str(x.Pattern)), 0, 0)
	case *ast.NullLit:
		c.emit(ir.OpConst, c.kconst(runtime.TaggedOf("None", nil)), 0, 0)
	case *ast.BoolLit:
		c.emit(ir.OpConst, c.kconst(runtime.Bool(x.Value)), 0, 0)
	case *ast.UnitLit:
		c.emit(ir.OpConst, c.kconst(runtime.Unit()), 0, 0)
	case *ast.TagLit:
		c.emit(ir.OpConst, c.kconst(runtime.Tag(x.Name)), 0, 0)
	case *ast.OptNone:
		c.emit(ir.OpConst, c.kconst(runtime.TaggedOf("None", nil)), 0, 0)
	case *ast.SelectorLit:
		c.emit(ir.OpMakeSelector, c.kconst(runtime.Str(x.Src)), 0, 0)
	case *ast.EffectResult:
		if len(c.effect) == 0 {
			cfail("`~` result outside an effect follower") // sema rejects
		}
		c.emit(ir.OpLoad, 0, int64(c.effect[len(c.effect)-1]), 0)

	case *ast.Ident:
		c.compileIdent(x)

	case *ast.ListLit:
		for _, el := range x.Elems {
			c.compileExpr(el)
		}
		c.emit(ir.OpMakeList, int64(len(x.Elems)), 0, 0)

	case *ast.TableLit:
		c.emit(ir.OpNewTable, 0, 0, 0)
		for i := range x.Entries {
			if id, bare := x.Entries[i].Key.(*ast.Ident); bare {
				c.emit(ir.OpConst, c.kconst(runtime.Str(id.Name)), 0, 0)
			} else {
				c.compileExpr(x.Entries[i].Key)
			}
			c.compileExpr(x.Entries[i].Value)
			c.emit(ir.OpTableSet, 0, 0, 0)
		}

	case *ast.TaggedLit:
		for _, el := range x.Elems {
			c.compileExpr(el)
		}
		c.emit(ir.OpMakeTagged, int64(len(x.Elems)), c.kconst(runtime.Tag(x.Tag)), 0)

	case *ast.NodeLit:
		c.compileNodeLit(x)

	case *ast.FnLit:
		idx := c.compileFnLit(x, "")
		c.emit(ir.OpMakeFn, int64(idx), 0, 0)

	case *ast.FnDecl:
		idx := c.compileFnLit(x.Fn, x.Name)
		c.emit(ir.OpMakeFn, int64(idx), 0, 0)
		// A named function is an expression that also installs its
		// binding. Keep one copy as the expression result; the store
		// consumes the other copy, and statement lowering may then
		// discard the result normally.
		c.emit(ir.OpDup, 0, 0, 0)
		c.declare(c.declSym(x))
		c.storeDecl(x)

	case *ast.EnumLit:
		c.compileEnum(x)

	case *ast.Unary:
		switch x.Op {
		case ast.OpNot:
			c.compileExpr(x.X)
			c.emit(ir.OpNot, 0, 0, 0)
		case ast.OpNeg:
			c.compileExpr(x.X)
			c.emit(ir.OpNeg, 0, 0, 0)
		case ast.OpSome:
			c.compileExpr(x.X)
			c.emit(ir.OpSome, 0, 0, 0)
		case ast.OpEff:
			// Expression-position `~`: same wrap, value stays on stack.
			slot := c.allocLocal()
			catch := c.emit(ir.OpEffectBegin, 0, int64(slot), 0)
			c.compileExpr(x.X)
			c.emit(ir.OpEffectEnd, int64(slot), 0, 0)
			end := c.emit(ir.OpJump, 0, 0, 0)
			c.patch(catch, c.here())
			c.emit(ir.OpEffectCatch, int64(slot), 0, 0)
			c.patch(end, c.here())
		}

	case *ast.Binary:
		c.compileBinary(x)

	case *ast.Call:
		c.compileExpr(x.Fn)
		for _, a := range x.Args {
			c.compileExpr(a)
		}
		c.emit(ir.OpCall, int64(len(x.Args)), 0, 0)

	case *ast.Index:
		c.compileExpr(x.X)
		c.compileExpr(x.I)
		c.emit(ir.OpIndex, 0, 0, 0)

	case *ast.Field:
		c.compileExpr(x.X)
		c.emit(ir.OpField, c.kconst(runtime.Str(x.Name)), 0, 0)

	case *ast.If:
		c.compileIf(x, true)

	case *ast.Match:
		// Expression position: the arm value is the match's value
		// (Section 15); nothing to pop.
		c.compileMatch(x)

	case *ast.On:
		// Expression position: the :Subscription stays (Section 11).
		c.compileOn(x)

	default:
		cfail("unsupported expression %T", e)
	}
}

func (c *compiler) compileIdent(x *ast.Ident) {
	sym, ok := c.info.Refs[x]
	if !ok {
		cfail("unresolved identifier %q", x.Name)
	}
	// Inside an enum block, `Iota` is the auto-number (Section 7).
	if c.inEnum > 0 && sym.Kind == sema.SymIota {
		c.emit(ir.OpConst, c.kconst(runtime.Int(int64(c.iotaNext))), 0, 0)
		c.iotaNext++
		return
	}
	if sym.Kind == sema.SymIota {
		c.emit(ir.OpConst, c.kconst(runtime.IotaValue()), 0, 0)
		return
	}
	c.loadBinding(sym)
}

func (c *compiler) compileBinary(x *ast.Binary) {
	switch x.Op {
	case ast.OpAnd:
		c.compileExpr(x.L)
		c.emit(ir.OpDup, 0, 0, 0)
		skip := c.emit(ir.OpJumpFalse, 0, 0, 0)
		c.emit(ir.OpPop, 0, 0, 0)
		c.compileExpr(x.R)
		c.emit(ir.OpTestBool, 0, 0, 0)
		c.patch(skip, c.here())
	case ast.OpOr:
		c.compileExpr(x.L)
		c.emit(ir.OpDup, 0, 0, 0)
		skip := c.emit(ir.OpJumpTrue, 0, 0, 0)
		c.emit(ir.OpPop, 0, 0, 0)
		c.compileExpr(x.R)
		c.emit(ir.OpTestBool, 0, 0, 0)
		c.patch(skip, c.here())
	case ast.OpRange:
		c.compileExpr(x.L)
		c.compileExpr(x.R)
		c.emit(ir.OpRange, 0, 0, 0)
	case ast.OpQuery:
		c.compileExpr(x.L)
		c.compileExpr(x.R)
		c.emit(ir.OpQuery, 0, 0, 0)
	case ast.OpAdd, ast.OpSub, ast.OpMul, ast.OpDiv, ast.OpMod:
		c.compileExpr(x.L)
		c.compileExpr(x.R)
		c.emit(ir.OpArith, int64(x.Op-ast.OpAdd), 0, 0)
	case ast.OpLt, ast.OpLe, ast.OpGt, ast.OpGe:
		c.compileExpr(x.L)
		c.compileExpr(x.R)
		c.emit(ir.OpCmp, int64(x.Op-ast.OpLt), 0, 0)
	case ast.OpEq:
		c.compileExpr(x.L)
		c.compileExpr(x.R)
		c.emit(ir.OpEq, 0, 0, 0)
	case ast.OpNeq:
		c.compileExpr(x.L)
		c.compileExpr(x.R)
		c.emit(ir.OpNe, 0, 0, 0)
	default:
		cfail("unsupported binary op %v", x.Op)
	}
}

func (c *compiler) compileFnLit(x *ast.FnLit, name string) int {
	parent := c.fn()
	fn := &ir.Func{Name: name}
	parent.Funcs = append(parent.Funcs, fn)
	c.fns = append(c.fns, fn)
	for i := range x.Params {
		p := &x.Params[i]
		sym := c.declSym(p)
		c.declareLocal(sym)
		fn.Params = append(fn.Params, p.Name)
		if p.Rest {
			fn.Rest = true
		}
	}
	c.compileBlock(x.Body, true)
	c.emit(ir.OpReturn, 0, 0, 0)
	c.fns = c.fns[:len(c.fns)-1]
	return len(parent.Funcs) - 1
}

func (c *compiler) compileEnum(x *ast.EnumLit) {
	c.emit(ir.OpNewTable, 0, 0, 0)
	saved := c.iotaNext
	c.iotaNext = 0
	c.inEnum++
	for i := range x.Members {
		m := &x.Members[i]
		c.emit(ir.OpConst, c.kconst(runtime.Tag(m.Name)), 0, 0)
		c.compileExpr(m.Value)
		c.emit(ir.OpTableSet, 0, 0, 0)
	}
	c.inEnum--
	c.iotaNext = saved
}

// ----------------------------------------------------------- node literals

func (c *compiler) compileNodeLit(x *ast.NodeLit) {
	if len(x.Body) == 1 {
		if el, ok := x.Body[0].(*ast.NodeElement); ok {
			c.compileElement(el)
			return
		}
	}
	c.emit(ir.OpNodeFragment, 0, 0, 0)
	c.compileNodeBody(x.Body)
	c.emit(ir.OpNodeEnd, 0, 0, 0)
}

// compileNodeBody compiles parts, appending each into the node frame on
// top of the node stack. A spread appends as it goes and leaves nothing,
// so it skips the splice that follows every other part.
func (c *compiler) compileNodeBody(parts []ast.NodePart) {
	for _, part := range parts {
		if sp, ok := part.(*ast.NodeSpread); ok {
			c.compileExpr(sp.X)
			c.emit(ir.OpNodeSpread, 0, 0, 0)
			continue
		}
		c.compilePart(part)
		c.emit(ir.OpNodeSplice, 0, 0, 0)
	}
}

func (c *compiler) compilePart(part ast.NodePart) {
	pos, _ := part.(interface {
		Span() (token.Pos, token.Pos)
	}).Span()
	c.line = int32(pos.Line)
	switch x := part.(type) {
	case *ast.NodeElement:
		c.compileElement(x)
	case *ast.NodeText:
		c.emit(ir.OpNodeText, c.kconst(runtime.Str(x.Text)), 0, 0)
	case *ast.NodeComment:
		c.emit(ir.OpNodeComment, c.kconst(runtime.Str(x.Text)), 0, 0)
	case *ast.NodeSplice:
		c.compileExpr(x.X)
	default:
		cfail("unsupported node part %T", part)
	}
}

// compileElement emits the element's frame: OpNodeEl opens it (attr
// values and child splices target the frame on top), the body appends
// into it, and OpNodeEnd closes it so following parts splice into the
// enclosing element again.
func (c *compiler) compileElement(x *ast.NodeElement) {
	c.emit(ir.OpNodeEl, c.kconst(runtime.Str(x.Name)), 0, 0)
	for i := range x.Attrs {
		a := &x.Attrs[i]
		if a.Value != nil {
			c.compileExpr(a.Value)
		} else {
			c.emit(ir.OpConst, c.kconst(runtime.Str(a.Literal)), 0, 0)
		}
		c.emit(ir.OpNodeAttr, c.kconst(runtime.Str(a.Name)), 0, 0)
	}
	if !x.SelfClose {
		c.compileNodeBody(x.Body)
	}
	c.emit(ir.OpNodeEnd, 0, 0, 0)
}

// --------------------------------------------------------------- patterns

func (c *compiler) compilePattern(p ast.Pattern) int {
	ip := c.lowerPattern(p)
	f := c.fn()
	f.Pats = append(f.Pats, ip)
	return len(f.Pats) - 1
}

func (c *compiler) lowerPattern(p ast.Pattern) *ir.Pattern {
	switch x := p.(type) {
	case *ast.PatWildcard:
		return &ir.Pattern{Kind: ir.PatWild}
	case *ast.PatOptAny:
		// `?`: matches :Some[anything] or :None (Section 16).
		return &ir.Pattern{Kind: ir.PatOptAny}
	case *ast.PatSome:
		return &ir.Pattern{Kind: ir.PatTagged, Tag: "Some",
			Elems: []*ir.Pattern{c.lowerPattern(x.Pat)}}
	case *ast.PatLit:
		return &ir.Pattern{Kind: ir.PatLit, Lit: c.litValue(x.Lit)}
	case *ast.PatBind:
		sym := c.declSym(x)
		slot := c.declare(sym)
		return &ir.Pattern{Kind: ir.PatBind, Slot: slot.slot}
	case *ast.PatList:
		pat := &ir.Pattern{Kind: ir.PatList, RestSlot: -1}
		for _, e := range x.Elems {
			pat.Elems = append(pat.Elems, c.lowerPattern(e))
		}
		if x.Rest != nil {
			sym := c.declSym(x) // sema binds `..rest` on the PatList node
			b := c.declare(sym)
			pat.Rest = true
			pat.RestSlot = b.slot
		}
		return pat
	case *ast.PatTagged:
		// Rest doubles as the "brackets were written" flag: a bare :Tag
		// matches any payload with that tag, :Tag[p...] requires the
		// payload to match element-wise (exactly; `..` is not valid
		// inside tagged payloads per the Section 19 grammar).
		pat := &ir.Pattern{Kind: ir.PatTagged, Tag: x.Tag, RestSlot: -1}
		if x.Brackets {
			pat.Rest = true
			for _, e := range x.Elems {
				pat.Elems = append(pat.Elems, c.lowerPattern(e))
			}
		}
		return pat
	default:
		cfail("unsupported pattern %T", p)
		return nil
	}
}

func (c *compiler) litValue(e ast.Expr) runtime.Value {
	switch x := e.(type) {
	case *ast.IntLit:
		return runtime.Int(x.Value)
	case *ast.FloatLit:
		return runtime.Float(x.Value)
	case *ast.StringLit:
		return runtime.Str(x.Value)
	case *ast.CharLit:
		return runtime.Char(x.Value)
	case *ast.BoolLit:
		return runtime.Bool(x.Value)
	case *ast.UnitLit:
		return runtime.Unit()
	case *ast.TagLit:
		return runtime.Tag(x.Name)
	case *ast.OptNone:
		return runtime.TaggedOf("None", nil)
	case *ast.NullLit:
		return runtime.TaggedOf("None", nil)
	}
	cfail("non-literal in pattern")
	return runtime.Unit()
}

var _ = diag.Span{} // keep diag import for future source-mapped errors
