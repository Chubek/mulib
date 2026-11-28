// Package lexer implements the UTF-8 scanner for Domscript, including the
// markup-mode switch between %< ... %> fences (Sections 3 and 6.3).
//
// The lexer maintains an explicit mode stack. The base mode is expression
// mode. Encountering %< pushes a fence-body (markup) mode; inside markup,
// a splice "{" pushes expression mode back on, and that expression mode's
// terminating "}" at bracket depth zero pops back to markup. Node literals
// nested inside splices inside node literals therefore nest arbitrarily.
package lexer

import (
	"strings"
	"unicode/utf8"

	"domscript/internal/diag"
	"domscript/internal/token"
)

type modeKind uint8

const (
	modeExpr      modeKind = iota // expression mode; depth counts ( and [
	modeFenceBody                 // markup: top-level body of a %< ... %> fence
	modeElemHead                  // markup: inside <name ... , before > or />
	modeElemBody                  // markup: element body, after <name ... >
)

type mode struct {
	kind       modeKind
	depth      int  // expr mode: ( [ nesting
	braceDepth int  // expr mode: { } nesting (splice termination only)
	fromSplice bool // expr mode: entered from a markup splice
	fenceFresh bool // fence body: a pending '<' may belong to the fence itself
}

// Lexer scans one source file into a token slice, reporting problems into
// a diag.Set. Scanning always runs to EOF; invalid input produces
// diagnostics plus a best-effort token stream.
type Lexer struct {
	src   string
	file  string
	off   int
	line  int
	col   int
	modes []mode
	diags *diag.Set
	out   []token.Token
	prev  token.Kind // last significant token kind (newline suppression)
}

// Scan tokenizes src. file names diagnostics; diags collects them.
func Scan(src, file string, diags *diag.Set) []token.Token {
	lx := &Lexer{src: src, file: file, line: 1, col: 1, diags: diags, prev: token.EOF}
	lx.modes = append(lx.modes, mode{kind: modeExpr})
	for {
		tok := lx.next()
		lx.out = append(lx.out, tok)
		if tok.Kind == token.EOF {
			return lx.out
		}
	}
}

func (lx *Lexer) pos() token.Pos {
	return token.Pos{File: lx.file, Line: lx.line, Col: lx.col, Off: lx.off}
}

func (lx *Lexer) cur() mode { return lx.modes[len(lx.modes)-1] }

func (lx *Lexer) errAt(p token.Pos, format string, args ...any) {
	lx.diags.Errorf(diag.Span{Start: p, End: p}, format, args...)
}

// peek returns the byte at off+n without consuming.
func (lx *Lexer) peek(n int) byte {
	if lx.off+n < len(lx.src) {
		return lx.src[lx.off+n]
	}
	return 0
}

// advance consumes one byte, maintaining line/column.
func (lx *Lexer) advance() byte {
	c := lx.src[lx.off]
	lx.off++
	if c == '\n' {
		lx.line++
		lx.col = 1
	} else {
		lx.col++
	}
	return c
}

func (lx *Lexer) emit(k token.Kind, text string, start token.Pos) token.Token {
	end := lx.pos()
	if k != token.Newline {
		lx.prev = k
	}
	return token.Token{Kind: k, Text: text, Pos: start, End: end}
}

