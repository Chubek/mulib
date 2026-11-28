package jit

import (
	"errors"
	"fmt"
	"regexp"
	"strconv"
	"strings"
)

// ISA represents a parsed machine instruction set architecture description.
type ISA struct {
	Arch      string
	WordSize  int
	AddrSize  int
	Endian    string
	Align     int
	Registers []Register
	RegClasses []RegClass
	Encodings  []Encoding
	Ops        []OpDef
	Flags      []FlagSet
	Aliases    map[string]Alias
}

// Register represents a single hardware register.
type Register struct {
	Name   string
	Width  int
	Number int
	Class  string // regclass name this register belongs to
}

// RegClass groups registers of a particular kind (GPR, FPR, etc.).
type RegClass struct {
	Name      string
	Registers []Register
}

// Encoding describes a machine instruction encoding format.
type Encoding struct {
	Name   string
	Fields []EncodingField
}

// EncodingField is one field in an encoding format.
type EncodingField struct {
	Name string
	Bits int
}

// OpDef describes one machine instruction pattern.
type OpDef struct {
	Name     string
	Class    string
	Encoding string
	Operands []Operand
	Syntax   string
}

// Operand is one operand of an op definition.
type Operand struct {
	Name string
	Kind string // "reg", "mem", "imm"
	Flag string // optional flag constraint
}

// FlagSet describes a set of CPU flags.
type FlagSet struct {
	Name string
	Flags []string
}

// Alias maps a register alias to a canonical register name.
type Alias struct {
	Canon string
	Width int
}

// ParseISA parses an ISA description from src.
func ParseISA(src string) (ISA, error) {
	var out ISA
	out.Aliases = make(map[string]Alias)

	// Strip comments.
	src = stripComments(src)

	// Arch header.
	head := regexp.MustCompile(`(?m)^\s*arch\s+([A-Za-z0-9_]+)\s*\{`)
	m := head.FindStringSubmatch(src)
	if len(m) == 0 {
		return out, fmt.Errorf("jit: missing arch declaration")
	}
	out.Arch = m[1]

	// Top-level attributes.
	out.WordSize = attrInt(src, "word_size", 0)
	out.AddrSize = attrInt(src, "addr_size", 0)
	out.Align = attrInt(src, "align", 0)
	out.Endian = attrStr(src, "endian")

	// Parse blocks.
	blocks := parseBlocks(src)
	for _, b := range blocks {
		switch b.kind {
		case "regclass":
			rc := parseRegClass(b)
			out.RegClasses = append(out.RegClasses, rc)
			out.Registers = append(out.Registers, rc.Registers...)
		case "encoding":
			out.Encodings = append(out.Encodings, parseEncoding(b))
		case "op":
			op := parseOpDef(b)
			out.Ops = append(out.Ops, op)
		case "flags":
			out.Flags = append(out.Flags, parseFlags(b))
		case "alias":
			parseAliases(b, out.Aliases)
		}
	}

	if out.WordSize == 0 || out.AddrSize == 0 || out.Endian == "" {
		return out, fmt.Errorf("jit: incomplete %s ISA", out.Arch)
	}

	// Parse standalone alias declarations (not inside blocks).
	parseStandaloneAliases(src, out.Aliases)

	return out, nil
}

// parseStandaloneAliases parses alias declarations that are not inside alias blocks.
func parseStandaloneAliases(src string, aliases map[string]Alias) {
	// Match: alias name = canonical;
	re := regexp.MustCompile(`(?m)^\s*alias\s+(\w+)\s*=\s*(\w+)\s*;`)
	for _, x := range re.FindAllStringSubmatch(src, -1) {
		aliases[x[1]] = Alias{Canon: x[2]}
	}
	// Match: alias name = number;
	re2 := regexp.MustCompile(`(?m)^\s*alias\s+(\w+)\s*=\s*(\d+)\s*;`)
	for _, x := range re2.FindAllStringSubmatch(src, -1) {
		w, _ := strconv.Atoi(x[2])
		aliases[x[1]] = Alias{Width: w}
	}
}

// attrInt extracts an integer attribute from the source.
func attrInt(src, key string, def int) int {
	re := regexp.MustCompile(key + `\s*=\s*(\d+)`)
	if x := re.FindStringSubmatch(src); len(x) > 0 {
		v, _ := strconv.Atoi(x[1])
		return v
	}
	return def
}

// attrStr extracts a string attribute from the source.
func attrStr(src, key string) string {
	re := regexp.MustCompile(key + `\s*=\s*([A-Za-z0-9_]+)`)
	if x := re.FindStringSubmatch(src); len(x) > 0 {
		return x[1]
	}
	return ""
}

// block represents a parsed block from the ISA source.
type block struct {
	kind string
	name string
	body string
}

