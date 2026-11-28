// Package ir: rewrite.go normalizes a Func after lowering. The rewrites
// are semantics-preserving control-flow cleanups the optimizer and the
// JIT both assume: jump chains collapse to their final destination, and
// unreachable instructions between a terminator and the next leader are
// dropped. Both run recursively over nested closures.
package ir

// Rewrite normalizes f in place and returns it.
func Rewrite(f *Func) *Func {
	threadJumps(f)
	removeDead(f)
	for _, sub := range f.Funcs {
		Rewrite(sub)
	}
	return f
}

// threadJumps retargets jumps whose destination is another unconditional
// jump, repeatedly, until fixed point.
func threadJumps(f *Func) {
	changed := true
	for changed {
		changed = false
		for i := range f.Code {
			in := &f.Code[i]
			switch in.Op {
			case OpJump:
				if t := finalJump(f, int(in.A)); t != int(in.A) {
					in.A = int64(t)
					changed = true
				}
			case OpJumpFalse, OpJumpTrue:
				if t := finalJump(f, int(in.A)); t != int(in.A) {
					in.A = int64(t)
					changed = true
				}
			}
		}
	}
}

// finalJump follows a chain of unconditional jumps starting at ip.
func finalJump(f *Func, ip int) int {
	for hops := 0; hops < len(f.Code); hops++ {
		if ip < 0 || ip >= len(f.Code) || f.Code[ip].Op != OpJump {
			return ip
		}
		ip = int(f.Code[ip].A)
	}
	return ip
}

// removeDead drops unreachable instructions: anything after a terminator
// up to the next leader (a jump target or another terminator's end).
func removeDead(f *Func) {
	l := leaders(f)
	out := make([]Instr, 0, len(f.Code))
	lines := make([]int32, 0, len(f.Code))
	// Map old index -> new index so jump targets stay valid.
	reloc := make([]int, len(f.Code))
	kept := make([]bool, len(f.Code))
	dead := false
	for i, in := range f.Code {
		if l[i] {
			dead = false // a leader is reachable by construction
		}
		if dead {
			continue
		}
		reloc[i] = len(out)
		kept[i] = true
		out = append(out, in)
		lines = append(lines, f.Line[i])
		if isTerminator(in) {
			dead = true
		}
	}
	f.Code = out
	f.Line = lines
	for i := range f.Code {
		in := &f.Code[i]
		switch in.Op {
		case OpJump, OpJumpFalse, OpJumpTrue:
			in.A = int64(reloc[in.A])
		case OpMatch:
			in.B = int64(reloc[in.B])
		case OpEffectBegin:
			in.A = int64(reloc[in.A])
		case OpForNext:
			in.B = int64(reloc[in.B])
		}
	}
	_ = kept
}