func isASCIIIdentStart(c byte) bool {
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
}
func isASCIIIdentCont(c byte) bool { return isASCIIIdentStart(c) || (c >= '0' && c <= '9') }
func isDigit(c byte) bool          { return c >= '0' && c <= '9' }
func isHexDigit(c byte) bool {
	return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')
}
func isMarkupNameStart(c byte) bool { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') }
func isMarkupNameCont(c byte) bool {
	return isMarkupNameStart(c) || isDigit(c) || c == '-'
}

// next scans and returns the next token.
func (lx *Lexer) next() token.Token {
	switch lx.cur().kind {
	case modeExpr:
		return lx.nextExpr()
	default:
		return lx.nextMarkup()
	}
}

// ---------------------------------------------------------------- expr mode

func (lx *Lexer) nextExpr() token.Token {
	// Whitespace, comments, and newline handling.
	for lx.off < len(lx.src) {
		c := lx.peek(0)
		if c == ' ' || c == '\t' || c == '\r' && lx.peek(1) != '\n' {
			lx.advance()
			continue
		}
		if c == '/' && lx.peek(1) == '/' {
			for lx.off < len(lx.src) && lx.peek(0) != '\n' {
				lx.advance()
			}
			continue
		}
		if c == '/' && lx.peek(1) == '*' {
			start := lx.pos()
			lx.advance()
			lx.advance()
			for {
				if lx.off >= len(lx.src) {
					lx.errAt(start, "unterminated block comment")
					break
				}
				if lx.peek(0) == '*' && lx.peek(1) == '/' {
					lx.advance()
					lx.advance()
					break
				}
				lx.checkUTF8()
				lx.advance()
			}
			continue
		}
		break
	}
	if lx.off >= len(lx.src) {
		p := lx.pos()
		if len(lx.modes) > 1 {
			lx.errAt(p, "unexpected end of file: unclosed fence or splice")
		}
		return lx.emit(token.EOF, "", p)
	}

	start := lx.pos()
	c := lx.peek(0)

	// Line break: significant only at bracket depth zero, in base
	// expression mode, and only when the previous significant token can
	// end an expression (Section 3.3).
	if c == '\n' || (c == '\r' && lx.peek(1) == '\n') {
		lx.advance()
		if c == '\r' {
			lx.advance() // consume the \n of \r\n
		}
		m := lx.cur()
		if m.depth == 0 && !m.fromSplice && lx.prev.CanEndExpr() {
			return lx.emit(token.Newline, "\n", start)
		}
		return lx.next()
	}

	switch {
	case c == '(':
		lx.advance()
		lx.bumpDepth(1)
		return lx.emit(token.LParen, "", start)
	case c == ')':
		lx.advance()
		lx.bumpDepth(-1)
		return lx.emit(token.RParen, "", start)
	case c == '[':
		lx.advance()
		lx.bumpDepth(1)
		return lx.emit(token.LBracket, "", start)
	case c == ']':
		lx.advance()
		lx.bumpDepth(-1)
		return lx.emit(token.RBracket, "", start)
	case c == '{':
		lx.advance()
		lx.modes[len(lx.modes)-1].braceDepth++
		return lx.emit(token.LBrace, "", start)
	case c == '}':
		m := lx.cur()
		if m.fromSplice && m.depth == 0 && m.braceDepth == 0 {
			lx.advance()
			lx.modes = lx.modes[:len(lx.modes)-1]
			return lx.emit(token.MSpliceClose, "", start)
		}
		if m.braceDepth > 0 {
			lx.modes[len(lx.modes)-1].braceDepth--
		}
		lx.advance()
		return lx.emit(token.RBrace, "", start)
	case c == ',':
		lx.advance()
		return lx.emit(token.Comma, "", start)
	case c == ';':
		lx.advance()
		return lx.emit(token.Semi, "", start)
	case c == '"':
		return lx.scanString(start)
	case c == '$' && lx.peek(1) == '"':
		return lx.scanSelector(start)
	case c == ':' && lx.peek(1) == ':' && lx.peek(2) == '=':
		lx.advance()
		lx.advance()
		lx.advance()
		return lx.emit(token.Define, "::=", start)
	case c == ':':
		return lx.scanTag(start)
	case c == '#' && lx.peek(1) == '\\':
		return lx.scanChar(start)
	case c == '|':
		lx.advance()
		return lx.emit(token.Pipe, "|", start)
	case isDigit(c):
		return lx.scanNumber(start)
	case isASCIIIdentStart(c):
		var sb strings.Builder
		for lx.off < len(lx.src) && isASCIIIdentCont(lx.peek(0)) {
			sb.WriteByte(lx.advance())
		}
		word := sb.String()
		return lx.emit(token.Keyword(word), word, start)
	}

	// Multi-char operators first.
	two := func(a, b byte) bool { return c == a && lx.peek(1) == b }
	switch {
	case two('+', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.PlusAssign, "+=", start)
	case two('-', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.MinusAssign, "-=", start)
	case two('*', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.StarAssign, "*=", start)
	case two('/', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.SlashAssign, "/=", start)
	case two('%', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.PercentAssign, "%=", start)
	case two('=', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.Eq, "", start)
	case two('!', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.Neq, "", start)
	case two('<', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.Le, "", start)
	case two('>', '='):
		lx.advance()
		lx.advance()
		return lx.emit(token.Ge, "", start)
	case two('.', '.'):
		lx.advance()
		lx.advance()
		return lx.emit(token.DotDot, "", start)
	case two('=', '>'):
		lx.advance()
		lx.advance()
		return lx.emit(token.FatArrow, "", start)
	case two('-', '>'):
		lx.advance()
		lx.advance()
		return lx.emit(token.ThinArrow, "", start)
	case two('%', '{'):
		lx.advance()
		lx.advance()
		return lx.emit(token.TableOpen, "", start)
	case two('%', '<'):
		// %< opens the fence but the '<' may belong to the markup body: in
		// %<div ... %> the fence's '<' doubles as the first element's
		// opening angle (Section 6.3 examples); in %<{a}{b}%> it is elided
		// and the fragment body starts at the splice.
		lx.advance()
		lx.modes = append(lx.modes, mode{kind: modeFenceBody, fenceFresh: true})
		return lx.emit(token.NodeOpen, "", start)
	case two('%', '>'):
		lx.advance()
		lx.advance()
		lx.errAt(start, "unexpected fence end: no open node literal")
		return lx.next()
	}

	// A `/` that cannot be division starts a regex literal `/.../`
	// (grammar lexeme patterns). Division follows a value (`a / b`);
	// a regex follows `::=`, `|`, `(`, `,`, an operator, etc.
	if c == '/' && !lx.prev.CanEndExpr() {
		if tok, ok := lx.tryScanRegex(start); ok {
			return tok
		}
	}

	one := map[byte]token.Kind{
		'=': token.Assign, '<': token.Lt, '>': token.Gt,
		'+': token.Plus, '-': token.Minus, '*': token.Star,
		'/': token.Slash, '%': token.Percent, '?': token.Question,
		'~': token.Tilde, '.': token.Dot, '$': token.Dollar,
	}
	if k, ok := one[c]; ok {
		lx.advance()
		return lx.emit(k, "", start)
	}

	// Non-ASCII or invalid byte.
	if c >= 0x80 {
		if !lx.checkUTF8() {
			lx.errAt(start, "invalid UTF-8 encoding")
			lx.advance()
			return lx.next()
		}
	}
	lx.errAt(start, "invalid character %q", string(rune(c)))
	lx.advance()
	return lx.next()
}

// bumpDepth adjusts the bracket depth of the current expr mode.
func (lx *Lexer) bumpDepth(d int) {
	m := &lx.modes[len(lx.modes)-1]
	m.depth += d
	if m.depth < 0 {
		m.depth = 0
	}
}

// checkUTF8 validates the rune at the cursor; on invalid encoding it
// consumes one byte and reports. Returns false when invalid.
func (lx *Lexer) checkUTF8() bool {
	if lx.off >= len(lx.src) {
		return true
	}
	r, size := utf8.DecodeRuneInString(lx.src[lx.off:])
	if r == utf8.RuneError && size <= 1 {
		return false
	}
	return true
}

func (lx *Lexer) scanString(start token.Pos) token.Token {
	// Raw capture: preserve backslashes verbatim so the parser can
	// expand escapes and `$` interpolation in one pass (this keeps
	// `\$` distinct from an interpolation `$`). Only `\"` and `\\`
	// affect termination; everything else is verbatim until `"`.
	lx.advance() // opening quote
	startOff := lx.off
	var raw strings.Builder
	for {
		if lx.off >= len(lx.src) {
			lx.errAt(start, "unterminated string literal")
			return lx.emit(token.String, lx.src[startOff:], start)
		}
		c := lx.peek(0)
		if c == '"' {
			text := raw.String()
			// raw was built incrementally; but to preserve exact bytes
			// we already appended; just close.
			lx.advance()
			return lx.emit(token.String, text, start)
		}
		if c == '\n' {
			lx.errAt(start, "unterminated string literal (newline in string)")
			return lx.emit(token.String, raw.String(), start)
		}
		if c == '\\' {
			// Preserve the backslash and the following byte verbatim
			// (including `\"` so it does not close the string), but
			// still validate the escape so `\"a\q\"` reports at lex
			// time as before. `\$` is the interpolation escape.
			escPos := lx.pos()
			raw.WriteByte(lx.advance())
			if lx.off >= len(lx.src) {
				lx.errAt(start, "unterminated escape sequence")
				return lx.emit(token.String, raw.String(), start)
			}
			if lx.peek(0) == '\n' {
				lx.errAt(start, "unterminated string literal (newline in string)")
				return lx.emit(token.String, raw.String(), start)
			}
			if lx.peek(0) >= 0x80 {
				r, size := utf8.DecodeRuneInString(lx.src[lx.off:])
				if r == utf8.RuneError && size <= 1 {
					lx.errAt(lx.pos(), "invalid UTF-8 encoding in string literal")
					lx.advance()
					raw.WriteByte('?')
					continue
				}
				raw.WriteString(lx.src[lx.off : lx.off+size])
				for i := 0; i < size; i++ {
					lx.advance()
				}
				continue
			}
			e := lx.peek(0)
			switch e {
			case 'n', 't', 'r', '\\', '"', '0', '$':
				raw.WriteByte(lx.advance())
			case 'u':
				// Validate `\u{XXXX}` shape now; the parser re-expands.
				raw.WriteByte(lx.advance())
				if lx.peek(0) != '{' {
					lx.errAt(escPos, "expected '{' after \\u escape")
					continue
				}
				raw.WriteByte(lx.advance())
				hexStart := lx.off
				for lx.off < len(lx.src) && isHexDigit(lx.peek(0)) {
					raw.WriteByte(lx.advance())
					if lx.off-hexStart > 8 {
						break
					}
				}
				hexLen := lx.off - hexStart
				if lx.off >= len(lx.src) || lx.peek(0) != '}' || hexLen == 0 || hexLen > 8 {
					lx.errAt(escPos, "malformed \\u{XXXX} escape")
					continue
				}
				raw.WriteByte(lx.advance())
			default:
				lx.errAt(escPos, "unknown escape sequence \\%s", string(rune(e)))
				raw.WriteByte(lx.advance())
			}
			continue
		}
		if c >= 0x80 {
			r, size := utf8.DecodeRuneInString(lx.src[lx.off:])
			if r == utf8.RuneError && size <= 1 {
				lx.errAt(lx.pos(), "invalid UTF-8 encoding in string literal")
				lx.advance()
				continue
			}
			raw.WriteString(lx.src[lx.off : lx.off+size])
			for i := 0; i < size; i++ {
				lx.advance()
			}
			continue
		}
		raw.WriteByte(lx.advance())
	}
}

func hexVal(c byte) int {
	switch {
	case c >= '0' && c <= '9':
		return int(c - '0')
	case c >= 'a' && c <= 'f':
		return int(c-'a') + 10
	default:
		return int(c-'A') + 10
	}
}

func (lx *Lexer) scanChar(start token.Pos) token.Token {
	// Consumes `#\` + name. Single non-letter chars are literal
	// (`#\,` => `,`); letter runs resolve via charNames
	// (`#\newline` => "\n"). Mirrors Scheme character literals.
	lx.advance() // '#'
	lx.advance() // '\\'
	if lx.off >= len(lx.src) {
		lx.errAt(start, "unterminated character literal")
		return lx.emit(token.Char, "", start)
	}
	c := lx.peek(0)
	if c == '\n' {
		lx.errAt(start, "unterminated character literal")
		return lx.emit(token.Char, "", start)
	}
	// Named character: maximal letter run.
	if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') {
		var sb strings.Builder
		for lx.off < len(lx.src) {
			d := lx.peek(0)
			if (d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') {
				sb.WriteByte(lx.advance())
			} else {
				break
			}
		}
		name := sb.String()
		if v, ok := charNames[name]; ok {
			return lx.emit(token.Char, v, start)
		}
		if len(name) == 1 {
			return lx.emit(token.Char, name, start)
		}
		// Try longest known prefix (e.g. `#\n` vs `#\newline` is
		// unambiguous since we consumed maximally; unknown multi-letter
		// names are errors).
		lx.errAt(start, "unknown character name #%s", "\\"+name)
		return lx.emit(token.Char, name, start)
	}
	// Single character (including `"`, `\`, `/`, `[`, etc., and UTF-8).
	if c >= 0x80 {
		r, size := utf8.DecodeRuneInString(lx.src[lx.off:])
		if r == utf8.RuneError && size <= 1 {
			lx.errAt(start, "invalid UTF-8 encoding in character literal")
			lx.advance()
			return lx.emit(token.Char, "", start)
		}
		s := lx.src[lx.off : lx.off+size]
		for i := 0; i < size; i++ {
			lx.advance()
		}
		return lx.emit(token.Char, s, start)
	}
	lx.advance()
	return lx.emit(token.Char, string([]byte{c}), start)
}

