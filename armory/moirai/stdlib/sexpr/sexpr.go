// Package sexpr implements a small, conventional S-expression reader and
// printer. Symbols are represented as Domscript tags, lists as Domscript
// lists, and strings/numbers/bools retain their corresponding value kinds.
package sexpr

import (
	"fmt"
	"strconv"
	"strings"
	"unicode"
	"unicode/utf8"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__sexpr_"+name, fn)
	}

	reg("parse", func(_ *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) != 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("parse expects a :String")
		}
		v, err := parse(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{v}), nil
	})

	reg("encode", func(_ *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) != 1 {
			return runtime.Unit(), fmt.Errorf("encode expects a value")
		}
		text, err := encode(args[0], false)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(text)}), nil
	})

	reg("pretty", func(_ *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) != 1 {
			return runtime.Unit(), fmt.Errorf("pretty expects a value")
		}
		text, err := encode(args[0], true)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(text)}), nil
	})

	reg("validate", func(_ *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) != 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("validate expects a :String")
		}
		if _, err := parse(args[0].StrValue()); err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Bool(true)}), nil
	})
}

type reader struct {
	src  string
	off  int
	line int
	col  int
}

func parse(src string) (runtime.Value, error) {
	r := &reader{src: src, line: 1, col: 1}
	r.space()
	if r.eof() {
		return runtime.Unit(), fmt.Errorf("empty S-expression")
	}
	v, err := r.expr()
	if err != nil {
		return runtime.Unit(), err
	}
	r.space()
	if !r.eof() {
		return runtime.Unit(), r.errf("unexpected %q after expression", r.peek())
	}
	return v, nil
}

func (r *reader) expr() (runtime.Value, error) {
	r.space()
	if r.eof() {
		return runtime.Unit(), r.errf("expected expression")
	}
	switch r.peek() {
	case '(':
		return r.list()
	case ')':
		return runtime.Unit(), r.errf("unexpected ')'")
	case '"':
		return r.string()
	case '\'':
		r.next()
		v, err := r.expr()
		if err != nil {
			return runtime.Unit(), err
		}
		return runtime.ListOf([]runtime.Value{runtime.Tag("quote"), v}), nil
	default:
		return r.atom()
	}
}

func (r *reader) list() (runtime.Value, error) {
	r.next() // '('
	var elems []runtime.Value
	for {
		r.space()
		if r.eof() {
			return runtime.Unit(), r.errf("unterminated list")
		}
		if r.peek() == ')' {
			r.next()
			return runtime.ListOf(elems), nil
		}
		v, err := r.expr()
		if err != nil {
			return runtime.Unit(), err
		}
		elems = append(elems, v)
	}
}

func (r *reader) string() (runtime.Value, error) {
	start := r.off
	r.next() // opening quote
	for !r.eof() {
		if r.peek() == '"' {
			r.next()
			text, err := strconv.Unquote(r.src[start:r.off])
			if err != nil {
				return runtime.Unit(), r.errf("invalid string: %v", err)
			}
			return runtime.Str(text), nil
		}
		if r.next() == '\\' && !r.eof() {
			r.next()
		}
	}
	return runtime.Unit(), r.errf("unterminated string")
}

func (r *reader) atom() (runtime.Value, error) {
	start := r.off
	for !r.eof() && !unicode.IsSpace(r.rune()) && r.peek() != '(' && r.peek() != ')' && r.peek() != ';' {
		r.next()
	}
	word := r.src[start:r.off]
	if word == "" {
		return runtime.Unit(), r.errf("expected atom")
	}
	switch word {
	case "nil":
		return runtime.Unit(), nil
	case "true", "#t":
		return runtime.Bool(true), nil
	case "false", "#f":
		return runtime.Bool(false), nil
	}
	if i, err := strconv.ParseInt(word, 10, 64); err == nil {
		return runtime.Int(i), nil
	}
	if f, err := strconv.ParseFloat(word, 64); err == nil && strings.ContainsAny(word, ".eE") {
		return runtime.Float(f), nil
	}
	return runtime.Tag(word), nil
}

func (r *reader) space() {
	for !r.eof() {
		if unicode.IsSpace(r.rune()) {
			r.next()
			continue
		}
		if r.peek() == ';' {
			for !r.eof() && r.next() != '\n' {
			}
			continue
		}
		return
	}
}

func (r *reader) eof() bool  { return r.off >= len(r.src) }
func (r *reader) peek() byte { return r.src[r.off] }
func (r *reader) rune() rune {
	v, _ := utf8.DecodeRuneInString(r.src[r.off:])
	return v
}
func (r *reader) next() byte {
	c := r.src[r.off]
	_, size := utf8.DecodeRuneInString(r.src[r.off:])
	if size == 0 {
		size = 1
	}
	r.off += size
	if c == '\n' {
		r.line++
		r.col = 1
	} else {
		r.col++
	}
	return c
}
func (r *reader) errf(format string, args ...any) error {
	return fmt.Errorf("S-expression at %d:%d: %s", r.line, r.col, fmt.Sprintf(format, args...))
}

func encode(v runtime.Value, pretty bool) (string, error) {
	return encodeAt(v, pretty, 0)
}

func encodeAt(v runtime.Value, pretty bool, indent int) (string, error) {
	switch v.Kind() {
	case runtime.KindUnit:
		return "nil", nil
	case runtime.KindBool:
		if v.BoolValue() {
			return "true", nil
		}
		return "false", nil
	case runtime.KindInt:
		return strconv.FormatInt(v.IntValue(), 10), nil
	case runtime.KindFloat:
		return strconv.FormatFloat(v.FloatValue(), 'g', -1, 64), nil
	case runtime.KindString:
		return strconv.Quote(v.StrValue()), nil
	case runtime.KindTag:
		if !validSymbol(v.StrValue()) {
			return "", fmt.Errorf("cannot encode invalid S-expression symbol %q", v.StrValue())
		}
		return v.StrValue(), nil
	case runtime.KindList:
		elems := v.List().Elems
		if len(elems) == 0 {
			return "()", nil
		}
		parts := make([]string, len(elems))
		for i, elem := range elems {
			part, err := encodeAt(elem, pretty, indent+1)
			if err != nil {
				return "", err
			}
			parts[i] = part
		}
		if !pretty || !hasNestedList(elems) {
			return "(" + strings.Join(parts, " ") + ")", nil
		}
		pad := strings.Repeat("  ", indent+1)
		return "(\n" + pad + strings.Join(parts, "\n"+pad) + "\n" + strings.Repeat("  ", indent) + ")", nil
	default:
		return "", fmt.Errorf("cannot encode %s as an S-expression", runtime.TypeName(v))
	}
}

func hasNestedList(elems []runtime.Value) bool {
	for _, elem := range elems {
		if elem.Kind() == runtime.KindList {
			return true
		}
	}
	return false
}

func validSymbol(symbol string) bool {
	if symbol == "" || strings.ContainsAny(symbol, "()\"';") {
		return false
	}
	for _, r := range symbol {
		if unicode.IsSpace(r) || unicode.IsControl(r) {
			return false
		}
	}
	return true
}
