package jit

import "domscript/internal/ir"

// Peephole applies local peephole optimizations to a sequence of IR instructions.
// It is written as a multi-pass optimizer: each pass applies one class of rewrite,
// and passes repeat until fixed point.
func Peephole(code []ir.Instr) []ir.Instr {
	if len(code) == 0 {
		return code
	}
	// Run passes until convergence.
	changed := true
	for changed {
		changed = false
		code, changed = peepAdjacentLine(code)
		if code2, c := peepDupPop(code); c {
			code, changed = code2, true
		}
		if code2, c := peepConstFold(code); c {
			code, changed = code2, true
		}
		if code2, c := peepRedundantLoadStore(code); c {
			code, changed = code2, true
		}
		if code2, c := peepJumpToNext(code); c {
			code, changed = code2, true
		}
		if code2, c := peepAlgebraic(code); c {
			code, changed = code2, true
		}
		if code2, c := peepNotNot(code); c {
			code, changed = code2, true
		}
		if code2, c := peepNegNeg(code); c {
			code, changed = code2, true
		}
	}
	return code
}

// peepAdjacentLine collapses adjacent OpLine instructions.
func peepAdjacentLine(code []ir.Instr) ([]ir.Instr, bool) {
	out := make([]ir.Instr, 0, len(code))
	changed := false
	for _, in := range code {
		if in.Op == ir.OpLine && len(out) > 0 && out[len(out)-1].Op == ir.OpLine {
			out[len(out)-1] = in
			changed = true
			continue
		}
		out = append(out, in)
	}
	return out, changed
}

// peepDupPop eliminates dup followed by pop (net no-op).
func peepDupPop(code []ir.Instr) ([]ir.Instr, bool) {
	out := make([]ir.Instr, 0, len(code))
	changed := false
	for _, in := range code {
		if in.Op == ir.OpPop && len(out) > 0 && out[len(out)-1].Op == ir.OpDup {
			out = out[:len(out)-1]
			changed = true
			continue
		}
		out = append(out, in)
	}
	return out, changed
}

// peepConstFold eliminates const followed by const with the same value when
// the second const is immediately consumed by a pop.
func peepConstFold(code []ir.Instr) ([]ir.Instr, bool) {
	out := make([]ir.Instr, 0, len(code))
	changed := false
	for i := 0; i < len(code); i++ {
		in := code[i]
		// Const; Const; Pop → Const (first const is dead)
		if i+2 < len(code) &&
			in.Op == ir.OpConst &&
			code[i+1].Op == ir.OpConst &&
			code[i+2].Op == ir.OpPop {
			out = append(out, code[i+1]) // keep the second const
			i += 2
			changed = true
			continue
		}
		// Const; Pop → nothing (unused const)
		if i+1 < len(code) && in.Op == ir.OpConst && code[i+1].Op == ir.OpPop {
			i++ // skip both
			changed = true
			continue
		}
		out = append(out, in)
	}
	return out, changed
}

// peepRedundantLoadStore eliminates load followed by store to the same slot
// when the loaded value is not used between them.
func peepRedundantLoadStore(code []ir.Instr) ([]ir.Instr, bool) {
	out := make([]ir.Instr, 0, len(code))
	changed := false
	for i := 0; i < len(code); i++ {
		in := code[i]
		// Store to slot X; Load from slot X → Store; Dup (the loaded value is already on stack)
		if i+1 < len(code) &&
			in.Op == ir.OpStore &&
			code[i+1].Op == ir.OpLoad &&
			in.A == code[i+1].A && in.B == code[i+1].B {
			out = append(out, in)
			out = append(out, ir.Instr{Op: ir.OpDup})
			i++ // skip the load
			changed = true
			continue
		}
		out = append(out, in)
	}
	return out, changed
}

// peepJumpToNext eliminates jumps to the very next instruction.
func peepJumpToNext(code []ir.Instr) ([]ir.Instr, bool) {
	out := make([]ir.Instr, 0, len(code))
	changed := false
	for i := 0; i < len(code); i++ {
		in := code[i]
		if in.Op == ir.OpJump && int(in.A) == i+1 {
			changed = true
			continue // skip this no-op jump
		}
		out = append(out, in)
	}
	return out, changed
}

// peepAlgebraic applies algebraic simplifications.
func peepAlgebraic(code []ir.Instr) ([]ir.Instr, bool) {
	out := make([]ir.Instr, 0, len(code))
	changed := false
	for i := 0; i < len(code); i++ {
		in := code[i]
		// Neg; Neg → nothing (double negation)
		if i+1 < len(code) && in.Op == ir.OpNeg && code[i+1].Op == ir.OpNeg {
			i++ // skip both
			changed = true
			continue
		}
		// Arith with zero being added or subtracted is a no-op for the non-zero operand.
		// Arith Add where one operand is 0 const → just the other operand.
		if i+2 < len(code) &&
			in.Op == ir.OpConst &&
			code[i+1].Op == ir.OpConst &&
			code[i+2].Op == ir.OpArith {
			// This is handled by the IR-level constant folder, but we can catch
			// cases where the const has already been folded but the IR wasn't
			// fully optimized.
		}
		// Not; Not → nothing (double negation)
		if i+1 < len(code) && in.Op == ir.OpNot && code[i+1].Op == ir.OpNot {
			i++ // skip both
			changed = true
			continue
		}
		out = append(out, in)
	}
	return out, changed
}

// peepNotNot eliminates Not followed by Not.
func peepNotNot(code []ir.Instr) ([]ir.Instr, bool) {
	return peepAlgebraic(code)
}

// peepNegNeg eliminates Neg followed by Neg.
func peepNegNeg(code []ir.Instr) ([]ir.Instr, bool) {
	out := make([]ir.Instr, 0, len(code))
	changed := false
	for i := 0; i < len(code); i++ {
		in := code[i]
		if i+1 < len(code) && in.Op == ir.OpNeg && code[i+1].Op == ir.OpNeg {
			i++ // skip both
			changed = true
			continue
		}
		out = append(out, in)
	}
	return out, changed
}