// charNames maps Scheme-like `#\name` literals to their single character.
var charNames = map[string]string{
	"newline": "\n",
	"space":   " ",
	"tab":     "\t",
	"return":  "\r",
	"bell":    "\x07",
	"feed":    "\x0c",
	"escape":  "\x1b",
	"null":    "\x00",
}

// tryScanRegex attempts `/pattern/` when the slash cannot be division.
// It scans ahead without consuming on failure; on success it consumes
// through the closing `/` and emits a Regex token with the inner text.
func (lx *Lexer) tryScanRegex(start token.Pos) (token.Token, bool) {
	off := lx.off + 1 // after opening '/'
	if off >= len(lx.src) {
		return token.Token{}, false
	}
	// Opening `/` followed by newline cannot start a regex.
	if lx.src[off] == '\n' {
		return token.Token{}, false
	}
	escaped := false
	inClass := false
	i := off
	for i < len(lx.src) {
		c := lx.src[i]
		if c == '\n' {
			return token.Token{}, false
		}
		if escaped {
			escaped = false
			i++
			continue
		}
		if c == '\\' {
			escaped = true
			i++
			continue
		}
		if c == '[' && !inClass {
			inClass = true
			i++
			continue
		}
		if c == ']' && inClass {
			inClass = false
			i++
			continue
		}
		if c == '/' && !inClass {
			inner := lx.src[off:i]
			if len(inner) == 0 {
				return token.Token{}, false
			}
			// Consume opening '/' through closing '/' (no newlines inside).
			for lx.off <= i {
				lx.advance()
			}
			return lx.emit(token.Regex, inner, start), true
		}
		i++
	}
	return token.Token{}, false
}

