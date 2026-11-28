// Package parser: interp.go implements `$`-interpolation expansion,
// `~` typed-error desugaring, and expression cloning for `+=` targets.
package parser

import (
	"strings"

	"domscript/internal/ast"
	"domscript/internal/diag"
	"domscript/internal/lexer"
	"domscript/internal/token"
)

func spanOf(t token.Token) diag.Span { return diag.Span{Start: t.Pos, End: t.End} }

// parseInterpCode parses `${code}` interpolation payloads as expressions.
func (p *Parser) parseInterpCode(code string, base token.Pos) ast.Expr {
	if strings.TrimSpace(code) == "" {
		p.diags.Errorf(diag.Span{Start: base, End: base}, "empty ${...} interpolation")
		return nil
	}
	subDiags := diag.NewSet(p.chunk, code)
	toks := lexer.Scan(code, p.chunk, subDiags)
	sub := &Parser{toks: toks, diags: subDiags, src: code, chunk: p.chunk}
	e := sub.parseExpr()
	if subDiags.HasErrors() {
		p.diags.Errorf(diag.Span{Start: base, End: base}, "invalid interpolation ${%s}", code)
		return nil
	}
	if e == nil {
		p.diags.Errorf(diag.Span{Start: base, End: base}, "invalid interpolation ${%s}", code)
		return nil
	}
	if !sub.at(token.EOF) {
		p.diags.Errorf(diag.Span{Start: base, End: base}, "invalid interpolation ${%s}", code)
		return nil
	}
	return e
}

// ------------------------------------------------------------------ cloning

// cloneExpr deep-copies an expression (used for `x op= y` desugaring where
// the target appears twice). Returns nil for nodes that cannot be cloned.
func cloneExpr(e ast.Expr) ast.Expr {
	switch x := e.(type) {
	case *ast.Ident:
		return &ast.Ident{Node: x.Node, Name: x.Name}
	case *ast.IntLit:
		return &ast.IntLit{Node: x.Node, Value: x.Value, Text: x.Text}
	case *ast.FloatLit:
		return &ast.FloatLit{Node: x.Node, Value: x.Value, Text: x.Text}
	case *ast.StringLit:
		return &ast.StringLit{Node: x.Node, Value: x.Value, Raw: x.Raw}
	case *ast.InterpLit:
		parts := make([]ast.Expr, len(x.Parts))
		for i, p := range x.Parts {
			c := cloneExpr(p)
			if c == nil {
				return nil
			}
			parts[i] = c
		}
		return &ast.InterpLit{Node: x.Node, Parts: parts}
	case *ast.CharLit:
		return &ast.CharLit{Node: x.Node, Value: x.Value}
	case *ast.RegexLit:
		return &ast.RegexLit{Node: x.Node, Pattern: x.Pattern}
	case *ast.NullLit:
		return &ast.NullLit{Node: x.Node}
	case *ast.BoolLit:
		return &ast.BoolLit{Node: x.Node, Value: x.Value}
	case *ast.UnitLit:
		return &ast.UnitLit{Node: x.Node}
	case *ast.TagLit:
		return &ast.TagLit{Node: x.Node, Name: x.Name}
	case *ast.SelectorLit:
		return &ast.SelectorLit{Node: x.Node, Src: x.Src}
	case *ast.ListLit:
		elems := make([]ast.Expr, len(x.Elems))
		for i, el := range x.Elems {
			c := cloneExpr(el)
			if c == nil {
				return nil
			}
			elems[i] = c
		}
		return &ast.ListLit{Node: x.Node, Elems: elems}
	case *ast.TableLit:
		entries := make([]ast.TableEntry, len(x.Entries))
		for i, en := range x.Entries {
			k := cloneExpr(en.Key)
			v := cloneExpr(en.Value)
			if k == nil || v == nil {
				return nil
			}
			entries[i] = ast.TableEntry{Node: en.Node, Key: k, Value: v}
		}
		return &ast.TableLit{Node: x.Node, Entries: entries}
	case *ast.TaggedLit:
		elems := make([]ast.Expr, len(x.Elems))
		for i, el := range x.Elems {
			c := cloneExpr(el)
			if c == nil {
				return nil
			}
			elems[i] = c
		}
		return &ast.TaggedLit{Node: x.Node, Tag: x.Tag, Elems: elems}
	case *ast.Binary:
		l := cloneExpr(x.L)
		r := cloneExpr(x.R)
		if l == nil || r == nil {
			return nil
		}
		return &ast.Binary{Node: x.Node, Op: x.Op, L: l, R: r}
	case *ast.Unary:
		c := cloneExpr(x.X)
		if c == nil {
			return nil
		}
		return &ast.Unary{Node: x.Node, Op: x.Op, X: c}
	case *ast.OptNone:
		return &ast.OptNone{Node: x.Node}
	case *ast.EffectResult:
		return &ast.EffectResult{Node: x.Node}
	case *ast.Call:
		fn := cloneExpr(x.Fn)
		if fn == nil {
			return nil
		}
		args := make([]ast.Expr, len(x.Args))
		for i, a := range x.Args {
			c := cloneExpr(a)
			if c == nil {
				return nil
			}
			args[i] = c
		}
		return &ast.Call{Node: x.Node, Fn: fn, Args: args}
	case *ast.Index:
		b := cloneExpr(x.X)
		i := cloneExpr(x.I)
		if b == nil || i == nil {
			return nil
		}
		return &ast.Index{Node: x.Node, X: b, I: i}
	case *ast.Field:
		b := cloneExpr(x.X)
		if b == nil {
			return nil
		}
		return &ast.Field{Node: x.Node, X: b, Name: x.Name}
	}
	return nil
}