// parseBlocks extracts all top-level blocks from the source.
func parseBlocks(src string) []block {
	var out []block
	// Match block headers: word { ... }
	re := regexp.MustCompile(`(?m)^\s*(regclass|encoding|op|flags|alias)\s+(\w+)\s*\{`)
	locs := re.FindAllStringSubmatchIndex(src, -1)
	for i, loc := range locs {
		kind := src[loc[2]:loc[3]]
		name := src[loc[4]:loc[5]]
		start := loc[1] // end of '{'
		end := len(src)
		if i+1 < len(locs) {
			end = locs[i+1][0]
		}
		// Find matching '}'.
		depth := 1
		close := start
		for j := start; j < end && depth > 0; j++ {
			if src[j] == '{' {
				depth++
			} else if src[j] == '}' {
				depth--
				if depth == 0 {
					close = j
				}
			}
		}
		body := src[start:close]
		out = append(out, block{kind: kind, name: name, body: body})
	}
	return out
}

// parseRegClass parses a regclass block body.
func parseRegClass(b block) RegClass {
	rc := RegClass{Name: b.name}
	// The body is the content inside {}. Registers are like: RAX(64)=0,
	re := regexp.MustCompile(`(\w+)\((\d+)\)\s*=\s*(\d+)`)
	for _, x := range re.FindAllStringSubmatch(b.body, -1) {
		w, _ := strconv.Atoi(x[2])
		n, _ := strconv.Atoi(x[3])
		rc.Registers = append(rc.Registers, Register{
			Name: x[1], Width: w, Number: n, Class: rc.Name,
		})
	}
	// Also handle the RV64 style: Name(Width) without =Number
	re2 := regexp.MustCompile(`(\w+)\((\d+)\)\s*,`)
	for i, x := range re2.FindAllStringSubmatch(b.body, -1) {
		w, _ := strconv.Atoi(x[2])
		rc.Registers = append(rc.Registers, Register{
			Name: x[1], Width: w, Number: i, Class: rc.Name,
		})
	}
	return rc
}

// parseEncoding parses an encoding block body.
func parseEncoding(b block) Encoding {
	e := Encoding{Name: b.name}
	// The body starts with the name; fields follow.
	lines := strings.Split(b.body, "\n")
	for _, ln := range lines {
		ln = strings.TrimSpace(ln)
		if ln == "" || strings.HasPrefix(ln, "//") {
			continue
		}
		parts := strings.SplitN(ln, ":", 2)
		if len(parts) != 2 {
			continue
		}
		name := strings.TrimSpace(parts[0])
		bitsStr := strings.TrimSpace(parts[1])
		bitsStr = strings.TrimSuffix(bitsStr, ";")
		bits, _ := strconv.Atoi(strings.TrimSpace(bitsStr))
		e.Fields = append(e.Fields, EncodingField{Name: name, Bits: bits})
	}
	return e
}

// parseOpDef parses an op block body.
func parseOpDef(b block) OpDef {
	o := OpDef{Name: b.name}
	// The body has name on the first line, then attributes.
	lines := strings.Split(b.body, "\n")
	for _, ln := range lines {
		ln = strings.TrimSpace(ln)
		if ln == "" || strings.HasPrefix(ln, "//") {
			continue
		}
		parts := strings.SplitN(ln, "=", 2)
		if len(parts) != 2 {
			continue
		}
		key := strings.TrimSpace(parts[0])
		val := strings.TrimSpace(strings.TrimSuffix(strings.TrimSpace(parts[1]), ";"))
		switch key {
		case "class":
			o.Class = val
		case "encoding":
			o.Encoding = val
		case "operands":
			o.Operands = parseOperands(val)
		case "syntax":
			o.Syntax = strings.Trim(val, `"`)
		}
	}
	return o
}

// parseOperands parses a comma-separated operand list.
func parseOperands(s string) []Operand {
	if s == "" {
		return nil
	}
	var out []Operand
	for _, part := range strings.Split(s, ",") {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		// format: name: kind(flag)
		op := Operand{}
		colon := strings.Index(part, ":")
		if colon >= 0 {
			op.Name = strings.TrimSpace(part[:colon])
			kindPart := strings.TrimSpace(part[colon+1:])
			// kind may be "reg" or "reg(GPR)" or "mem(GPR)"
			paren := strings.Index(kindPart, "(")
			if paren >= 0 {
				op.Kind = kindPart[:paren]
				op.Flag = strings.TrimRight(kindPart[paren+1:], ")")
			} else {
				op.Kind = kindPart
			}
		}
		out = append(out, op)
	}
	return out
}

// parseFlags parses a flags block body.
func parseFlags(b block) FlagSet {
	fs := FlagSet{Name: b.name}
	// Body: name { flags... }
	body := strings.TrimSpace(b.body)
	for _, f := range strings.Split(body, ",") {
		f = strings.TrimSpace(f)
		if f != "" {
			fs.Flags = append(fs.Flags, f)
		}
	}
	return fs
}