func (lx *Lexer) scanSelector(start token.Pos) token.Token {
	lx.advance() // $
	lx.advance() // "
	var sb strings.Builder
	for {
		if lx.off >= len(lx.src) {
			lx.errAt(start, "unterminated selector literal")
			return lx.emit(token.Selector, sb.String(), start)
		}
		c := lx.peek(0)
		if c == '"' {
			lx.advance()
			return lx.emit(token.Selector, sb.String(), start)
		}
		if c == '\n' {
			lx.errAt(start, "unterminated selector literal (newline in selector)")
			return lx.emit(token.Selector, sb.String(), start)
		}
		if c >= 0x80 && !lx.checkUTF8() {
			lx.errAt(lx.pos(), "invalid UTF-8 encoding in selector literal")
			lx.advance()
			continue
		}
		sb.WriteByte(lx.advance())
	}
}

// scanTag lexes :Ident or :"arbitrary text".
func (lx *Lexer) scanTag(start token.Pos) token.Token {
	lx.advance() // ':'
	if lx.peek(0) == '"' {
		lx.advance()
		var sb strings.Builder
		for {
			if lx.off >= len(lx.src) {
				lx.errAt(start, "unterminated tag literal")
				return lx.emit(token.Tag, sb.String(), start)
			}
			c := lx.peek(0)
			if c == '"' {
				lx.advance()
				return lx.emit(token.Tag, sb.String(), start)
			}
			if c == '\n' {
				lx.errAt(start, "unterminated tag literal (newline in tag)")
				return lx.emit(token.Tag, sb.String(), start)
			}
			if c >= 0x80 && !lx.checkUTF8() {
				lx.errAt(lx.pos(), "invalid UTF-8 encoding in tag literal")
				lx.advance()
				continue
			}
			sb.WriteByte(lx.advance())
		}
	}
	c := lx.peek(0)
	if c == ' ' || c == '\t' || c == '\r' || c == '\n' {
		return lx.emit(token.Colon, "", start)
	}
	if !isASCIIIdentStart(c) {
		lx.errAt(start, "expected tag name after ':'")
		return lx.emit(token.Tag, "", start)
	}
	var sb strings.Builder
	for lx.off < len(lx.src) && isASCIIIdentCont(lx.peek(0)) {
		sb.WriteByte(lx.advance())
	}
	return lx.emit(token.Tag, sb.String(), start)
}

