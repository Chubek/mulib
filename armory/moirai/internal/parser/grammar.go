// Package parser: grammar.go parses `grammar` and `absyn`/`asdl` blocks.
package parser

import (
	"domscript/internal/ast"
	"domscript/internal/token"
)

func (p *Parser) parseGrammarDecl() ast.Stmt {
	kw := p.next() // 'grammar'
	name, _, ok := p.expectIdent("grammar name after 'grammar'")
	if !ok {
		return nil
	}
	if _, ok := p.expect(token.LBrace, "'{' after grammar name"); !ok {
		return nil
	}
	decl := &ast.GrammarDecl{Node: ast.Node{Pos: kw.Pos}, Name: name}
	for {
		p.skipTerminators()
		if p.at(token.RBrace) {
			closing := p.next()
			decl.End = closing.End
			return decl
		}
		if p.at(token.EOF) {
			p.errorf("unexpected end of file in grammar block")
			return nil
		}
		if !p.at(token.Ident) {
			p.errorf("expected keyword, lexeme, or rule in grammar block, found %s", p.cur().Kind)
			return nil
		}
		kind := p.next()
		switch kind.Text {
		case "keyword":
			k, ok := p.parseGrammarKeyword(kind)
			if !ok {
				return nil
			}
			decl.Keywords = append(decl.Keywords, *k)
		case "lexeme":
			l, ok := p.parseGrammarLexeme(kind)
			if !ok {
				return nil
			}
			decl.Lexemes = append(decl.Lexemes, *l)
		case "rule":
			r, ok := p.parseGrammarRule(kind)
			if !ok {
				return nil
			}
			decl.Rules = append(decl.Rules, *r)
		default:
			p.errorf("expected keyword, lexeme, or rule in grammar block, found %q", kind.Text)
			return nil
		}
	}
}

func (p *Parser) expectIdent(what string) (string, token.Pos, bool) {
	t, ok := p.expect(token.Ident, what)
	if !ok {
		return "", token.Pos{}, false
	}
	return t.Text, t.End, true
}

func (p *Parser) parseGrammarKeyword(kind token.Token) (*ast.GrammarKeyword, bool) {
	nameTok, ok := p.expect(token.Ident, "terminal name after 'keyword'")
	if !ok {
		return nil, false
	}
	if _, ok := p.expect(token.Define, "'::=' after keyword name"); !ok {
		return nil, false
	}
	strTok, ok := p.expect(token.String, "literal after '::='")
	if !ok {
		return nil, false
	}
	lit := cookStringNoInterp(strTok.Text)
	k := &ast.GrammarKeyword{Node: ast.Node{Pos: kind.Pos, End: strTok.End}, Name: nameTok.Text, Literal: lit}
	if !p.accept(token.Semi) {
		if !p.expectTerminator() {
			return nil, false
		}
	} else {
		p.skipTerminators()
	}
	k.End = p.toks[p.pos-1].End
	// Re-anchor end to the string when no terminator was consumed yet.
	if k.End.Off < strTok.End.Off {
		k.End = strTok.End
	}
	return k, true
}

func (p *Parser) parseGrammarLexeme(kind token.Token) (*ast.GrammarLexeme, bool) {
	nameTok, ok := p.expect(token.Ident, "terminal name after 'lexeme'")
	if !ok {
		return nil, false
	}
	if _, ok := p.expect(token.Define, "'::=' after lexeme name"); !ok {
		return nil, false
	}
	l := &ast.GrammarLexeme{Node: ast.Node{Pos: kind.Pos}, Name: nameTok.Text}
	switch p.cur().Kind {
	case token.Regex:
		t := p.next()
		s := t.Text
		l.Regex = &s
		l.End = t.End
	case token.String:
		t := p.next()
		s := cookStringNoInterp(t.Text)
		l.Literal = &s
		l.End = t.End
	case token.KwFn:
		fn := p.parseFn()
		if fn == nil {
			return nil, false
		}
		l.Fn = fn
		l.End = endOf(fn)
	default:
		p.errorf("expected /regex/ or fn(reader) after '::=' in lexeme")
		return nil, false
	}
	if !p.accept(token.Semi) {
		if !p.expectTerminator() {
			return nil, false
		}
	} else {
		p.skipTerminators()
	}
	return l, true
}

