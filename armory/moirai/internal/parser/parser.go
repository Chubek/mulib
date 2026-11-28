// Package parser implements the recursive-descent parser for Domscript's
// Section 19 grammar. Operator precedence and associativity follow the
// spec's operator table via a precedence-climbing expression parser.
//
// On a syntax error the parser records a diagnostic, synchronizes at the
// next statement boundary, and continues, so one file yields all of its
// syntax errors in a single pass.
package parser

import (
	"fmt"
	"strconv"
	"strings"

	"domscript/internal/ast"
	"domscript/internal/diag"
	"domscript/internal/lexer"
	"domscript/internal/token"
)

// Parser holds the token stream and parse state for one chunk.
type Parser struct {
	toks  []token.Token
	pos   int
	diags *diag.Set
	src   string
	chunk string
	xst   bool
	// markup element stack for close-tag matching inside a node literal
	elemStack    []string
	depth        int // statement nesting depth, for recovery heuristics
	topLevelSeen bool
	sawDialect   bool
}

// ParseOptions configures optional parser extensions.
type ParseOptions struct {
	XST bool
}

// Parse lexes and parses src, returning the program (best-effort when
// diagnostics were produced).
func Parse(src, chunk string, diags *diag.Set, opts ...ParseOptions) *ast.Program {
	opt := ParseOptions{}
	if len(opts) > 0 {
		opt = opts[0]
	}
	toks := lexer.Scan(src, chunk, diags)
	p := &Parser{toks: toks, diags: diags, src: src, chunk: chunk, xst: opt.XST}
	prog := &ast.Program{}
	prog.Pos = toks[0].Pos
	for !p.at(token.EOF) {
		p.skipTerminators()
		if p.at(token.EOF) {
			break
		}
		if p.at(token.RBrace) {
			p.errorf("unexpected '}' at top level")
			p.next()
			continue
		}
		st := p.parseStatement(true)
		if st == nil {
			p.recover()
			continue
		}
		prog.Stmts = append(prog.Stmts, st)
		switch st.(type) {
		case *ast.Import, *ast.Dialect:
		default:
			p.topLevelSeen = true
		}
		if !p.at(token.EOF) && !p.at(token.RBrace) && !p.expectTerminator() {
			p.recover()
		}
	}
	if len(prog.Stmts) > 0 {
		prog.End = endOf(prog.Stmts[len(prog.Stmts)-1])
	} else {
		prog.End = toks[0].Pos
	}
	if len(p.elemStack) != 0 {
		p.errorf("unclosed element %q in node literal", p.elemStack[len(p.elemStack)-1])
	}
	return prog
}

// spanner is satisfied by every AST node (they all embed ast.Node).
type spanner = interface{ Span() (token.Pos, token.Pos) }

func startOf(n any) token.Pos {
	if s, ok := n.(spanner); ok {
		p, _ := s.Span()
		return p
	}
	return token.Pos{}
}

func endOf(n any) token.Pos {
	if s, ok := n.(spanner); ok {
		_, e := s.Span()
		return e
	}
	return token.Pos{}
}

func (p *Parser) cur() token.Token { return p.toks[p.pos] }
func (p *Parser) at(k token.Kind) bool {
	return p.cur().Kind == k
}
func (p *Parser) next() token.Token {
	t := p.toks[p.pos]
	if p.pos < len(p.toks)-1 {
		p.pos++
	}
	return t
}
func (p *Parser) peek(n int) token.Token {
	if p.pos+n < len(p.toks) {
		return p.toks[p.pos+n]
	}
	return p.toks[len(p.toks)-1]
}

func (p *Parser) errorf(format string, args ...any) {
	t := p.cur()
	p.diags.Errorf(diag.Span{Start: t.Pos, End: t.End}, format, args...)
}

func (p *Parser) accept(k token.Kind) bool {
	if p.at(k) {
		p.next()
		return true
	}
	return false
}

func (p *Parser) expect(k token.Kind, what string) (token.Token, bool) {
	if p.at(k) {
		return p.next(), true
	}
	p.errorf("expected %s, found %s", what, p.cur().Kind)
	return token.Token{}, false
}

// skipTerminators collapses consecutive statement terminators (Section 3.3).
func (p *Parser) skipTerminators() {
	for p.at(token.Newline) || p.at(token.Semi) {
		p.next()
	}
}

func (p *Parser) expectTerminator() bool {
	if p.at(token.Newline) || p.at(token.Semi) || p.at(token.EOF) {
		p.skipTerminators()
		return true
	}
	p.errorf("expected end of statement, found %s", p.cur().Kind)
	return false
}

// recover skips to the next statement boundary after a syntax error.
func (p *Parser) recover() {
	for !p.at(token.EOF) {
		k := p.cur().Kind
		if k == token.Newline || k == token.Semi {
			p.skipTerminators()
			return
		}
		if k == token.RBrace || k == token.NodeClose {
			return
		}
		p.next()
	}
}

// ---------------------------------------------------------------- statements

