// Package ir: cfg.go builds the control-flow graph of a compiled Func.
// The baseline VM ignores it, but the rewrites below, the optimizer, and
// the JIT's trace-head detection all want basic blocks: maximal straight
// instruction runs with single entry (a leader) and single exit.
package ir

// Block is one basic block: Code[Start:End], with successors and
// predecessors by block index. Block -1 in Preds means "entry" (only
// block 0 has it).
type Block struct {
	ID    int
	Start int // inclusive
	End   int // exclusive
	Succs []int
	Preds []int
}

// Terminators are the ops that end a block.
func isTerminator(in Instr) bool {
	switch in.Op {
	case OpJump, OpJumpFalse, OpJumpTrue, OpReturn:
		return true
	}
	return false
}

// Leaders returns the instruction indices that start basic blocks.
func leaders(f *Func) map[int]bool {
	l := map[int]bool{0: true}
	for i, in := range f.Code {
		if isTerminator(in) {
			if i+1 < len(f.Code) {
				l[i+1] = true
			}
			switch in.Op {
			case OpJump:
				l[int(in.A)] = true
			case OpJumpFalse, OpJumpTrue:
				l[int(in.A)] = true
				if i+1 < len(f.Code) {
					l[i+1] = true
				}
			}
		}
	}
	// Match fail targets, effect catch targets, and loop ends are branch
	// destinations recorded in operand fields rather than as jumps.
	for _, in := range f.Code {
		switch in.Op {
		case OpMatch:
			l[int(in.B)] = true
		case OpEffectBegin:
			l[int(in.A)] = true
		case OpForNext:
			l[int(in.B)] = true
		}
	}
	return l
}

// Blocks computes the CFG of f.
func Blocks(f *Func) []*Block {
	if len(f.Code) == 0 {
		return nil
	}
	l := leaders(f)
	var starts []int
	for i := range f.Code {
		if l[i] {
			starts = append(starts, i)
		}
	}
	at := map[int]int{} // instr index -> block id
	blocks := make([]*Block, len(starts))
	for bi, s := range starts {
		e := len(f.Code)
		if bi+1 < len(starts) {
			e = starts[bi+1]
		}
		blocks[bi] = &Block{ID: bi, Start: s, End: e}
		for i := s; i < e; i++ {
			at[i] = bi
		}
	}
	for bi, b := range blocks {
		last := f.Code[b.End-1]
		switch last.Op {
		case OpJump:
			b.Succs = append(b.Succs, at[int(last.A)])
		case OpJumpFalse, OpJumpTrue:
			b.Succs = append(b.Succs, bi+1, at[int(last.A)])
		case OpReturn:
			// no successors
		default:
			// falls through to the next block if any
			if bi+1 < len(blocks) {
				b.Succs = append(b.Succs, bi+1)
			}
		}
	}
	for bi, b := range blocks {
		for _, s := range b.Succs {
			if s >= 0 && s < len(blocks) {
				blocks[s].Preds = append(blocks[s].Preds, bi)
			}
		}
	}
	blocks[0].Preds = append(blocks[0].Preds, -1)
	return blocks
}

// Validate checks internal consistency of f's code: jump targets in
// range, const/func/pattern pool indices in range. Used by the compile
// tests and by dsc check.
func Validate(f *Func) error {
	return validate(f, nil)
}

// validate checks slots against the frame selected by the instruction's
// environment depth. A nested function may legitimately access a slot in
// its parent activation, so checking every slot against the current
// function's NLocals rejects valid closure/global references.
func validate(f *Func, parents []*Func) error {
	n := int64(len(f.Code))
	for i, in := range f.Code {
		bad := func(what string) error {
			return &InvalidError{Func: f.Name, IP: i, What: what}
		}
		switch in.Op {
		case OpJump, OpJumpFalse, OpJumpTrue:
			if in.A < 0 || in.A > n {
				return bad("jump target out of range")
			}
		case OpConst, OpMakeSelector, OpMakeRegex, OpMakeTagged, OpNodeEl, OpNodeText,
			OpNodeComment, OpNodeAttr, OpSubscribe:
			if in.A < 0 || in.A >= int64(len(f.Consts)) {
				return bad("const index out of range")
			}
		case OpMakeFn:
			if in.A < 0 || in.A >= int64(len(f.Funcs)) {
				return bad("func index out of range")
			}
		case OpMatch:
			if in.A < 0 || in.A >= int64(len(f.Pats)) {
				return bad("pattern index out of range")
			}
			if in.B < 0 || in.B > n {
				return bad("match fail target out of range")
			}
		case OpEffectBegin:
			if in.A < 0 || in.A > n {
				return bad("effect catch target out of range")
			}
		case OpForNext:
			if in.B < 0 || in.B > n {
				return bad("loop end target out of range")
			}
		case OpLoad, OpStore:
			if in.A < 0 || in.B < 0 {
				return bad("env slot out of range")
			}
			target := f
			if int(in.A) > len(parents) {
				return bad("env depth out of range")
			}
			if in.A > 0 {
				target = parents[len(parents)-int(in.A)]
			}
			if in.B >= int64(target.NLocals) {
				return bad("env slot out of range")
			}
		}
	}
	for _, sub := range f.Funcs {
		nextParents := append(append([]*Func(nil), parents...), f)
		if err := validate(sub, nextParents); err != nil {
			return err
		}
	}
	return nil
}

// InvalidError reports a malformed Func (a compiler bug, not a user error).
type InvalidError struct {
	Func string
	IP   int
	What string
}

func (e *InvalidError) Error() string {
	return "ir: func " + e.Func + " instr " + itoa(e.IP) + ": " + e.What
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	var b [20]byte
	i := len(b)
	for n > 0 {
		i--
		b[i] = byte('0' + n%10)
		n /= 10
	}
	return string(b[i:])
}