// parseAliases parses alias declarations from the block body.
func parseAliases(b block, aliases map[string]Alias) {
	if b.body == "" {
		return
	}
	// Alias blocks are a series of: alias name = canonical;
	re := regexp.MustCompile(`(\w+)\s*=\s*(\w+)\s*;`)
	for _, x := range re.FindAllStringSubmatch(b.body, -1) {
		aliases[x[1]] = Alias{Canon: x[2]}
	}
	// Also handle: alias name = number;
	re2 := regexp.MustCompile(`(\w+)\s*=\s*(\d+)\s*;`)
	for _, x := range re2.FindAllStringSubmatch(b.body, -1) {
		w, _ := strconv.Atoi(x[2])
		aliases[x[1]] = Alias{Width: w}
	}
}

// stripComments removes C-style comments from the source.
func stripComments(src string) string {
	// Remove // line comments.
	re := regexp.MustCompile(`//[^\n]*`)
	src = re.ReplaceAllString(src, "")
	// Remove /* block comments */.
	re2 := regexp.MustCompile(`/\*[\s\S]*?\*/`)
	src = re2.ReplaceAllString(src, "")
	return src
}

// Register looks up a register by name (case-insensitive).
func (i ISA) Register(name string) (Register, bool) {
	// Check aliases first.
	if a, ok := i.Aliases[name]; ok {
		if a.Canon != "" {
			name = a.Canon
		}
	}
	for _, r := range i.Registers {
		if strings.EqualFold(r.Name, name) {
			return r, true
		}
	}
	return Register{}, false
}

// Op looks up an op definition by name.
func (i ISA) Op(name string) (OpDef, bool) {
	for _, o := range i.Ops {
		if o.Name == name {
			return o, true
		}
	}
	return OpDef{}, false
}

// Encoding looks up an encoding by name.
func (i ISA) Encoding(name string) (Encoding, bool) {
	for _, e := range i.Encodings {
		if e.Name == name {
			return e, true
		}
	}
	return Encoding{}, false
}

// RegClass returns the register class with the given name.
func (i ISA) RegClass(name string) (RegClass, bool) {
	for _, rc := range i.RegClasses {
		if rc.Name == name {
			return rc, true
		}
	}
	return RegClass{}, false
}

// CallerSaved returns registers that are caller-saved (volatile across calls).
func (i ISA) CallerSaved() []Register {
	// Heuristic: registers in the first half of GPR and FPR are caller-saved.
	var out []Register
	for _, rc := range i.RegClasses {
		n := len(rc.Registers)
		limit := n / 2 // first half
		if limit > 8 {
			limit = 8 // common ABI: first 8 are caller-saved
		}
		for j := 0; j < limit && j < n; j++ {
			out = append(out, rc.Registers[j])
		}
	}
	return out
}

// CalleeSaved returns registers that are callee-saved (preserved across calls).
func (i ISA) CalleeSaved() []Register {
	var out []Register
	for _, rc := range i.RegClasses {
		n := len(rc.Registers)
		start := n / 2
		if start > 8 {
			start = 8
		}
		// Skip SP, FP, etc.
		for j := start; j < n; j++ {
			name := strings.ToUpper(rc.Registers[j].Name)
			if name == "SP" || name == "FP" || name == "RSP" || name == "RBP" ||
				name == "LR" || name == "RA" || name == "GP" || name == "TP" {
				continue
			}
			out = append(out, rc.Registers[j])
		}
	}
	return out
}

// FramePointer returns the frame pointer register, if any.
func (i ISA) FramePointer() (Register, bool) {
	for _, r := range i.Registers {
		name := strings.ToUpper(r.Name)
		if name == "RBP" || name == "FP" || name == "S0" {
			return r, true
		}
	}
	return Register{}, false
}

// StackPointer returns the stack pointer register.
func (i ISA) StackPointer() (Register, bool) {
	for _, r := range i.Registers {
		name := strings.ToUpper(r.Name)
		if name == "RSP" || name == "SP" {
			return r, true
		}
	}
	return Register{}, false
}

// ReturnReg returns the register used for return values.
func (i ISA) ReturnReg() (Register, bool) {
	for _, r := range i.Registers {
		name := strings.ToUpper(r.Name)
		if name == "RAX" || name == "R0" || name == "A0" {
			return r, true
		}
	}
	return Register{}, false
}

// Validate checks that the ISA is well-formed.
func (i ISA) Validate() error {
	if i.Arch == "" {
		return errors.New("jit: ISA missing arch")
	}
	if i.WordSize == 0 {
		return errors.New("jit: ISA missing word_size")
	}
	if i.AddrSize == 0 {
		return errors.New("jit: ISA missing addr_size")
	}
	if i.Endian == "" {
		return errors.New("jit: ISA missing endian")
	}
	if len(i.Registers) == 0 {
		return errors.New("jit: ISA has no registers")
	}
	return nil
}