func (p *Parser) parseStatement(top bool) ast.Stmt {
	t := p.cur()
	start := t.Pos
	if top && p.xst && t.Kind == token.Ident {
		switch t.Text {
		case "dialect":
			return p.parseDialect()
		case "extend":
			return p.parseExtend()
		}
	}
	if top && !p.xst && t.Kind == token.Ident {
		switch t.Text {
		case "dialect", "extend":
			p.errorf("%q requires -X", t.Text)
			p.next()
			return nil
		}
	}
	// Top-level `grammar` / `absyn` / `asdl` declarations (contextual:
	// `grammar Name { ... }`). Checked before expression parsing so the
	// leading identifier is not mistaken for a variable.
	if top && t.Kind == token.Ident && (t.Text == "grammar" || t.Text == "absyn" || t.Text == "asdl") {
		if p.peek(1).Kind == token.Ident && p.peek(2).Kind == token.LBrace {
			if t.Text == "grammar" {
				return p.parseGrammarDecl()
			}
			return p.parseAbsynDecl()
		}
	}
	switch t.Kind {
	case token.KwProgram:
		if !top {
			p.errorf("program declaration is only allowed at top level")
			return nil
		}
		return p.parseProgram()

	case token.KwModule:
		if !top {
			p.errorf("module declaration is only allowed at top level")
			return nil
		}
		return p.parseModule()

	case token.KwImport:
		p.next()
		modName, modEnd, ok := p.parseNamespacedIdent("module name after 'import'")
		if !ok {
			return nil
		}
		im := &ast.Import{Node: ast.Node{Pos: start, End: modEnd}, Module: modName}
		switch {
		case p.accept(token.KwAs):
			id, ok := p.expect(token.Ident, "alias after 'as'")
			if !ok {
				return nil
			}
			im.Alias, im.End = id.Text, id.End
		case p.accept(token.Colon):
			// `import m : alias` (single, backward compatible) vs
			// `import m : a, b, c` (selective). A comma anywhere
			// (including trailing) selects selective mode.
			first, ok := p.expect(token.Ident, "name after ':'")
			if !ok {
				return nil
			}
			names := []string{first.Text}
			end := first.End
			selective := false
			for p.accept(token.Comma) {
				selective = true
				// Trailing comma: `import m : a,` means selective [a].
				if p.at(token.Ident) {
					id := p.next()
					names = append(names, id.Text)
					end = id.End
				} else {
					break
				}
			}
			if selective {
				im.Symbols, im.End = names, end
			} else {
				im.Alias, im.End = first.Text, first.End
			}
		case p.cur().Kind == token.Tag && p.cur().Text == "":
			// Accept whitespace separated "import std.mod : alias" syntax where the
			// scanner emits an empty tag token for the colon.
			p.next()
			id, ok := p.expect(token.Ident, "alias after ':'")
			if !ok {
				return nil
			}
			// Peek for selective continuation `, ...`.
			if p.at(token.Comma) {
				names := []string{id.Text}
				end := id.End
				for p.accept(token.Comma) {
					if p.at(token.Ident) {
						nx := p.next()
						names = append(names, nx.Text)
						end = nx.End
					} else {
						break
					}
				}
				im.Symbols, im.End = names, end
			} else {
				im.Alias, im.End = id.Text, id.End
			}
		}
		if p.accept(token.KwForm) {
			switch p.cur().Kind {
			case token.Ident, token.String:
				lib := p.next()
				im.Library, im.End = lib.Text, lib.End
			default:
				p.errorf("expected library name after 'form'")
				return nil
			}
		}
		return im

	case token.KwLet:
		return p.parseLet(true, start)

	case token.KwConst:
		return p.parseLet(false, start)

	case token.KwWhile:
		p.next()
		cond := p.parseExpr()
		if cond == nil {
			return nil
		}
		body := p.parseBlock()
		if body == nil {
			return nil
		}
		return &ast.While{Node: ast.Node{Pos: start, End: body.End}, Cond: cond, Body: body}

	case token.KwFor:
		p.next()
		id, ok := p.expect(token.Ident, "loop variable after 'for'")
		if !ok {
			return nil
		}
		if _, ok := p.expect(token.KwIn, "'in' after loop variable"); !ok {
			return nil
		}
		iter := p.parseExpr()
		if iter == nil {
			return nil
		}
		body := p.parseBlock()
		if body == nil {
			return nil
		}
		return &ast.For{Node: ast.Node{Pos: start, End: body.End}, Var: id.Text, Iter: iter, Body: body}

	case token.KwOn:
		if o := p.parseOn(); o != nil {
			return o
		}
		return nil

	case token.KwReturn:
		p.next()
		// `return` with no value: terminator (or structural end) follows.
		if p.exprCanStart(p.cur().Kind) {
			v := p.parseExpr()
			if v == nil {
				return nil
			}
			return &ast.Return{Node: ast.Node{Pos: start, End: endOf(v)}, Value: v}
		}
		return &ast.Return{Node: ast.Node{Pos: start, End: t.End}}

	case token.KwBreak:
		p.next()
		return &ast.Break{Node: ast.Node{Pos: start, End: t.End}}

	case token.KwContinue:
		p.next()
		return &ast.Continue{Node: ast.Node{Pos: start, End: t.End}}

	case token.KwIf:
		if it := p.parseIf(); it != nil {
			return it
		}
		return nil

	case token.KwMatch:
		if m := p.parseMatch(); m != nil {
			return &ast.ExprStmt{Node: ast.Node{Pos: start, End: m.End}, X: m}
		}
		return nil

	case token.Tilde:
		// Prefix effect statement: `~ expr` Terminator Statement. A bare
		// `~` with no operand is the EffectResult primary as a statement.
		if !p.exprCanStart(p.peek(1).Kind) {
			p.next()
			return &ast.ExprStmt{Node: ast.Node{Pos: start, End: p.toks[p.pos-1].End},
				X: &ast.EffectResult{Node: ast.Node{Pos: start, End: p.toks[p.pos-1].End}}}
		}
		p.next()
		operand := p.parseExpr()
		if operand == nil {
			return nil
		}
		if !p.expectTerminator() {
			return nil
		}
		p.skipTerminators()
		body := p.parseStatement(false)
		if body == nil {
			return nil
		}
		return &ast.Effect{Node: ast.Node{Pos: start, End: endOf(body)}, Operand: operand, Body: body}

	case token.LBrace:
		blk := p.parseBlock()
		if blk == nil {
			return nil
		}
		return &ast.BlockStmt{Node: ast.Node{Pos: start, End: blk.End}, Block: blk}
	}

	// Expression statement, assignment, or the infix effect statement
	// `expr ~ follower` shown in Section 10.
	x := p.parseExpr()
	if x == nil {
		return nil
	}
	end := endOf(x)
	if p.at(token.Assign) {
		p.next()
		switch x.(type) {
		case *ast.Ident, *ast.Index, *ast.Field:
		default:
			p.errorf("invalid assignment target")
			return nil
		}
		v := p.parseExpr()
		if v == nil {
			return nil
		}
		return &ast.Assign{Node: ast.Node{Pos: start, End: endOf(v)}, Target: x, Value: v}
	}
	// Compound assignment (`+=`, `-=`, `*=`, `/=`, `%=`) desugars to
	// `x = x op y`. The target is duplicated (safe for Ident; Index/Field
	// with side-effecting subexpressions evaluate twice — documented).
	if p.at(token.PlusAssign) || p.at(token.MinusAssign) || p.at(token.StarAssign) || p.at(token.SlashAssign) || p.at(token.PercentAssign) {
		var op ast.BinOp
		switch p.cur().Kind {
		case token.PlusAssign:
			op = ast.OpAdd
		case token.MinusAssign:
			op = ast.OpSub
		case token.StarAssign:
			op = ast.OpMul
		case token.SlashAssign:
			op = ast.OpDiv
		default:
			op = ast.OpMod
		}
		p.next()
		switch x.(type) {
		case *ast.Ident, *ast.Index, *ast.Field:
		default:
			p.errorf("invalid assignment target")
			return nil
		}
		v := p.parseExpr()
		if v == nil {
			return nil
		}
		lhs := cloneAssignTarget(x)
		if lhs == nil {
			p.errorf("invalid assignment target")
			return nil
		}
		bin := &ast.Binary{Node: ast.Node{Pos: startOf(lhs), End: endOf(v)}, Op: op, L: lhs, R: v}
		return &ast.Assign{Node: ast.Node{Pos: start, End: endOf(v)}, Target: x, Value: bin}
	}
	if p.at(token.Tilde) {
		p.next()
		body := p.parseStatement(false)
		if body == nil {
			return nil
		}
		return &ast.Effect{Node: ast.Node{Pos: start, End: endOf(body)}, Operand: x, Body: body}
	}
	return &ast.ExprStmt{Node: ast.Node{Pos: start, End: end}, X: x}
}

func cloneAssignTarget(x ast.Expr) ast.Expr {
	// Deep copy via the shared cloner (handles Ident/Index/Field plus
	// any nested expressions such as calls in `f()[0] += 1`).
	return cloneExpr(x)
}

func (p *Parser) parseBlock() *ast.Block {
	// Allow Allman style (`)\n{`, `if cond\n{`): skip a leading newline
	// before the opening brace (the lexer only emits it when the previous
	// token can end an expression, e.g. `)`).
	p.skipTerminators()
	open, ok := p.expect(token.LBrace, "'{'")
	if !ok {
		return nil
	}
	blk := &ast.Block{Node: ast.Node{Pos: open.Pos}}
	for {
		p.skipTerminators()
		if p.at(token.EOF) {
			p.errorf("unexpected end of file: unclosed block")
			return nil
		}
		if p.at(token.RBrace) {
			closing := p.next()
			blk.End = closing.End
			return blk
		}
		st := p.parseStatement(false)
		if st == nil {
			// Recover inside the block: skip to a boundary and keep going.
			for !p.at(token.EOF) && !p.at(token.RBrace) &&
				!p.at(token.Newline) && !p.at(token.Semi) {
				p.next()
			}
			p.skipTerminators()
			if p.at(token.EOF) {
				return nil
			}
			continue
		}
		blk.Stmts = append(blk.Stmts, st)
		if !p.at(token.RBrace) && !p.at(token.EOF) && !p.expectTerminator() {
			for !p.at(token.EOF) && !p.at(token.RBrace) &&
				!p.at(token.Newline) && !p.at(token.Semi) {
				p.next()
			}
		}
	}
}

func (p *Parser) parseIf() *ast.If {
	kw, ok := p.expect(token.KwIf, "'if'")
	if !ok {
		return nil
	}
	cond := p.parseExpr()
	if cond == nil {
		return nil
	}
	then := p.parseBlock()
	if then == nil {
		return nil
	}
	node := &ast.If{Node: ast.Node{Pos: kw.Pos, End: then.End}, Cond: cond, Then: then}
	// `else` may sit on the next line; save/restore the index when the
	// next significant token is not else.
	save := p.pos
	p.skipTerminators()
	if p.at(token.KwElse) {
		p.next()
		if p.at(token.KwIf) {
			els := p.parseIf()
			if els == nil {
				return nil
			}
			node.Else = els
			node.End = els.End
		} else {
			blk := p.parseBlock()
			if blk == nil {
				return nil
			}
			node.Else = blk
			node.End = blk.End
		}
	} else {
		p.pos = save
	}
	return node
}