// ------------------------------------------------------- string interpolation

// isIdentStart reports whether c can start a `$name` interpolation.
func isInterpIdentStart(c byte) bool {
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
}

func isInterpIdentCont(c byte) bool {
	return isInterpIdentStart(c) || (c >= '0' && c <= '9')
}

// cookEscape expands a single `\x` escape (raw contains the backslash and
// the following char, or `\u{...}`). It mirrors the lexer's accepted set
// plus `\$`. Unknown escapes are preserved verbatim (the lexer already
// reported them).
func cookEscape(raw string, i int) (string, int) {
	// raw[i] == '\\', raw[i+1] exists.
	e := raw[i+1]
	switch e {
	case 'n':
		return "\n", i + 2
	case 't':
		return "\t", i + 2
	case 'r':
		return "\r", i + 2
	case '\\':
		return "\\", i + 2
	case '"':
		return "\"", i + 2
	case '0':
		return "\x00", i + 2
	case '$':
		return "$", i + 2
	case 'u':
		// `\u{XXXX}` (validated by the lexer; re-expand here).
		if i+2 < len(raw) && raw[i+2] == '{' {
			j := i + 3
			hex := 0
			n := 0
			for j < len(raw) && n < 8 {
				h := raw[j]
				var v int
				switch {
				case h >= '0' && h <= '9':
					v = int(h - '0')
				case h >= 'a' && h <= 'f':
					v = int(h-'a') + 10
				case h >= 'A' && h <= 'F':
					v = int(h-'A') + 10
				default:
					j = -1
					break
				}
				if j == -1 {
					break
				}
				hex = hex*16 + v
				n++
				j++
			}
			if j != -1 && n > 0 && j < len(raw) && raw[j] == '}' && hex <= 0x10FFFF {
				return string(rune(hex)), j + 1
			}
		}
		return raw[i : i+2], i + 2
	default:
		return raw[i : i+2], i + 2
	}
}

// findInterpCodeEnd finds the `}` matching the `${` at raw[start-2:].
// start is the offset just after `${`. It respects `"..."` strings and
// nested braces. Returns the offset of the matching `}` or -1.
func findInterpCodeEnd(raw string, start int) int {
	depth := 1
	i := start
	for i < len(raw) {
		c := raw[i]
		if c == '"' {
			i++
			for i < len(raw) {
				d := raw[i]
				if d == '\\' {
					i += 2
					continue
				}
				i++
				if d == '"' {
					break
				}
				if d == '\n' {
					return -1
				}
			}
			continue
		}
		if c == '{' {
			depth++
			i++
			continue
		}
		if c == '}' {
			depth--
			if depth == 0 {
				return i
			}
			i++
			continue
		}
		i++
	}
	return -1
}

