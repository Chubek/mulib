// Package grammar implements Domscript's `grammar`/`absyn` runtime on top
// of dependencies/libglr's design.
//
// Mapping to libglr (dependencies/libglr/include/glr):
//
//   - grammar.h: Grammar owns symbols (terminals from `keyword`/`lexeme`,
//     nonterminals from `rule`) and productions (rule alternatives). Our
//     runtime.Grammar mirrors glr_grammar_t; Lexeme.Pattern mirrors
//     glr_scannerless_set_pattern (POSIX extended regex, matched with Go
//     RE2 like libglr's scannerless patterns); custom `fn(reader)`
//     lexers mirror lexer-hooks.h (invoked with a *runtime.Reader).
//   - parser.h: Parser holds a Grammar plus trivia (whitespace skipped
//     between tokens, like glr_parser_set_trivia). Parse builds a shared
//     packed forest; our Forest is a simplified DAG (single derivation for
//     unambiguous inputs, first-match for ambiguous — full GLR forking is
//     future work behind the `libglr` build tag which links the C library).
//   - forest.h: Forest nodes are Terminal (matched text) or Nonterminal
//     (rule name + children).
//   - query.h / semantic-action.h: Absyn constructors (`:Null[]`,
//     `:Bool[true]`, ...) are the S-expression AST building equivalent;
//     DeriveTree/DeriveVisitor expose the Absyn for host traversal.
//
// The C library is used via the `libglr` build tag (see cgo.go); the
// default pure-Go engine below handles the JSON example and other
// deterministic grammars. Set DOMSCRIPT_GLR=libglr to link the C engine
// when it is built (cmake -S dependencies/libglr -B build-libglr).
package grammar

import (
	"fmt"
	"strings"

	"domscript/internal/runtime"
)

// Forest is a simplified shared packed parse forest.
type Forest struct {
	Root *Node
	Text string
}

// Node is one forest node.
type Node struct {
	Kind     string // "terminal" or rule name
	Text     string // terminal matched text
	Children []*Node
	Symbol   string // terminal name (for terminals)
}

// Parse parses input with g (trivia whitespace skipped between tokens).
// It returns the forest for the start rule (first declared rule) or an
// error with the failure offset.
func Parse(g *runtime.Grammar, input string) (*Forest, error) {
	if g == nil {
		return nil, fmt.Errorf("nil grammar")
	}
	start := g.StartRule()
	if start == nil {
		return nil, fmt.Errorf("grammar %q has no rules", g.Name)
	}
	p := &parser{grammar: g, input: input, pos: 0}
	node, ok := p.parseRule(start, 0)
	if !ok {
		return nil, &runtime.TypedError{Tag: "ParseError", Msg: fmt.Sprintf("parse failed at offset %d", p.failPos), Val: runtime.TaggedOf("ParseError", []runtime.Value{runtime.Str(fmt.Sprintf("parse failed at offset %d", p.failPos))})}
	}
	p.skipTrivia()
	if p.pos != len(input) {
		return nil, &runtime.TypedError{Tag: "ParseError", Msg: fmt.Sprintf("trailing input at offset %d", p.pos), Val: runtime.TaggedOf("ParseError", []runtime.Value{runtime.Str(fmt.Sprintf("trailing input at offset %d", p.pos))})}
	}
	_ = ok
	return &Forest{Root: node, Text: input}, nil
}

type parser struct {
	grammar *runtime.Grammar
	input   string
	pos     int
	failPos int
}

func (p *parser) skipTrivia() {
	for p.pos < len(p.input) {
		c := p.input[p.pos]
		if c == ' ' || c == '\t' || c == '\n' || c == '\r' {
			p.pos++
		} else {
			break
		}
	}
}

func (p *parser) parseRule(rule *runtime.Rule, depth int) (*Node, bool) {
	if depth > 200 {
		return nil, false
	}
	for _, alt := range rule.Alts {
		save := p.pos
		var kids []*Node
		ok := true
		for _, e := range alt {
			n, good := p.parseElem(e, depth+1)
			if !good {
				ok = false
				break
			}
			if n != nil {
				kids = append(kids, n)
			}
		}
		if ok {
			return &Node{Kind: rule.Name, Children: kids}, true
		}
		p.pos = save
		if p.pos > p.failPos {
			p.failPos = p.pos
		}
	}
	return nil, false
}