func (p *Parser) parseMatch() *ast.Match {
	kw, ok := p.expect(token.KwMatch, "'match'")
	if !ok {
		return nil
	}
	// A match with no scrutinee reads the current effect result: the
	// Section 10 infix form is "expr ~ match { arms }".
	var scrut ast.Expr
	p.skipTerminators()
	if p.at(token.LBrace) {
		scrut = &ast.EffectResult{Node: ast.Node{Pos: kw.Pos, End: kw.End}}
	} else {
		scrut = p.parseExpr()
		if scrut == nil {
			return nil
		}
	}
	p.skipTerminators()
	if _, ok := p.expect(token.LBrace, "'{' after match scrutinee"); !ok {
		return nil
	}
	m := &ast.Match{Node: ast.Node{Pos: kw.Pos}, Scrutinee: scrut}
	for {
		p.skipTerminators()
		if p.at(token.RBrace) {
			closing := p.next()
			m.End = closing.End
			if len(m.Arms) == 0 {
				p.diags.Errorf(diag.Span{Start: kw.Pos, End: closing.End},
					"match has no arms; a match with no matching arm raises")
			}
			return m
		}
		if p.at(token.EOF) {
			p.errorf("unexpected end of file in match arms")
			return nil
		}
		patStart := p.cur().Pos
		pat := p.parsePattern()
		if pat == nil {
			return nil
		}
		if _, ok := p.expect(token.FatArrow, "'=>' after pattern"); !ok {
			return nil
		}
		var body ast.Expr
		if p.at(token.LBrace) {
			blk := p.parseBlock()
			if blk == nil {
				return nil
			}
			body = blk
		} else {
			body = p.parseExpr()
			if body == nil {
				return nil
			}
		}
		arm := ast.MatchArm{Node: ast.Node{Pos: patStart, End: endOf(body)}, Pat: pat, Body: body}
		m.Arms = append(m.Arms, arm)
		// Arms separate by newline or optional comma (grammar + gdesc note).
		p.skipTerminators()
		p.accept(token.Comma)
	}
}

func (p *Parser) parseOn() *ast.On {
	kw, ok := p.expect(token.KwOn, "'on'")
	if !ok {
		return nil
	}
	target := p.parseExpr()
	if target == nil {
		return nil
	}
	ev, ok := p.expect(token.Ident, "event name after target expression")
	if !ok {
		return nil
	}
	if _, ok := p.expect(token.LParen, "'(' after event name"); !ok {
		return nil
	}
	params, ok := p.parseParams(token.RParen)
	if !ok {
		return nil
	}
	if _, ok := p.expect(token.RParen, "')' after handler parameters"); !ok {
		return nil
	}
	body := p.parseBlock()
	if body == nil {
		return nil
	}
	return &ast.On{Node: ast.Node{Pos: kw.Pos, End: body.End}, Target: target, Event: ev.Text, Params: params, Body: body}
}

func (p *Parser) parseDialect() ast.Stmt {
	kw := p.next()
	if p.topLevelSeen {
		p.errorf("dialect declaration must appear before other statements")
	}
	if p.sawDialect {
		p.errorf("multiple dialect declarations")
	}
	lit, ok := p.expect(token.String, "dialect name string")
	if !ok {
		return nil
	}
	if lit.Text == "" {
		p.diags.Errorf(diag.Span{Start: lit.Pos, End: lit.End}, "dialect name cannot be empty")
	}
	p.sawDialect = true
	return &ast.Dialect{Node: ast.Node{Pos: kw.Pos, End: lit.End}, Name: lit.Text}
}

func (p *Parser) parseExtend() ast.Stmt {
	kw := p.next()
	if !p.acceptIdent("using") {
		p.errorf("expected 'using' after extend")
		return nil
	}
	if !p.at(token.Ident) {
		p.errorf("expected parser reference after 'using'")
		return nil
	}
	ref := p.next()
	body, end, ok := p.parseOpaqueBlock()
	if !ok {
		return nil
	}
	return &ast.Extend{Node: ast.Node{Pos: kw.Pos, End: end}, Using: ref.Text, Body: body}
}

func (p *Parser) parseOpaqueBlock() (string, token.Pos, bool) {
	open, ok := p.expect(token.LBrace, "'{'")
	if !ok {
		return "", token.Pos{}, false
	}
	startOff := open.End.Off
	depth := 1
	for depth > 0 {
		if p.at(token.EOF) {
			p.errorf("unexpected end of file in extension block")
			return "", token.Pos{}, false
		}
		switch p.cur().Kind {
		case token.LBrace:
			depth++
			p.next()
		case token.RBrace:
			depth--
			if depth == 0 {
				closing := p.next()
				return p.src[startOff:closing.Pos.Off], closing.End, true
			}
			p.next()
		default:
			p.next()
		}
	}
	return "", token.Pos{}, false
}

func (p *Parser) parseDottedIdent(what string) (string, token.Pos, bool) {
	var parts []string
	id, ok := p.expect(token.Ident, what)
	if !ok {
		return "", token.Pos{}, false
	}
	parts = append(parts, id.Text)
	end := id.End
	for p.accept(token.Dot) {
		seg, ok := p.expect(token.Ident, what)
		if !ok {
			return "", token.Pos{}, false
		}
		parts = append(parts, seg.Text)
		end = seg.End
	}
	return strings.Join(parts, "."), end, true
}

func (p *Parser) parseNamespacedIdent(what string) (string, token.Pos, bool) {
	name, end, ok := p.parseDottedIdent(what)
	if !ok {
		return "", token.Pos{}, false
	}
	if !strings.Contains(name, ".") {
		p.errorf("%s requires a dotted namespace", what)
		return "", token.Pos{}, false
	}
	return name, end, true
}

func (p *Parser) parseProgram() ast.Stmt {
	kw := p.next()
	if kw.Kind != token.KwProgram {
		p.errorf("expected program declaration")
		return nil
	}
	name, end, ok := p.parseDottedIdent("program name after 'program'")
	if !ok {
		return nil
	}
	if strings.Contains(name, ".") {
		p.errorf("program name after 'program' cannot be namespaced")
		return nil
	}
	return &ast.ProgramDecl{Node: ast.Node{Pos: kw.Pos, End: end}, Name: name}
}

func (p *Parser) parseModule() ast.Stmt {
	kw := p.next()
	if kw.Kind != token.KwModule {
		p.errorf("expected module declaration")
		return nil
	}
	start := kw.Pos
	name, end, ok := p.parseNamespacedIdent("module name after 'module'")
	if !ok {
		return nil
	}
	return &ast.Module{Node: ast.Node{Pos: start, End: end}, Name: name}
}

func (p *Parser) parseLet(mutable bool, start token.Pos) ast.Stmt {
	p.next()
	id, ok := p.expect(token.Ident, "identifier in declaration")
	if !ok {
		return nil
	}
	if _, ok := p.expect(token.Assign, "'=' in declaration"); !ok {
		return nil
	}
	v := p.parseExpr()
	if v == nil {
		return nil
	}
	return &ast.Let{Node: ast.Node{Pos: start, End: endOf(v)}, Name: id.Text, Value: v, Mutable: mutable}
}

func (p *Parser) acceptIdent(name string) bool {
	if p.at(token.Ident) && p.cur().Text == name {
		p.next()
		return true
	}
	return false
}

// parseParams parses `Ident {"," Ident} [","] ["," Ident ".."]` up to (but
// not consuming) the end token.
func (p *Parser) parseParams(end token.Kind) ([]ast.Param, bool) {
	var params []ast.Param
	for {
		if p.at(end) {
			return params, true
		}
		id, ok := p.expect(token.Ident, "parameter name")
		if !ok {
			return nil, false
		}
		prm := ast.Param{Node: ast.Node{Pos: id.Pos, End: id.End}, Name: id.Text}
		if p.at(token.DotDot) {
			p.next()
			prm.Rest = true
			prm.Node.End = p.toks[p.pos-1].End
			params = append(params, prm)
			p.accept(token.Comma) // tolerated trailing comma
			return params, true
		}
		params = append(params, prm)
		if !p.accept(token.Comma) {
			return params, true
		}
	}
}

// --------------------------------------------------------------- expressions

func (p *Parser) parseExpr() ast.Expr { return p.parseAssert() }

// parseAssert handles the `X ~ Err(...)` typed-error form. When `~` is
// followed by an `*Error(...)` call it desugars (see desugarAssert) and
// consumes it; any other `~` is left for statement-level effect parsing.
func (p *Parser) parseAssert() ast.Expr {
	l := p.parseOr()
	if l == nil {
		return nil
	}
	if p.at(token.Tilde) && p.isErrorHandlerStart() {
		tilde := p.next()
		h := p.parseOr()
		if h == nil {
			return nil
		}
		return p.desugarAssert(l, h, tilde.Pos)
	}
	return l
}

// isErrorHandlerStart reports whether `~` is followed by `Name(...)` where
// Name is `Error` or ends in `Error` (IOError, ParseError, ...).
func (p *Parser) isErrorHandlerStart() bool {
	if !p.at(token.Tilde) {
		return false
	}
	n1 := p.peek(1)
	if n1.Kind != token.Ident {
		return false
	}
	name := n1.Text
	if name != "Error" && !(len(name) > 5 && name[len(name)-5:] == "Error") {
		return false
	}
	return p.peek(2).Kind == token.LParen
}

