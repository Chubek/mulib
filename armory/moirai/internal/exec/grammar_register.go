// Package exec: grammar_register.go builds libglr-backed runtime Grammars
// and Absyns from parsed declarations after a chunk runs.
package exec

import (
	"fmt"

	"domscript/internal/ast"
	"domscript/internal/ffi"
	"domscript/internal/grammar"
	"domscript/internal/runtime"
)

// ensureParseNative registers the `Parse(parser, input)` helper (backed by
// internal/grammar, i.e. dependencies/libglr's design) once per state.
func ensureParseNative(st *runtime.State) {
	if _, ok := st.LookupGlobal("Parse"); ok {
		return
	}
	ffi.Register(st, "Parse", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) != 2 {
			return runtime.Unit(), fmt.Errorf("Parse expects 2 args (parser, input), got %d", len(args))
		}
		if args[0].Kind() != runtime.KindParser {
			return runtime.Unit(), fmt.Errorf("Parse expects a :Parser, got %s", runtime.TypeName(args[0]))
		}
		var input string
		switch args[1].Kind() {
		case runtime.KindString:
			input = args[1].StrValue()
		case runtime.KindReader:
			input = args[1].Reader().Rest()
		default:
			return runtime.Unit(), fmt.Errorf("Parse input must be :String, got %s", runtime.TypeName(args[1]))
		}
		forest, err := grammar.Parse(args[0].Parser().Grammar, input)
		if err != nil {
			// Return :Error (value, not raise) so `~ Parse(...)` works.
			msg := err.Error()
			if te, ok := err.(*runtime.TypedError); ok {
				return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(te.Msg)}), nil
			}
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(msg)}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{forestToValue(forest)}), nil
	})
}

func forestToValue(f *grammar.Forest) runtime.Value {
	t := runtime.NewTable()
	_ = t.Set(runtime.Tag("text"), runtime.Str(f.Text))
	if f.Root != nil {
		_ = t.Set(runtime.Tag("root"), nodeToValue(f.Root))
	}
	return runtime.TableValue(t)
}

func nodeToValue(n *grammar.Node) runtime.Value {
	t := runtime.NewTable()
	_ = t.Set(runtime.Tag("kind"), runtime.Str(n.Kind))
	_ = t.Set(runtime.Tag("text"), runtime.Str(n.Text))
	if n.Symbol != "" {
		_ = t.Set(runtime.Tag("symbol"), runtime.Str(n.Symbol))
	}
	kids := make([]runtime.Value, len(n.Children))
	for i, c := range n.Children {
		kids[i] = nodeToValue(c)
	}
	_ = t.Set(runtime.Tag("children"), runtime.ListOf(kids))
	return runtime.TableValue(t)
}

// RegisterGrammars registers every `grammar`/`absyn` declaration in prog.
// Custom lexeme closures are fetched from hidden `__grammar_G_L` globals
// the compiler stored during Compile. Call RegisterGrammarsStatic before
// RunChunk (so DeriveParser works mid-chunk) and PatchGrammarFns after.
func RegisterGrammars(st *runtime.State, prog *ast.Program) error {
	if err := RegisterGrammarsStatic(st, prog); err != nil {
		return err
	}
	return PatchGrammarFns(st, prog)
}

// RegisterGrammarsStatic registers grammars/absyns with static parts only
// (custom lexeme Fns left nil). Safe to call before the chunk runs.
func RegisterGrammarsStatic(st *runtime.State, prog *ast.Program) error {
	for _, stmt := range prog.Stmts {
		switch x := stmt.(type) {
		case *ast.GrammarDecl:
			g, err := buildGrammarStatic(x)
			if err != nil {
				return err
			}
			if st.Grammars == nil {
				st.Grammars = make(map[string]*runtime.Grammar)
			}
			st.Grammars[g.Name] = g
		case *ast.AbsynDecl:
			a := buildAbsyn(x)
			if st.Absyns == nil {
				st.Absyns = make(map[string]*runtime.Absyn)
			}
			st.Absyns[a.Name] = a
		}
	}
	return nil
}

// PatchGrammarFns fills custom lexeme Fns from hidden globals after RunChunk.
func PatchGrammarFns(st *runtime.State, prog *ast.Program) error {
	for _, stmt := range prog.Stmts {
		x, ok := stmt.(*ast.GrammarDecl)
		if !ok {
			continue
		}
		g, ok := st.Grammars[x.Name]
		if !ok {
			continue
		}
		for _, l := range x.Lexemes {
			if l.Fn == nil {
				continue
			}
			hidden := "__grammar_" + x.Name + "_" + l.Name
			v, ok := st.LookupGlobal(hidden)
			if !ok {
				return fmt.Errorf("lexeme %q: missing custom function (internal %q)", l.Name, hidden)
			}
			if v.Kind() != runtime.KindFn {
				return fmt.Errorf("lexeme %q: custom lexer is not a function", l.Name)
			}
			g.Lexemes[l.Name] = &runtime.Lexeme{Name: l.Name, Kind: runtime.LexCustom, Fn: v.Fn()}
		}
	}
	return nil
}