// makeStringExpr expands a raw string token (backslashes preserved by the
// lexer) into a StringLit or an InterpLit. `$name` binds an Ident;
// `${code}` parses code as an expression; `$$` and `\$` yield `$`.
func (p *Parser) makeStringExpr(t token.Token) ast.Expr {
	raw := t.Text
	// Fast path: no `$` at all (escapes only).
	hasDollar := false
	for i := 0; i < len(raw); i++ {
		if raw[i] == '$' {
			hasDollar = true
			break
		}
	}
	if !hasDollar {
		return &ast.StringLit{
			Node:  ast.Node{Pos: t.Pos, End: t.End},
			Value: cookStringNoInterp(raw),
			Raw:   raw,
		}
	}
	var parts []ast.Expr
	var lit strings.Builder
	flushLit := func() {
		if lit.Len() > 0 {
			parts = append(parts, &ast.StringLit{
				Node:  ast.Node{Pos: t.Pos, End: t.End},
				Value: lit.String(),
			})
			lit.Reset()
		}
	}
	i := 0
	for i < len(raw) {
		c := raw[i]
		if c == '\\' && i+1 < len(raw) {
			s, ni := cookEscape(raw, i)
			lit.WriteString(s)
			i = ni
			continue
		}
		if c == '$' {
			if i+1 < len(raw) && raw[i+1] == '$' {
				lit.WriteByte('$')
				i += 2
				continue
			}
			if i+1 < len(raw) && raw[i+1] == '{' {
				end := findInterpCodeEnd(raw, i+2)
				if end == -1 {
					p.diags.Errorf(spanOf(t), "unterminated ${...} interpolation")
					lit.WriteString("${")
					i += 2
					continue
				}
				code := raw[i+2 : end]
				flushLit()
				e := p.parseInterpCode(code, t.Pos)
				if e == nil {
					// Error already reported; splice empty string.
					e = &ast.StringLit{Node: ast.Node{Pos: t.Pos, End: t.End}}
				}
				parts = append(parts, e)
				i = end + 1
				continue
			}
			if i+1 < len(raw) && isInterpIdentStart(raw[i+1]) {
				j := i + 2
				for j < len(raw) && isInterpIdentCont(raw[j]) {
					j++
				}
				name := raw[i+1 : j]
				flushLit()
				parts = append(parts, &ast.Ident{
					Node: ast.Node{Pos: t.Pos, End: t.End},
					Name: name,
				})
				i = j
				continue
			}
			// Lone `$` (before space, punctuation, digit, end, ...).
			lit.WriteByte('$')
			i++
			continue
		}
		lit.WriteByte(c)
		i++
	}
	flushLit()
	if len(parts) == 0 {
		return &ast.StringLit{
			Node:  ast.Node{Pos: t.Pos, End: t.End},
			Value: "",
			Raw:   raw,
		}
	}
	// Only one part and it is a literal (no actual interpolation found:
	// e.g. `"price: $5"` where `$` is followed by a digit).
	if len(parts) == 1 {
		if s, ok := parts[0].(*ast.StringLit); ok {
			s.Raw = raw
			return s
		}
	}
	return &ast.InterpLit{Node: ast.Node{Pos: t.Pos, End: t.End}, Parts: parts}
}

// cookStringNoInterp expands escapes without interpolation (used for
// grammar literals and single-part strings). `\$` becomes `$`.
func cookStringNoInterp(raw string) string {
	var sb strings.Builder
	i := 0
	for i < len(raw) {
		if raw[i] == '\\' && i+1 < len(raw) {
			s, ni := cookEscape(raw, i)
			sb.WriteString(s)
			i = ni
			continue
		}
		sb.WriteByte(raw[i])
		i++
	}
	return sb.String()
}