// scanNumber lexes integer and float literals, including 0x/0b/0o bases
// and underscore separators (Section 3.6).
func (lx *Lexer) scanNumber(start token.Pos) token.Token {
	var sb strings.Builder
	isFloat := false
	if lx.peek(0) == '0' && (lx.peek(1) == 'x' || lx.peek(1) == 'b' || lx.peek(1) == 'o') {
		sb.WriteByte(lx.advance())
		sb.WriteByte(lx.advance())
		valid := map[byte]func(byte) bool{'x': isHexDigit, 'b': func(c byte) bool { return c == '0' || c == '1' },
			'o': func(c byte) bool { return c >= '0' && c <= '7' }}[sb.String()[1]]
		digits := 0
		for lx.off < len(lx.src) {
			c := lx.peek(0)
			if c == '_' {
				lx.advance()
				continue
			}
			if !valid(c) {
				break
			}
			sb.WriteByte(lx.advance())
			digits++
		}
		if digits == 0 {
			lx.errAt(start, "malformed based integer literal")
		}
		return lx.emit(token.Int, sb.String(), start)
	}
	for lx.off < len(lx.src) && (isDigit(lx.peek(0)) || lx.peek(0) == '_') {
		c := lx.advance()
		if c != '_' {
			sb.WriteByte(c)
		}
	}
	if lx.peek(0) == '.' && isDigit(lx.peek(1)) {
		isFloat = true
		sb.WriteByte(lx.advance())
		for lx.off < len(lx.src) && (isDigit(lx.peek(0)) || lx.peek(0) == '_') {
			c := lx.advance()
			if c != '_' {
				sb.WriteByte(c)
			}
		}
	}
	if e := lx.peek(0); e == 'e' || e == 'E' {
		n1, n2 := lx.peek(1), lx.peek(2)
		if isDigit(n1) || ((n1 == '+' || n1 == '-') && isDigit(n2)) {
			isFloat = true
			sb.WriteByte(lx.advance())
			if n1 == '+' || n1 == '-' {
				sb.WriteByte(lx.advance())
			}
			for lx.off < len(lx.src) && isDigit(lx.peek(0)) {
				sb.WriteByte(lx.advance())
			}
		}
	}
	if isFloat {
		return lx.emit(token.Float, sb.String(), start)
	}
	return lx.emit(token.Int, sb.String(), start)
}