// desugarAssert builds `match ~X { :Success[x] => ..., :Error[e] => ... }`
// from `X ~ H`. Success payloads unwrap; error payloads (value or Go
// effect) evaluate H and Raise it. H is shared (no bindings inside).
func (p *Parser) desugarAssert(l, h ast.Expr, tildePos token.Pos) ast.Expr {
	lPos, lEnd := startOf(l), endOf(l)
	hEnd := endOf(h)
	wholePos := lPos
	wholeEnd := hEnd
	// ~l
	effX := &ast.Unary{Node: ast.Node{Pos: lPos, End: lEnd}, Op: ast.OpEff, X: l}
	xval := &ast.PatBind{Node: ast.Node{Pos: tildePos, End: tildePos}, Name: "__assert_xval"}
	emsg := &ast.PatBind{Node: ast.Node{Pos: tildePos, End: tildePos}, Name: "__assert_emsg"}
	mkIdent := func(name string) *ast.Ident {
		return &ast.Ident{Node: ast.Node{Pos: tildePos, End: tildePos}, Name: name}
	}
	mkCall := func(fn string, args ...ast.Expr) *ast.Call {
		return &ast.Call{Node: ast.Node{Pos: tildePos, End: hEnd}, Fn: mkIdent(fn), Args: args}
	}
	mkBlock := func(x ast.Expr) *ast.Block {
		var st ast.Stmt = &ast.ExprStmt{Node: ast.Node{Pos: startOf(x), End: endOf(x)}, X: x}
		return &ast.Block{Node: ast.Node{Pos: startOf(x), End: endOf(x)}, Stmts: []ast.Stmt{st}}
	}
	// if IsSuccess(xval) { Unwrap(xval) } else if IsError(xval) { Raise(h) } else { xval }
	innerIf := &ast.If{
		Node: ast.Node{Pos: tildePos, End: hEnd},
		Cond: mkCall("IsSuccess", mkIdent("__assert_xval")),
		Then: mkBlock(mkCall("Unwrap", mkIdent("__assert_xval"))),
		Else: &ast.If{
			Node: ast.Node{Pos: tildePos, End: hEnd},
			Cond: mkCall("IsError", mkIdent("__assert_xval")),
			Then: mkBlock(mkCall("Raise", h)),
			Else: mkBlock(mkIdent("__assert_xval")),
		},
	}
	arm1 := ast.MatchArm{
		Node: ast.Node{Pos: tildePos, End: hEnd},
		Pat:  &ast.PatTagged{Node: ast.Node{Pos: tildePos, End: tildePos}, Tag: "Success", Elems: []ast.Pattern{xval}, Brackets: true},
		Body: innerIf,
	}
	arm2 := ast.MatchArm{
		Node: ast.Node{Pos: tildePos, End: hEnd},
		Pat:  &ast.PatTagged{Node: ast.Node{Pos: tildePos, End: tildePos}, Tag: "Error", Elems: []ast.Pattern{emsg}, Brackets: true},
		Body: mkCall("Raise", h),
	}
	// Typed Go errors (`~X` is `:IOError`, `:ParseError`, ...) fall
	// through the `:Error` arm; catch them (and anything else) here.
	arm3 := ast.MatchArm{
		Node: ast.Node{Pos: tildePos, End: hEnd},
		Pat:  &ast.PatWildcard{Node: ast.Node{Pos: tildePos, End: tildePos}},
		Body: mkCall("Raise", h),
	}
	_ = wholePos
	_ = wholeEnd
	return &ast.Match{
		Node:      ast.Node{Pos: lPos, End: hEnd},
		Scrutinee: effX,
		Arms:      []ast.MatchArm{arm1, arm2, arm3},
	}
}

func (p *Parser) parseOr() ast.Expr {
	l := p.parseAnd()
	if l == nil {
		return nil
	}
	for p.at(token.KwOr) {
		p.next()
		r := p.parseAnd()
		if r == nil {
			return nil
		}
		l = &ast.Binary{Node: ast.Node{Pos: startOf(l), End: endOf(r)},
			Op: ast.OpOr, L: l, R: r}
	}
	return l
}

func (p *Parser) parseAnd() ast.Expr {
	l := p.parseEq()
	if l == nil {
		return nil
	}
	for p.at(token.KwAnd) {
		p.next()
		r := p.parseEq()
		if r == nil {
			return nil
		}
		l = &ast.Binary{Node: ast.Node{Pos: startOf(l), End: endOf(r)},
			Op: ast.OpAnd, L: l, R: r}
	}
	return l
}

func (p *Parser) parseEq() ast.Expr {
	l := p.parseRel()
	if l == nil {
		return nil
	}
	for p.at(token.Eq) || p.at(token.Neq) {
		op := ast.OpEq
		if p.at(token.Neq) {
			op = ast.OpNeq
		}
		p.next()
		r := p.parseRel()
		if r == nil {
			return nil
		}
		l = &ast.Binary{Node: ast.Node{Pos: startOf(l), End: endOf(r)}, Op: op, L: l, R: r}
	}
	return l
}

func (p *Parser) parseRel() ast.Expr {
	l := p.parseQuery()
	if l == nil {
		return nil
	}
	for {
		var op ast.BinOp
		switch p.cur().Kind {
		case token.Lt:
			op = ast.OpLt
		case token.Le:
			op = ast.OpLe
		case token.Gt:
			op = ast.OpGt
		case token.Ge:
			op = ast.OpGe
		default:
			return l
		}
		p.next()
		r := p.parseQuery()
		if r == nil {
			return nil
		}
		l = &ast.Binary{Node: ast.Node{Pos: startOf(l), End: endOf(r)}, Op: op, L: l, R: r}
	}
}

func (p *Parser) parseQuery() ast.Expr {
	l := p.parseRange()
	if l == nil {
		return nil
	}
	if p.at(token.Dollar) {
		p.next()
		r := p.parseRange()
		if r == nil {
			return nil
		}
		return &ast.Binary{Node: ast.Node{Pos: startOf(l), End: endOf(r)},
			Op: ast.OpQuery, L: l, R: r}
	}
	return l
}

func (p *Parser) parseRange() ast.Expr {
	l := p.parseAdd()
	if l == nil {
		return nil
	}
	if p.at(token.DotDot) {
		p.next()
		r := p.parseAdd()
		if r == nil {
			return nil
		}
		return &ast.Binary{Node: ast.Node{Pos: startOf(l), End: endOf(r)},
			Op: ast.OpRange, L: l, R: r}
	}
	return l
}

func (p *Parser) parseAdd() ast.Expr {
	l := p.parseMul()
	if l == nil {
		return nil
	}
	for p.at(token.Plus) || p.at(token.Minus) {
		op := ast.OpAdd
		if p.at(token.Minus) {
			op = ast.OpSub
		}
		p.next()
		r := p.parseMul()
		if r == nil {
			return nil
		}
		l = &ast.Binary{Node: ast.Node{Pos: startOf(l), End: endOf(r)}, Op: op, L: l, R: r}
	}
	return l
}

func (p *Parser) parseMul() ast.Expr {
	l := p.parseUnary()
	if l == nil {
		return nil
	}
	for p.at(token.Star) || p.at(token.Slash) || p.at(token.Percent) {
		op := ast.OpMul
		switch p.cur().Kind {
		case token.Slash:
			op = ast.OpDiv
		case token.Percent:
			op = ast.OpMod
		}
		p.next()
		r := p.parseUnary()
		if r == nil {
			return nil
		}
		l = &ast.Binary{Node: ast.Node{Pos: startOf(l), End: endOf(r)}, Op: op, L: l, R: r}
	}
	return l
}

func (p *Parser) parseUnary() ast.Expr {
	t := p.cur()
	switch t.Kind {
	case token.KwNot:
		p.next()
		x := p.parseUnary()
		if x == nil {
			return nil
		}
		return &ast.Unary{Node: ast.Node{Pos: t.Pos, End: endOf(x)}, Op: ast.OpNot, X: x}
	case token.Minus:
		p.next()
		x := p.parseUnary()
		if x == nil {
			return nil
		}
		return &ast.Unary{Node: ast.Node{Pos: t.Pos, End: endOf(x)}, Op: ast.OpNeg, X: x}
	case token.Tilde:
		// Expression-position effect operand `~ Unary`; a bare `~` that is
		// not followed by an expression is the EffectResult primary.
		if p.exprCanStart(p.peek(1).Kind) {
			p.next()
			x := p.parseUnary()
			if x == nil {
				return nil
			}
			return &ast.Unary{Node: ast.Node{Pos: t.Pos, End: endOf(x)}, Op: ast.OpEff, X: x}
		}
		p.next()
		return &ast.EffectResult{Node: ast.Node{Pos: t.Pos, End: t.End}}
	case token.Question:
		p.next()
		// `?` alone is :None; `? Postfix` wraps in :Some.
		if p.postfixCanStart(p.cur().Kind) {
			x := p.parsePostfix()
			if x == nil {
				return nil
			}
			return &ast.Unary{Node: ast.Node{Pos: t.Pos, End: endOf(x)}, Op: ast.OpSome, X: x}
		}
		return &ast.OptNone{Node: ast.Node{Pos: t.Pos, End: t.End}}
	}
	return p.parsePostfix()
}