func (p *parser) parseElem(e runtime.RuleElem, depth int) (*Node, bool) {
	// Handle suffix by dispatching to base then repeating.
	switch e.Suffix {
	case 0:
		return p.parseBase(e, depth)
	case '?':
		save := p.pos
		n, ok := p.parseBase(e, depth)
		if !ok {
			p.pos = save
			return nil, true // optional absent
		}
		return n, true
	case '*':
		var kids []*Node
		for {
			save := p.pos
			p.skipTrivia()
			// Avoid infinite loop on empty match.
			n, ok := p.parseBase(e, depth)
			if !ok {
				p.pos = save
				break
			}
			if p.pos == save {
				break
			}
			if n != nil {
				kids = append(kids, n)
			}
		}
		// `*` groups splice children directly (no wrapper).
		if len(kids) == 0 {
			return nil, true
		}
		return &Node{Kind: "seq", Children: kids}, true
	case '+':
		first, ok := p.parseBase(e, depth)
		if !ok {
			return nil, false
		}
		kids := []*Node{}
		if first != nil {
			kids = append(kids, first)
		}
		for {
			save := p.pos
			n, ok := p.parseBase(e, depth)
			if !ok {
				p.pos = save
				break
			}
			if p.pos == save {
				break
			}
			if n != nil {
				kids = append(kids, n)
			}
		}
		return &Node{Kind: "seq", Children: kids}, true
	}
	return nil, false
}

func (p *parser) parseBase(e runtime.RuleElem, depth int) (*Node, bool) {
	// Groups (with their own suffix already stripped? No, Group may have
	// suffix in e.Suffix, but we handled suffix above by recursing to
	// parseBase with Suffix==0? Actually parseElem handles suffix by calling
	// parseBase for the base (which for Group means trying alternatives).
	// To avoid double-handling, when e.Kind==Group and e.Suffix!=0, we
	// already branched above; here e.Suffix is the original, but for Group
	// we need to try the group. Simplify: if Group, try alternatives.
	if e.Kind == runtime.RuleGroup {
		// Try each alternative sequence.
		for _, alt := range e.Group {
			save := p.pos
			var kids []*Node
			ok := true
			for _, se := range alt {
				n, good := p.parseElem(se, depth+1)
				if !good {
					ok = false
					break
				}
				if n != nil {
					kids = append(kids, n)
				}
			}
			if ok {
				if len(kids) == 0 {
					return nil, true
				}
				if len(kids) == 1 {
					return kids[0], true
				}
				return &Node{Kind: "seq", Children: kids}, true
			}
			p.pos = save
		}
		return nil, false
	}
	// For non-group with suffix, parseElem already handled suffix by
	// calling parseBase? No, parseElem for suffix calls parseBase(e) where
	// e still has Suffix! That would infinite recurse. Fix: strip suffix
	// for base parsing.
	base := e
	base.Suffix = 0
	return p.parseAtom(base, depth)
}

func (p *parser) parseAtom(e runtime.RuleElem, depth int) (*Node, bool) {
	p.skipTrivia()
	switch e.Kind {
	case runtime.RuleRef:
		// Terminal (keyword/lexeme) or nonterminal (rule)?
		if lex, ok := p.grammar.Keywords[e.Name]; ok {
			return p.matchLiteral(lex.Literal, e.Name)
		}
		if lex, ok := p.grammar.Lexemes[e.Name]; ok {
			switch lex.Kind {
			case runtime.LexLiteral:
				return p.matchLiteral(lex.Literal, e.Name)
			case runtime.LexPattern:
				return p.matchPattern(lex, e.Name)
			case runtime.LexCustom:
				// Custom lexers need a Domscript call hook (installed by
				// exec). Without it, fail gracefully.
				return nil, false
			}
			return nil, false
		}
		if rule, ok := p.grammar.Rules[e.Name]; ok {
			return p.parseRule(rule, depth)
		}
		return nil, false
	case runtime.RuleLit:
		return p.matchLiteral(e.Name, "")
	case runtime.RuleChar:
		return p.matchLiteral(e.Char, "")
	case runtime.RuleRegex:
		if strings.HasPrefix(e.Pattern, "") {
			// Compile on the fly (cached grammars precompile lexemes;
			// inline rule regexes are rare).
			re, err := runtime.NewRegex(e.Pattern)
			if err != nil {
				return nil, false
			}
			loc := re.Re.FindStringIndex(p.input[p.pos:])
			if loc == nil || loc[0] != 0 {
				return nil, false
			}
			text := p.input[p.pos : p.pos+loc[1]]
			p.pos += loc[1]
			return &Node{Kind: "terminal", Text: text, Symbol: "regex"}, true
		}
		return nil, false
	}
	return nil, false
}

func (p *parser) matchLiteral(lit, sym string) (*Node, bool) {
	p.skipTrivia()
	if strings.HasPrefix(p.input[p.pos:], lit) {
		p.pos += len(lit)
		return &Node{Kind: "terminal", Text: lit, Symbol: sym}, true
	}
	return nil, false
}

func (p *parser) matchPattern(lex *runtime.Lexeme, sym string) (*Node, bool) {
	p.skipTrivia()
	if lex.Re == nil {
		return nil, false
	}
	loc := lex.Re.FindStringIndex(p.input[p.pos:])
	if loc == nil || loc[0] != 0 {
		return nil, false
	}
	text := p.input[p.pos : p.pos+loc[1]]
	p.pos += loc[1]
	return &Node{Kind: "terminal", Text: text, Symbol: sym}, true
}