func (p *Parser) parseGrammarRule(kind token.Token) (*ast.GrammarRule, bool) {
	nameTok, ok := p.expect(token.Ident, "rule name after 'rule'")
	if !ok {
		return nil, false
	}
	if _, ok := p.expect(token.Define, "'::=' after rule name"); !ok {
		return nil, false
	}
	r := &ast.GrammarRule{Node: ast.Node{Pos: kind.Pos}, Name: nameTok.Text}
	alts, ok := p.parseRuleAlts(true)
	if !ok {
		return nil, false
	}
	r.Alts = alts
	if !p.accept(token.Semi) {
		if !p.expectTerminator() {
			return nil, false
		}
	} else {
		p.skipTerminators()
	}
	if len(alts) > 0 {
		r.End = p.toks[p.pos-1].End
	} else {
		r.End = nameTok.End
	}
	return r, true
}

// parseRuleAlts parses `alt (| alt)*` until `;`, `)`, `]` or EOF.
// topLevel selects whether `;` terminates (rule level) vs `)` (group).
func (p *Parser) parseRuleAlts(topLevel bool) ([][]ast.RuleElem, bool) {
	var alts [][]ast.RuleElem
	cur := []ast.RuleElem{}
	flush := func() {
		alts = append(alts, cur)
		cur = []ast.RuleElem{}
	}
	skipNewlines := func() {
		for p.at(token.Newline) {
			p.next()
		}
	}
	for {
		skipNewlines()
		switch p.cur().Kind {
		case token.Pipe:
			p.next()
			flush()
			continue
		case token.Semi:
			if topLevel {
				flush()
				return alts, true
			}
			p.errorf("unexpected ';' inside rule group")
			return nil, false
		case token.RParen:
			if !topLevel {
				flush()
				return alts, true
			}
			p.errorf("unexpected ')' in rule")
			return nil, false
		case token.RBrace, token.EOF:
			if topLevel {
				// Missing ';' (recovered by caller terminator check).
				flush()
				return alts, true
			}
			p.errorf("unexpected end of rule group")
			return nil, false
		case token.Newline:
			p.next()
			continue
		}
		elem, ok := p.parseRuleElem()
		if !ok {
			return nil, false
		}
		cur = append(cur, *elem)
	}
}

func (p *Parser) parseRuleElem() (*ast.RuleElem, bool) {
	t := p.cur()
	var elem ast.RuleElem
	switch t.Kind {
	case token.Ident:
		p.next()
		elem = ast.RuleElem{Node: ast.Node{Pos: t.Pos, End: t.End}, Kind: ast.KindRuleRef, Name: t.Text}
	case token.String:
		p.next()
		elem = ast.RuleElem{Node: ast.Node{Pos: t.Pos, End: t.End}, Kind: ast.KindRuleLit, Name: cookStringNoInterp(t.Text)}
	case token.Char:
		p.next()
		elem = ast.RuleElem{Node: ast.Node{Pos: t.Pos, End: t.End}, Kind: ast.KindRuleChar, Char: t.Text}
	case token.Regex:
		p.next()
		elem = ast.RuleElem{Node: ast.Node{Pos: t.Pos, End: t.End}, Kind: ast.KindRuleRegex, Name: t.Text}
	case token.LParen:
		open := p.next()
		alts, ok := p.parseRuleAlts(false)
		if !ok {
			return nil, false
		}
		closing, ok := p.expect(token.RParen, "')' after rule group")
		if !ok {
			return nil, false
		}
		elem = ast.RuleElem{Node: ast.Node{Pos: open.Pos, End: closing.End}, Kind: ast.KindRuleGroup, Group: alts}
	default:
		p.errorf("expected rule element, found %s", t.Kind)
		return nil, false
	}
	// EBNF suffix.
	switch p.cur().Kind {
	case token.Question:
		q := p.next()
		elem.Suffix = '?'
		elem.End = q.End
	case token.Star:
		s := p.next()
		elem.Suffix = '*'
		elem.End = s.End
	case token.Plus:
		s := p.next()
		elem.Suffix = '+'
		elem.End = s.End
	}
	return &elem, true
}