func (p *Parser) parsePostfix() ast.Expr {
	x := p.parsePrimary()
	if x == nil {
		return nil
	}
	for {
		switch p.cur().Kind {
		case token.LParen:
			p.next()
			var args []ast.Expr
			for !p.at(token.RParen) {
				a := p.parseExpr()
				if a == nil {
					return nil
				}
				args = append(args, a)
				if !p.accept(token.Comma) {
					break
				}
			}
			if _, ok := p.expect(token.RParen, "')' after arguments"); !ok {
				return nil
			}
			x = &ast.Call{Node: ast.Node{Pos: startOf(x), End: p.toks[p.pos-1].End}, Fn: x, Args: args}
		case token.LBracket:
			p.next()
			i := p.parseExpr()
			if i == nil {
				return nil
			}
			if _, ok := p.expect(token.RBracket, "']' after index"); !ok {
				return nil
			}
			x = &ast.Index{Node: ast.Node{Pos: startOf(x), End: p.toks[p.pos-1].End}, X: x, I: i}
		case token.Dot:
			p.next()
			id, ok := p.expect(token.Ident, "field name after '.'")
			if !ok {
				return nil
			}
			x = &ast.Field{Node: ast.Node{Pos: startOf(x), End: id.End}, X: x, Name: id.Text}
		default:
			return x
		}
	}
}

// exprCanStart reports whether a token can begin an expression (used to
// disambiguate `~` prefix vs `~` primary and `return` with/without value).
func (p *Parser) exprCanStart(k token.Kind) bool {
	switch k {
	case token.Ident, token.Int, token.Float, token.String, token.Char,
		token.Regex, token.Tag, token.Selector, token.KwTrue, token.KwFalse,
		token.KwNull, token.LParen, token.LBracket, token.TableOpen,
		token.NodeOpen, token.KwFn, token.KwEnum, token.KwNot, token.Minus,
		token.Question, token.Tilde, token.KwIf, token.KwMatch, token.KwOn:
		return true
	}
	return false
}

func (p *Parser) postfixCanStart(k token.Kind) bool {
	switch k {
	case token.Ident, token.Int, token.Float, token.String, token.Char,
		token.Regex, token.Tag, token.Selector, token.KwTrue, token.KwFalse,
		token.KwNull, token.LParen, token.LBracket, token.TableOpen,
		token.NodeOpen, token.KwFn, token.KwEnum, token.KwIf, token.KwMatch:
		return true
	}
	return false
}

func (p *Parser) parsePrimary() ast.Expr {
	t := p.cur()
	switch t.Kind {
	case token.Ident:
		p.next()
		return &ast.Ident{Node: ast.Node{Pos: t.Pos, End: t.End}, Name: t.Text}
	case token.Int:
		p.next()
		v, err := parseBasedInt(t.Text)
		if err != nil {
			p.diags.Errorf(diag.Span{Start: t.Pos, End: t.End}, "invalid integer literal %q", t.Text)
		}
		return &ast.IntLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Value: v, Text: t.Text}
	case token.Float:
		p.next()
		v, err := strconv.ParseFloat(t.Text, 64)
		if err != nil {
			p.diags.Errorf(diag.Span{Start: t.Pos, End: t.End}, "invalid float literal %q", t.Text)
		}
		return &ast.FloatLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Value: v, Text: t.Text}
	case token.String:
		p.next()
		return p.makeStringExpr(t)
	case token.Char:
		p.next()
		return &ast.CharLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Value: t.Text}
	case token.Regex:
		p.next()
		return &ast.RegexLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Pattern: t.Text}
	case token.Selector:
		p.next()
		return &ast.SelectorLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Src: t.Text}
	case token.KwTrue, token.KwFalse:
		p.next()
		return &ast.BoolLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Value: t.Kind == token.KwTrue}
	case token.KwNull:
		p.next()
		return &ast.NullLit{Node: ast.Node{Pos: t.Pos, End: t.End}}
	case token.Tag:
		p.next()
		if p.at(token.LBracket) {
			return p.parseTaggedList(t)
		}
		return &ast.TagLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Name: t.Text}
	case token.LParen:
		p.next()
		if p.at(token.RParen) {
			closing := p.next()
			return &ast.UnitLit{Node: ast.Node{Pos: t.Pos, End: closing.End}}
		}
		x := p.parseExpr()
		if x == nil {
			return nil
		}
		if _, ok := p.expect(token.RParen, "')'"); !ok {
			return nil
		}
		return x
	case token.LBracket:
		return p.parseListLit()
	case token.TableOpen:
		return p.parseTableLit()
	case token.NodeOpen:
		return p.parseNodeLit()
	case token.KwFn:
		return p.parseFn()
	case token.KwEnum:
		return p.parseEnum()
	case token.KwIf:
		return p.parseIf()
	case token.KwMatch:
		return p.parseMatch()
	case token.KwOn:
		if o := p.parseOn(); o != nil {
			return o
		}
		return nil
	case token.Tilde:
		p.next()
		return &ast.EffectResult{Node: ast.Node{Pos: t.Pos, End: t.End}}
	}
	p.errorf("expected expression, found %s", t.Kind)
	return nil
}

// parseBasedInt handles decimal, 0x, 0b, and 0o forms (underscores were
// stripped by the lexer).
func parseBasedInt(text string) (int64, error) {
	return strconv.ParseInt(text, 0, 64)
}

func (p *Parser) parseListLit() ast.Expr {
	open, ok := p.expect(token.LBracket, "'['")
	if !ok {
		return nil
	}
	lst := &ast.ListLit{Node: ast.Node{Pos: open.Pos}}
	for {
		if p.at(token.RBracket) {
			closing := p.next()
			lst.End = closing.End
			return lst
		}
		e := p.parseExpr()
		if e == nil {
			return nil
		}
		lst.Elems = append(lst.Elems, e)
		lst.End = endOf(e)
		if !p.accept(token.Comma) {
			break
		}
	}
	closing, ok := p.expect(token.RBracket, "']' after list elements")
	if !ok {
		return nil
	}
	lst.End = closing.End
	return lst
}

func (p *Parser) parseTableLit() ast.Expr {
	open, ok := p.expect(token.TableOpen, "'%{'")
	if !ok {
		return nil
	}
	tbl := &ast.TableLit{Node: ast.Node{Pos: open.Pos}}
	for {
		if p.at(token.RBrace) {
			closing := p.next()
			tbl.End = closing.End
			return tbl
		}
		k := p.parseTableKey()
		if k == nil {
			return nil
		}
		if _, ok := p.expect(token.FatArrow, "'=>' after table key"); !ok {
			return nil
		}
		v := p.parseExpr()
		if v == nil {
			return nil
		}
		tbl.Entries = append(tbl.Entries, ast.TableEntry{
			Node: ast.Node{Pos: startOf(k), End: endOf(v)}, Key: k, Value: v})
		if !p.accept(token.Comma) {
			break
		}
	}
	closing, ok := p.expect(token.RBrace, "'}' after table entries")
	if !ok {
		return nil
	}
	tbl.End = closing.End
	return tbl
}

// parseTableKey accepts reserved words as shorthand string keys. This keeps
// standard-library tables such as `%{ match => __path_match }` usable while
// preserving keyword syntax everywhere else.
func (p *Parser) parseTableKey() ast.Expr {
	if name, ok := tableKeyKeyword[p.cur().Kind]; ok && p.peek(1).Kind == token.FatArrow {
		t := p.next()
		return &ast.Ident{Node: ast.Node{Pos: t.Pos, End: t.End}, Name: name}
	}
	return p.parseExpr()
}