// -------------------------------------------------------------- markup mode

func (lx *Lexer) nextMarkup() token.Token {
	m := lx.cur()
	if m.kind == modeElemHead {
		return lx.nextElemHead()
	}
	return lx.nextMarkupBody()
}

// nextElemHead scans inside <name attr=... until > or />.
func (lx *Lexer) nextElemHead() token.Token {
	for lx.off < len(lx.src) {
		c := lx.peek(0)
		if c == ' ' || c == '\t' || c == '\n' || c == '\r' {
			lx.advance()
			continue
		}
		break
	}
	start := lx.pos()
	if lx.off >= len(lx.src) {
		lx.errAt(start, "unexpected end of file inside element tag")
		return lx.emit(token.EOF, "", start)
	}
	c := lx.peek(0)
	switch {
	case c == '>':
		lx.advance()
		lx.modes[len(lx.modes)-1].kind = modeElemBody
		return lx.emit(token.MGT, "", start)
	case c == '/' && lx.peek(1) == '>':
		lx.advance()
		lx.advance()
		lx.modes = lx.modes[:len(lx.modes)-1] // pop the element head
		return lx.emit(token.MSelfClose, "", start)
	case c == '{':
		return lx.openSplice(start, token.MSpliceOpen)
	case c == '=':
		lx.advance()
		return lx.emit(token.MEq, "", start)
	case c == '"':
		lx.advance()
		var sb strings.Builder
		for {
			if lx.off >= len(lx.src) {
				lx.errAt(start, "unterminated attribute value")
				return lx.emit(token.MString, sb.String(), start)
			}
			if lx.peek(0) == '"' {
				lx.advance()
				return lx.emit(token.MString, sb.String(), start)
			}
			if lx.peek(0) >= 0x80 && !lx.checkUTF8() {
				lx.errAt(lx.pos(), "invalid UTF-8 encoding in attribute value")
				lx.advance()
				continue
			}
			sb.WriteByte(lx.advance())
		}
	case isMarkupNameStart(c):
		var sb strings.Builder
		for lx.off < len(lx.src) && isMarkupNameCont(lx.peek(0)) {
			sb.WriteByte(lx.advance())
		}
		return lx.emit(token.MAttrName, sb.String(), start)
	default:
		lx.errAt(start, "invalid character %q in element tag", string(rune(c)))
		lx.advance()
		return lx.next()
	}
}