func (p *Parser) parseAbsynDecl() ast.Stmt {
	kw := p.next() // 'absyn' or 'asdl'
	name, _, ok := p.expectIdent("type name after 'absyn'")
	if !ok {
		return nil
	}
	if _, ok := p.expect(token.LBrace, "'{' after absyn name"); !ok {
		return nil
	}
	decl := &ast.AbsynDecl{Node: ast.Node{Pos: kw.Pos}, Name: name}
	for {
		p.skipTerminators()
		if p.at(token.RBrace) {
			closing := p.next()
			decl.End = closing.End
			return decl
		}
		if p.at(token.EOF) {
			p.errorf("unexpected end of file in absyn block")
			return nil
		}
		ty, ok := p.parseAbsynType()
		if !ok {
			return nil
		}
		decl.Types = append(decl.Types, *ty)
	}
}

func (p *Parser) parseAbsynType() (*ast.AbsynType, bool) {
	nameTok, ok := p.expect(token.Ident, "type name in absyn block")
	if !ok {
		return nil, false
	}
	if _, ok := p.expect(token.Define, "'::=' after absyn type name"); !ok {
		return nil, false
	}
	ty := &ast.AbsynType{Node: ast.Node{Pos: nameTok.Pos}, Name: nameTok.Text}
	skipNewlines := func() {
		for p.at(token.Newline) {
			p.next()
		}
	}
	for {
		skipNewlines()
		if p.at(token.Semi) {
			semi := p.next()
			ty.End = semi.End
			p.skipTerminators()
			return ty, true
		}
		if p.at(token.EOF) || p.at(token.RBrace) {
			p.errorf("expected ';' after absyn constructors")
			return nil, false
		}
		// Leading or separating '|'.
		p.accept(token.Pipe)
		skipNewlines()
		if p.at(token.Semi) {
			continue
		}
		ctor, ok := p.parseAbsynCtor()
		if !ok {
			return nil, false
		}
		ty.Ctors = append(ty.Ctors, *ctor)
	}
}

func (p *Parser) parseAbsynCtor() (*ast.AbsynCtor, bool) {
	nameTok, ok := p.expect(token.Ident, "constructor name")
	if !ok {
		return nil, false
	}
	ctor := &ast.AbsynCtor{Node: ast.Node{Pos: nameTok.Pos, End: nameTok.End}, Name: nameTok.Text}
	if !p.at(token.LParen) {
		return ctor, true
	}
	p.next() // '('
	for {
		p.skipTerminators()
		if p.at(token.RParen) {
			closing := p.next()
			ctor.End = closing.End
			return ctor, true
		}
		if p.at(token.EOF) {
			p.errorf("unexpected end of file in constructor fields")
			return nil, false
		}
		// Field type: Ident with optional `*`/`?` suffix (e.g. `value*`).
		tyTok, ok := p.expect(token.Ident, "field type")
		if !ok {
			return nil, false
		}
		tyName := tyTok.Text
		if p.at(token.Star) {
			p.next()
			tyName += "*"
		} else if p.at(token.Question) {
			p.next()
			tyName += "?"
		}
		nameTok2, ok := p.expect(token.Ident, "field name")
		if !ok {
			return nil, false
		}
		ctor.Fields = append(ctor.Fields, ast.AbsynField{
			Node: ast.Node{Pos: tyTok.Pos, End: nameTok2.End},
			Type: tyName, Name: nameTok2.Text,
		})
		if p.accept(token.Comma) {
			continue
		}
	}
}