var tableKeyKeyword = map[token.Kind]string{
	token.KwLet: "let", token.KwFn: "fn", token.KwIf: "if", token.KwElse: "else",
	token.KwWhile: "while", token.KwFor: "for", token.KwIn: "in", token.KwMatch: "match",
	token.KwReturn: "return", token.KwBreak: "break", token.KwContinue: "continue",
	token.KwEnum: "enum", token.KwAnd: "and", token.KwOr: "or", token.KwNot: "not",
	token.KwTrue: "true", token.KwFalse: "false", token.KwModule: "module",
	token.KwProgram: "program", token.KwOn: "on", token.KwConst: "const",
	token.KwImport: "import", token.KwForm: "form", token.KwAs: "as",
}

func (p *Parser) parseTaggedList(tag token.Token) ast.Expr {
	open, ok := p.expect(token.LBracket, "'[' after tag")
	if !ok {
		return nil
	}
	tl := &ast.TaggedLit{Node: ast.Node{Pos: tag.Pos, End: open.End}, Tag: tag.Text}
	for {
		if p.at(token.RBracket) {
			closing := p.next()
			tl.End = closing.End
			return tl
		}
		e := p.parseExpr()
		if e == nil {
			return nil
		}
		tl.Elems = append(tl.Elems, e)
		if !p.accept(token.Comma) {
			break
		}
	}
	closing, ok := p.expect(token.RBracket, "']' after tagged-list elements")
	if !ok {
		return nil
	}
	tl.End = closing.End
	return tl
}

func (p *Parser) parseFn() ast.Expr {
	kw, ok := p.expect(token.KwFn, "'fn'")
	if !ok {
		return nil
	}
	// Named FnDecl binds Name in the enclosing scope; anonymous FnLit
	// follows directly.
	if p.at(token.Ident) && p.peek(1).Kind == token.LParen {
		id := p.next()
		fn, ok := p.parseFnRest(kw.Pos)
		if !ok {
			return nil
		}
		return &ast.FnDecl{Node: ast.Node{Pos: kw.Pos, End: fn.End}, Name: id.Text, Fn: fn}
	}
	fn, ok := p.parseFnRest(kw.Pos)
	if !ok {
		return nil
	}
	return fn
}

func (p *Parser) parseFnRest(start token.Pos) (*ast.FnLit, bool) {
	if _, ok := p.expect(token.LParen, "'(' after fn"); !ok {
		return nil, false
	}
	params, ok := p.parseParams(token.RParen)
	if !ok {
		return nil, false
	}
	if _, ok := p.expect(token.RParen, "')' after parameters"); !ok {
		return nil, false
	}
	body := p.parseBlock()
	if body == nil {
		return nil, false
	}
	return &ast.FnLit{Node: ast.Node{Pos: start, End: body.End}, Params: params, Body: body}, true
}

func (p *Parser) parseEnum() ast.Expr {
	kw, ok := p.expect(token.KwEnum, "'enum'")
	if !ok {
		return nil
	}
	if _, ok := p.expect(token.LBrace, "'{' after enum"); !ok {
		return nil
	}
	en := &ast.EnumLit{Node: ast.Node{Pos: kw.Pos}}
	for {
		p.skipTerminators()
		if p.at(token.RBrace) {
			closing := p.next()
			en.End = closing.End
			return en
		}
		if p.at(token.EOF) {
			p.errorf("unexpected end of file in enum block")
			return nil
		}
		id, ok := p.expect(token.Ident, "enum member name")
		if !ok {
			return nil
		}
		if _, ok := p.expect(token.Assign, "'=' after enum member"); !ok {
			return nil
		}
		v := p.parseExpr()
		if v == nil {
			return nil
		}
		en.Members = append(en.Members, ast.EnumMember{
			Node: ast.Node{Pos: id.Pos, End: endOf(v)}, Name: id.Text, Value: v})
		p.accept(token.Comma)
	}
}

// ------------------------------------------------------------------ markup

func (p *Parser) parseNodeLit() ast.Expr {
	open, ok := p.expect(token.NodeOpen, "'%<'")
	if !ok {
		return nil
	}
	nl := &ast.NodeLit{Node: ast.Node{Pos: open.Pos}}
	body, ok := p.parseMarkupBody(true)
	if !ok {
		return nil
	}
	closing, ok := p.expect(token.NodeClose, "the node-literal fence end")
	if !ok {
		return nil
	}
	nl.Body = body
	nl.End = closing.End
	return nl
}

// parseMarkupBody parses markup parts until the matching close: MCloseName
// pops one entry from the element stack (managed here for name matching),
// and NodeClose ends the fence.
func (p *Parser) parseMarkupBody(fence bool) ([]ast.NodePart, bool) {
	var parts []ast.NodePart
	for {
		switch p.cur().Kind {
		case token.NodeClose:
			if fence {
				return parts, true
			}
			p.errorf("element not closed before the node-literal fence end")
			return nil, false
		case token.EOF:
			p.errorf("unexpected end of file inside node literal")
			return nil, false
		case token.MOpenName:
			el, ok := p.parseMarkupElement()
			if !ok {
				return nil, false
			}
			parts = append(parts, el)
		case token.MText:
			t := p.next()
			parts = append(parts, &ast.NodeText{Node: ast.Node{Pos: t.Pos, End: t.End}, Text: t.Text})
		case token.MComment:
			t := p.next()
			parts = append(parts, &ast.NodeComment{Node: ast.Node{Pos: t.Pos, End: t.End}, Text: t.Text})
		case token.MSpliceOpen:
			t := p.next()
			x := p.parseExpr()
			if x == nil {
				return nil, false
			}
			if _, ok := p.expect(token.MSpliceClose, "'}' closing the splice"); !ok {
				return nil, false
			}
			parts = append(parts, &ast.NodeSplice{Node: ast.Node{Pos: t.Pos, End: endOf(x)}, X: x})
		case token.MSpreadOpen:
			t := p.next()
			x := p.parseExpr()
			if x == nil {
				return nil, false
			}
			if _, ok := p.expect(token.MSpliceClose, "'}' closing the spread"); !ok {
				return nil, false
			}
			parts = append(parts, &ast.NodeSpread{Node: ast.Node{Pos: t.Pos, End: endOf(x)}, X: x})
		case token.MCloseName:
			if fence {
				p.errorf("close tag </%s> with no open element", p.cur().Text)
				return nil, false
			}
			return parts, true
		case token.MGT, token.MSelfClose, token.MAttrName, token.MEq, token.MString:
			p.errorf("unexpected %s in markup body", p.cur().Kind)
			p.next()
		default:
			p.errorf("unexpected %s in markup body", p.cur().Kind)
			return nil, false
		}
	}
}

func (p *Parser) parseMarkupElement() (*ast.NodeElement, bool) {
	open := p.next() // MOpenName
	el := &ast.NodeElement{Node: ast.Node{Pos: open.Pos}, Name: open.Text}
	// Attributes.
	for {
		switch p.cur().Kind {
		case token.MAttrName:
			at := p.next()
			attr := ast.NodeAttr{Node: ast.Node{Pos: at.Pos, End: at.End}, Name: at.Text}
			if p.accept(token.MEq) {
				if p.at(token.MString) {
					s := p.next()
					attr.Literal = s.Text
					attr.Node.End = s.End
				} else if p.at(token.MSpliceOpen) {
					p.next()
					v := p.parseExpr()
					if v == nil {
						return nil, false
					}
					if _, ok := p.expect(token.MSpliceClose, "'}' closing the attribute splice"); !ok {
						return nil, false
					}
					attr.Value = v
					attr.Node.End = endOf(v)
				} else {
					p.errorf("expected attribute value after '='")
					return nil, false
				}
			}
			el.Attrs = append(el.Attrs, attr)
		case token.MGT:
			p.next()
			el.Node.End = p.toks[p.pos-1].End
			p.elemStack = append(p.elemStack, el.Name)
			body, ok := p.parseMarkupBody(false)
			if !ok {
				return nil, false
			}
			cl, ok := p.expect(token.MCloseName, "closing tag")
			if !ok {
				return nil, false
			}
			if cl.Text != el.Name {
				p.diags.Errorf(diag.Span{Start: cl.Pos, End: cl.End},
					"mismatched close tag </%s> for <%s>", cl.Text, el.Name)
			}
			if n := len(p.elemStack); n > 0 {
				p.elemStack = p.elemStack[:n-1]
			}
			el.Body = body
			el.Node.End = cl.End
			return el, true
		case token.MSelfClose:
			t := p.next()
			el.SelfClose = true
			el.Node.End = t.End
			return el, true
		default:
			p.errorf("expected attribute, '>' or '/>' in element tag, found %s", p.cur().Kind)
			return nil, false
		}
	}
}