func buildGrammar(st *runtime.State, x *ast.GrammarDecl) (*runtime.Grammar, error) {
	g, err := buildGrammarStatic(x)
	if err != nil {
		return nil, err
	}
	// Fill custom Fns from hidden globals (post-run).
	for _, l := range x.Lexemes {
		if l.Fn == nil {
			continue
		}
		hidden := "__grammar_" + x.Name + "_" + l.Name
		v, ok := st.LookupGlobal(hidden)
		if !ok {
			return nil, fmt.Errorf("lexeme %q: missing custom function (internal %q)", l.Name, hidden)
		}
		if v.Kind() != runtime.KindFn {
			return nil, fmt.Errorf("lexeme %q: custom lexer is not a function", l.Name)
		}
		g.Lexemes[l.Name] = &runtime.Lexeme{Name: l.Name, Kind: runtime.LexCustom, Fn: v.Fn()}
	}
	return g, nil
}

func buildGrammarStatic(x *ast.GrammarDecl) (*runtime.Grammar, error) {
	g := &runtime.Grammar{
		Name:     x.Name,
		Keywords: make(map[string]*runtime.Lexeme),
		Lexemes:  make(map[string]*runtime.Lexeme),
		Rules:    make(map[string]*runtime.Rule),
	}
	for _, k := range x.Keywords {
		if _, ok := g.Keywords[k.Name]; ok {
			return nil, fmt.Errorf("duplicate terminal %q in grammar %q", k.Name, x.Name)
		}
		g.Keywords[k.Name] = &runtime.Lexeme{Name: k.Name, Kind: runtime.LexLiteral, Literal: k.Literal}
	}
	for _, l := range x.Lexemes {
		if _, ok := g.Keywords[l.Name]; ok {
			return nil, fmt.Errorf("duplicate terminal %q in grammar %q", l.Name, x.Name)
		}
		if _, ok := g.Lexemes[l.Name]; ok {
			return nil, fmt.Errorf("duplicate terminal %q in grammar %q", l.Name, x.Name)
		}
		switch {
		case l.Regex != nil:
			re, err := runtime.NewRegex(*l.Regex)
			if err != nil {
				return nil, fmt.Errorf("lexeme %q: invalid regex %q: %v", l.Name, *l.Regex, err)
			}
			g.Lexemes[l.Name] = &runtime.Lexeme{Name: l.Name, Kind: runtime.LexPattern, Pattern: *l.Regex, Re: re.Re}
		case l.Literal != nil:
			g.Lexemes[l.Name] = &runtime.Lexeme{Name: l.Name, Kind: runtime.LexLiteral, Literal: *l.Literal}
		case l.Fn != nil:
			// Custom lexers are patched post-run (see PatchGrammarFns);
			// leave a placeholder so DeriveParser mid-chunk still finds
			// the grammar (with Fn nil until patched).
			g.Lexemes[l.Name] = &runtime.Lexeme{Name: l.Name, Kind: runtime.LexCustom}
		default:
			return nil, fmt.Errorf("lexeme %q has no pattern", l.Name)
		}
	}
	for _, r := range x.Rules {
		if _, ok := g.Rules[r.Name]; ok {
			return nil, fmt.Errorf("duplicate rule %q in grammar %q", r.Name, x.Name)
		}
		rule := &runtime.Rule{Name: r.Name}
		for _, alt := range r.Alts {
			var elems []runtime.RuleElem
			for _, e := range alt {
				elems = append(elems, convertRuleElem(e))
			}
			rule.Alts = append(rule.Alts, elems)
		}
		g.Rules[r.Name] = rule
		g.Order = append(g.Order, r.Name)
	}
	if len(g.Order) == 0 {
		return nil, fmt.Errorf("grammar %q has no rules", x.Name)
	}
	return g, nil
}

func convertRuleElem(e ast.RuleElem) runtime.RuleElem {
	out := runtime.RuleElem{Suffix: e.Suffix}
	switch e.Kind {
	case ast.KindRuleRef:
		out.Kind = runtime.RuleRef
		out.Name = e.Name
	case ast.KindRuleLit:
		out.Kind = runtime.RuleLit
		out.Name = e.Name
	case ast.KindRuleChar:
		out.Kind = runtime.RuleChar
		out.Char = e.Char
	case ast.KindRuleRegex:
		out.Kind = runtime.RuleRegex
		out.Pattern = e.Name
	case ast.KindRuleGroup:
		out.Kind = runtime.RuleGroup
		for _, alt := range e.Group {
			var elems []runtime.RuleElem
			for _, se := range alt {
				elems = append(elems, convertRuleElem(se))
			}
			out.Group = append(out.Group, elems)
		}
	}
	return out
}

func buildAbsyn(x *ast.AbsynDecl) *runtime.Absyn {
	a := &runtime.Absyn{Name: x.Name, Types: make(map[string]*runtime.AbsynType)}
	for _, ty := range x.Types {
		t := &runtime.AbsynType{Name: ty.Name}
		for _, c := range ty.Ctors {
			ctor := runtime.AbsynCtor{Name: c.Name}
			for _, f := range c.Fields {
				ctor.Fields = append(ctor.Fields, runtime.AbsynField{Type: f.Type, Name: f.Name})
			}
			t.Ctors = append(t.Ctors, ctor)
		}
		a.Types[ty.Name] = t
		a.Order = append(a.Order, ty.Name)
	}
	return a
}