// openSplice handles "{" (and "{..") in markup: emits the splice-open
// token and pushes expression mode for the spliced expression.
func (lx *Lexer) openSplice(start token.Pos, def token.Kind) token.Token {
	lx.advance() // {
	if lx.peek(0) == '.' && lx.peek(1) == '.' {
		lx.advance()
		lx.advance()
		lx.modes = append(lx.modes, mode{kind: modeExpr, fromSplice: true})
		return lx.emit(token.MSpreadOpen, "", start)
	}
	lx.modes = append(lx.modes, mode{kind: modeExpr, fromSplice: true})
	return lx.emit(def, "", start)
}

// nextMarkupBody scans a fence body or element body: text runs, child
// elements, comments, splices, and the closing %> (fence only).
func (lx *Lexer) nextMarkupBody() token.Token {
	if lx.off >= len(lx.src) {
		p := lx.pos()
		lx.errAt(p, "unexpected end of file inside node literal (missing fence end)")
		return lx.emit(token.EOF, "", p)
	}
	start := lx.pos()
	c := lx.peek(0)

	// At the very start of a fence body, a pending '<' is either fused
	// with the first element (fall through to the '<' dispatch below) or
	// elided (fragment body starting with a splice or text).
	if lx.cur().fenceFresh {
		lx.modes[len(lx.modes)-1].fenceFresh = false
		if c == '<' {
			if n := lx.peek(1); !isMarkupNameStart(n) && n != '!' && n != '/' {
				lx.advance()
				return lx.next()
			}
		}
	}

	if c == '<' {
		switch {
		case lx.peek(1) == '!' && lx.peek(2) == '-' && lx.peek(3) == '-':
			lx.advance()
			lx.advance()
			lx.advance()
			lx.advance()
			var sb strings.Builder
			for {
				if lx.off >= len(lx.src) {
					lx.errAt(start, "unterminated markup comment")
					return lx.emit(token.MComment, sb.String(), start)
				}
				if lx.peek(0) == '-' && lx.peek(1) == '-' && lx.peek(2) == '>' {
					lx.advance()
					lx.advance()
					lx.advance()
					return lx.emit(token.MComment, sb.String(), start)
				}
				if lx.peek(0) >= 0x80 && !lx.checkUTF8() {
					lx.errAt(lx.pos(), "invalid UTF-8 encoding in markup comment")
					lx.advance()
					continue
				}
				sb.WriteByte(lx.advance())
			}
		case lx.peek(1) == '/':
			if !isMarkupNameStart(lx.peek(2)) {
				lx.errAt(start, "expected element name after '</'")
				lx.advance()
				lx.advance()
				return lx.next()
			}
			lx.advance()
			lx.advance()
			var sb strings.Builder
			for lx.off < len(lx.src) && isMarkupNameCont(lx.peek(0)) {
				sb.WriteByte(lx.advance())
			}
			// Pop the element body this close-tag ends.
			if lx.modes[len(lx.modes)-1].kind == modeElemBody {
				lx.modes = lx.modes[:len(lx.modes)-1]
			}
			// Skip optional whitespace, then consume the closing '>'.
			for lx.off < len(lx.src) {
				w := lx.peek(0)
				if w == ' ' || w == '\t' || w == '\n' || w == '\r' {
					lx.advance()
					continue
				}
				break
			}
			if lx.peek(0) != '>' {
				lx.errAt(start, "expected '>' after closing tag name %q", sb.String())
			} else {
				lx.advance()
			}
			return lx.emit(token.MCloseName, sb.String(), start)
		case isMarkupNameStart(lx.peek(1)):
			lx.advance() // <
			var sb strings.Builder
			for lx.off < len(lx.src) && isMarkupNameCont(lx.peek(0)) {
				sb.WriteByte(lx.advance())
			}
			lx.modes = append(lx.modes, mode{kind: modeElemHead})
			return lx.emit(token.MOpenName, sb.String(), start)
		default:
			lx.errAt(start, "invalid character after '<' in markup")
			lx.advance()
			return lx.next()
		}
	}

	if c == '{' {
		return lx.openSplice(start, token.MSpliceOpen)
	}

	if c == '%' && lx.peek(1) == '>' {
		if lx.cur().kind == modeFenceBody {
			lx.advance()
			lx.advance()
			lx.modes = lx.modes[:len(lx.modes)-1]
			return lx.emit(token.NodeClose, "", start)
		}
		lx.errAt(start, "unexpected fence end: not at the top level of a node literal")
		lx.advance()
		lx.advance()
		return lx.next()
	}

	// Text run: verbatim until '<', '{', or the '%>' fence end.
	var sb strings.Builder
	for lx.off < len(lx.src) {
		c := lx.peek(0)
		if c == '<' || c == '{' {
			break
		}
		if c == '%' && lx.peek(1) == '>' && lx.cur().kind == modeFenceBody {
			break
		}
		if c >= 0x80 {
			if !lx.checkUTF8() {
				lx.errAt(lx.pos(), "invalid UTF-8 encoding in markup text")
				lx.advance()
				continue
			}
			_, size := utf8.DecodeRuneInString(lx.src[lx.off:])
			sb.WriteString(lx.src[lx.off : lx.off+size])
			for i := 0; i < size; i++ {
				lx.advance()
			}
			continue
		}
		sb.WriteByte(lx.advance())
	}
	if sb.Len() == 0 {
		// The run broke immediately on '<', '{', or the fence end; let the
		// dispatcher above consume the delimiter (and avoid empty tokens).
		return lx.next()
	}
	return lx.emit(token.MText, sb.String(), start)
}