// ----------------------------------------------------------------- patterns

func (p *Parser) parsePattern() ast.Pattern {
	t := p.cur()
	switch t.Kind {
	case token.Ident:
		if t.Text == "_" {
			p.next()
			return &ast.PatWildcard{Node: ast.Node{Pos: t.Pos, End: t.End}}
		}
		p.next()
		return &ast.PatBind{Node: ast.Node{Pos: t.Pos, End: t.End}, Name: t.Text}
	case token.Int, token.Float, token.String, token.Char, token.KwTrue, token.KwFalse, token.KwNull:
		var lit ast.Expr
		switch t.Kind {
		case token.String:
			// Patterns match literally; no interpolation here.
			p.next()
			lit = &ast.StringLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Value: cookStringNoInterp(t.Text), Raw: t.Text}
		case token.Char:
			p.next()
			lit = &ast.CharLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Value: t.Text}
		case token.KwNull:
			p.next()
			lit = &ast.NullLit{Node: ast.Node{Pos: t.Pos, End: t.End}}
		default:
			lit = p.parsePrimary()
			if lit == nil {
				return nil
			}
		}
		return &ast.PatLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Lit: lit}
	case token.LParen:
		p.next()
		if _, ok := p.expect(token.RParen, "')' (unit pattern)"); !ok {
			return nil
		}
		return &ast.PatLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Lit: &ast.UnitLit{Node: ast.Node{Pos: t.Pos, End: t.End}}}
	case token.Tag:
		p.next()
		pt := &ast.PatTagged{Node: ast.Node{Pos: t.Pos, End: t.End}, Tag: t.Text}
		if p.at(token.LBracket) {
			p.next()
			pt.Brackets = true
			for {
				if p.at(token.RBracket) {
					closing := p.next()
					pt.Node.End = closing.End
					return pt
				}
				if p.at(token.DotDot) {
					p.errorf("'..' rest binding is not valid inside a tagged-list pattern")
					return nil
				}
				sub := p.parsePattern()
				if sub == nil {
					return nil
				}
				pt.Elems = append(pt.Elems, sub)
				if !p.accept(token.Comma) {
					break
				}
			}
			closing, ok := p.expect(token.RBracket, "']' after tagged-list pattern")
			if !ok {
				return nil
			}
			pt.Node.End = closing.End
		}
		return pt
	case token.Question:
		p.next()
		if p.patternCanStart(p.cur().Kind) {
			sub := p.parsePattern()
			if sub == nil {
				return nil
			}
			return &ast.PatSome{Node: ast.Node{Pos: t.Pos, End: endOf(sub)}, Pat: sub}
		}
		return &ast.PatOptAny{Node: ast.Node{Pos: t.Pos, End: t.End}}
	case token.LBracket:
		p.next()
		pl := &ast.PatList{Node: ast.Node{Pos: t.Pos}}
		for {
			if p.at(token.RBracket) {
				closing := p.next()
				pl.Node.End = closing.End
				return pl
			}
			if p.at(token.DotDot) {
				p.next()
				id, ok := p.expect(token.Ident, "rest binding name after '..'")
				if !ok {
					return nil
				}
				if _, ok := p.expect(token.RBracket, "']' after rest binding"); !ok {
					return nil
				}
				name := id.Text
				pl.Rest = &name
				pl.Node.End = p.toks[p.pos-1].End
				return pl
			}
			sub := p.parsePattern()
			if sub == nil {
				return nil
			}
			pl.Elems = append(pl.Elems, sub)
			if !p.accept(token.Comma) {
				break
			}
		}
		closing, ok := p.expect(token.RBracket, "']' after list pattern")
		if !ok {
			return nil
		}
		pl.Node.End = closing.End
		return pl
	}
	p.errorf("expected pattern, found %s", t.Kind)
	return nil
}

func (p *Parser) patternCanStart(k token.Kind) bool {
	switch k {
	case token.Ident, token.Int, token.Float, token.String, token.Char,
		token.KwTrue, token.KwFalse, token.KwNull, token.Tag, token.LParen,
		token.LBracket, token.Question:
		return true
	}
	return false
}

// Dump renders a program as S-expression-ish text; used by golden tests
// and the ds-format tool.
func Dump(n any) string {
	var sb strings.Builder
	dumpNode(&sb, n)
	return sb.String()
}

// ------------------------------------------------------------- AST dump

func dumpNode(sb *strings.Builder, n any) {
	switch x := n.(type) {
	case *ast.Program:
		for _, st := range x.Stmts {
			dumpNode(sb, st)
		}
	case *ast.Let:
		sb.WriteString("(let " + x.Name + " ")
		dumpNode(sb, x.Value)
		sb.WriteString(")")
	case *ast.Module:
		sb.WriteString("(module " + x.Name + ")")
	case *ast.ProgramDecl:
		sb.WriteString("(program " + x.Name + ")")
	case *ast.Import:
		sb.WriteString("(import " + x.Module)
		if len(x.Symbols) > 0 {
			sb.WriteString(" : " + x.Symbols[0])
			for _, s := range x.Symbols[1:] {
				sb.WriteString(", " + s)
			}
		} else if x.Alias != "" {
			sb.WriteString(" as " + x.Alias)
		}
		if x.Library != "" {
			sb.WriteString(" form " + x.Library)
		}
		sb.WriteString(")")
	case *ast.Dialect:
		sb.WriteString("(dialect " + strconv.Quote(x.Name) + ")")
	case *ast.Extend:
		sb.WriteString("(extend using " + x.Using + " {" + x.Body + "})")
	case *ast.Assign:
		sb.WriteString("(assign ")
		dumpNode(sb, x.Target)
		sb.WriteString(" ")
		dumpNode(sb, x.Value)
		sb.WriteString(")")
	case *ast.ExprStmt:
		dumpNode(sb, x.X)
	case *ast.Effect:
		sb.WriteString("(effect ")
		dumpNode(sb, x.Operand)
		sb.WriteString(" ")
		dumpNode(sb, x.Body)
		sb.WriteString(")")
	case *ast.If:
		sb.WriteString("(if ")
		dumpNode(sb, x.Cond)
		sb.WriteString(" ")
		dumpNode(sb, x.Then)
		if x.Else != nil {
			sb.WriteString(" else ")
			dumpNode(sb, x.Else)
		}
		sb.WriteString(")")
	case *ast.While:
		sb.WriteString("(while ")
		dumpNode(sb, x.Cond)
		sb.WriteString(" ")
		dumpNode(sb, x.Body)
		sb.WriteString(")")
	case *ast.For:
		sb.WriteString("(for " + x.Var + " in ")
		dumpNode(sb, x.Iter)
		sb.WriteString(" ")
		dumpNode(sb, x.Body)
		sb.WriteString(")")
	case *ast.Match:
		sb.WriteString("(match ")
		dumpNode(sb, x.Scrutinee)
		for _, arm := range x.Arms {
			sb.WriteString(" (")
			dumpNode(sb, arm.Pat)
			sb.WriteString(" => ")
			dumpNode(sb, arm.Body)
			sb.WriteString(")")
		}
		sb.WriteString(")")
	case *ast.On:
		sb.WriteString("(on ")
		dumpNode(sb, x.Target)
		sb.WriteString(" " + x.Event + " ")
		dumpNode(sb, x.Body)
		sb.WriteString(")")
	case *ast.Return:
		if x.Value != nil {
			sb.WriteString("(return ")
			dumpNode(sb, x.Value)
			sb.WriteString(")")
		} else {
			sb.WriteString("(return)")
		}
	case *ast.Break:
		sb.WriteString("(break)")
	case *ast.Continue:
		sb.WriteString("(continue)")
	case *ast.BlockStmt:
		dumpNode(sb, x.Block)
	case *ast.Block:
		sb.WriteString("{")
		for _, st := range x.Stmts {
			sb.WriteString(" ")
			dumpNode(sb, st)
		}
		sb.WriteString(" }")
	case *ast.Ident:
		sb.WriteString(x.Name)
	case *ast.IntLit:
		sb.WriteString(strconv.FormatInt(x.Value, 10))
	case *ast.FloatLit:
		sb.WriteString(strconv.FormatFloat(x.Value, 'g', -1, 64))
	case *ast.StringLit:
		sb.WriteString(strconv.Quote(x.Value))
	case *ast.InterpLit:
		sb.WriteString("(interp")
		for _, e := range x.Parts {
			sb.WriteString(" ")
			dumpNode(sb, e)
		}
		sb.WriteString(")")
	case *ast.CharLit:
		sb.WriteString("#\\" + charDumpName(x.Value))
	case *ast.RegexLit:
		sb.WriteString("/" + x.Pattern + "/")
	case *ast.NullLit:
		sb.WriteString("null")
	case *ast.BoolLit:
		sb.WriteString(strconv.FormatBool(x.Value))
	case *ast.UnitLit:
		sb.WriteString("()")
	case *ast.TagLit:
		sb.WriteString(":" + x.Name)
	case *ast.SelectorLit:
		sb.WriteString("$" + strconv.Quote(x.Src))
	case *ast.ListLit:
		sb.WriteString("[")
		for i, e := range x.Elems {
			if i > 0 {
				sb.WriteString(" ")
			}
			dumpNode(sb, e)
		}
		sb.WriteString("]")
	case *ast.TableLit:
		sb.WriteString("%{")
		for _, e := range x.Entries {
			sb.WriteString(" (")
			dumpNode(sb, e.Key)
			sb.WriteString(" => ")
			dumpNode(sb, e.Value)
			sb.WriteString(")")
		}
		sb.WriteString("}")
	case *ast.TaggedLit:
		sb.WriteString(":" + x.Tag + "[")
		for i, e := range x.Elems {
			if i > 0 {
				sb.WriteString(" ")
			}
			dumpNode(sb, e)
		}
		sb.WriteString("]")
	case *ast.NodeLit:
		sb.WriteString("%<")
		for _, part := range x.Body {
			dumpNode(sb, part)
		}
		sb.WriteString("%>")
	case *ast.NodeElement:
		sb.WriteString("<" + x.Name)
		for _, a := range x.Attrs {
			sb.WriteString(" " + a.Name + "=")
			if a.Value != nil {
				sb.WriteString("{")
				dumpNode(sb, a.Value)
				sb.WriteString("}")
			} else {
				sb.WriteString(strconv.Quote(a.Literal))
			}
		}
		if x.SelfClose {
			sb.WriteString("/>")
			return
		}
		sb.WriteString(">")
		for _, part := range x.Body {
			dumpNode(sb, part)
		}
		sb.WriteString("</" + x.Name + ">")
	case *ast.NodeText:
		sb.WriteString(strconv.Quote(x.Text))
	case *ast.NodeComment:
		sb.WriteString("<!--" + x.Text + "-->")
	case *ast.NodeSplice:
		sb.WriteString("{")
		dumpNode(sb, x.X)
		sb.WriteString("}")
	case *ast.NodeSpread:
		sb.WriteString("{..")
		dumpNode(sb, x.X)
		sb.WriteString("}")
	case *ast.FnDecl:
		sb.WriteString("(fn " + x.Name + " ")
		dumpNode(sb, x.Fn)
		sb.WriteString(")")
	case *ast.FnLit:
		sb.WriteString("(fn (")
		for i, prm := range x.Params {
			if i > 0 {
				sb.WriteString(" ")
			}
			if prm.Rest {
				sb.WriteString(prm.Name + "..")
			} else {
				sb.WriteString(prm.Name)
			}
		}
		sb.WriteString(") ")
		dumpNode(sb, x.Body)
		sb.WriteString(")")
	case *ast.EnumLit:
		sb.WriteString("(enum")
		for _, m := range x.Members {
			sb.WriteString(" (" + m.Name + " ")
			dumpNode(sb, m.Value)
			sb.WriteString(")")
		}
		sb.WriteString(")")
	case *ast.Binary:
		sb.WriteString("(" + x.Op.String() + " ")
		dumpNode(sb, x.L)
		sb.WriteString(" ")
		dumpNode(sb, x.R)
		sb.WriteString(")")
	case *ast.Unary:
		var op string
		switch x.Op {
		case ast.OpNot:
			op = "not"
		case ast.OpNeg:
			op = "-"
		case ast.OpSome:
			op = "?"
		case ast.OpEff:
			op = "~"
		}
		sb.WriteString("(" + op + " ")
		dumpNode(sb, x.X)
		sb.WriteString(")")
	case *ast.OptNone:
		sb.WriteString("(?)")
	case *ast.EffectResult:
		sb.WriteString("(~)")
	case *ast.Call:
		sb.WriteString("(")
		dumpNode(sb, x.Fn)
		for _, a := range x.Args {
			sb.WriteString(" ")
			dumpNode(sb, a)
		}
		sb.WriteString(")")
	case *ast.Index:
		sb.WriteString("(index ")
		dumpNode(sb, x.X)
		sb.WriteString(" ")
		dumpNode(sb, x.I)
		sb.WriteString(")")
	case *ast.Field:
		sb.WriteString("(field ")
		dumpNode(sb, x.X)
		sb.WriteString(" " + x.Name + ")")
	case *ast.PatWildcard:
		sb.WriteString("_")
	case *ast.PatLit:
		dumpNode(sb, x.Lit)
	case *ast.PatBind:
		sb.WriteString(x.Name)
	case *ast.PatList:
		sb.WriteString("[")
		for i, e := range x.Elems {
			if i > 0 {
				sb.WriteString(" ")
			}
			dumpNode(sb, e)
		}
		if x.Rest != nil {
			sb.WriteString(" .." + *x.Rest)
		}
		sb.WriteString("]")
	case *ast.PatTagged:
		sb.WriteString(":" + x.Tag)
		if x.Brackets {
			sb.WriteString("[")
			for i, e := range x.Elems {
				if i > 0 {
					sb.WriteString(" ")
				}
				dumpNode(sb, e)
			}
			sb.WriteString("]")
		}
	case *ast.PatOptAny:
		sb.WriteString("(?)")
	case *ast.PatSome:
		sb.WriteString("(? ")
		dumpNode(sb, x.Pat)
		sb.WriteString(")")
	case *ast.GrammarDecl:
		sb.WriteString("(grammar " + x.Name)
		for _, k := range x.Keywords {
			sb.WriteString(" (keyword " + k.Name + " " + strconv.Quote(k.Literal) + ")")
		}
		for _, l := range x.Lexemes {
			sb.WriteString(" (lexeme " + l.Name + " ")
			if l.Regex != nil {
				sb.WriteString("/" + *l.Regex + "/")
			} else if l.Literal != nil {
				sb.WriteString(strconv.Quote(*l.Literal))
			} else if l.Fn != nil {
				dumpNode(sb, l.Fn)
			}
			sb.WriteString(")")
		}
		for _, r := range x.Rules {
			sb.WriteString(" (rule " + r.Name + " ::= ")
			for ai, alt := range r.Alts {
				if ai > 0 {
					sb.WriteString(" | ")
				}
				for ei, e := range alt {
					if ei > 0 {
						sb.WriteString(" ")
					}
					dumpRuleElem(sb, e)
				}
			}
			sb.WriteString(")")
		}
		sb.WriteString(")")
	case *ast.AbsynDecl:
		sb.WriteString("(absyn " + x.Name)
		for _, ty := range x.Types {
			sb.WriteString(" (" + ty.Name + " ::=")
			for _, c := range ty.Ctors {
				sb.WriteString(" (" + c.Name)
				for _, f := range c.Fields {
					sb.WriteString(" (" + f.Type + " " + f.Name + ")")
				}
				sb.WriteString(")")
			}
			sb.WriteString(")")
		}
		sb.WriteString(")")
	default:
		sb.WriteString(fmt.Sprintf("(?%T)", n))
	}
}

func dumpRuleElem(sb *strings.Builder, e ast.RuleElem) {
	switch e.Kind {
	case ast.KindRuleRef:
		sb.WriteString(e.Name)
	case ast.KindRuleLit:
		sb.WriteString(strconv.Quote(e.Name))
	case ast.KindRuleChar:
		sb.WriteString("#\\" + charDumpName(e.Char))
	case ast.KindRuleRegex:
		sb.WriteString("/" + e.Name + "/")
	case ast.KindRuleGroup:
		sb.WriteString("(")
		for ai, alt := range e.Group {
			if ai > 0 {
				sb.WriteString(" | ")
			}
			for ei, se := range alt {
				if ei > 0 {
					sb.WriteString(" ")
				}
				dumpRuleElem(sb, se)
			}
		}
		sb.WriteString(")")
	}
	if e.Suffix != 0 {
		sb.WriteByte(e.Suffix)
	}
}

func charDumpName(s string) string {
	switch s {
	case "\n":
		return "newline"
	case " ":
		return "space"
	case "\t":
		return "tab"
	case "\r":
		return "return"
	case "\x07":
		return "bell"
	case "\x0c":
		return "feed"
	case "\x1b":
		return "escape"
	case "\x00":
		return "null"
	}
	return s
}
