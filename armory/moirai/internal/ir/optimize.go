// Package ir: optimize.go — constant folding over the bytecode. Only
// provably side-effect-free shapes fold: two consecutive scalar consts
// consumed by an arithmetic, comparison, or logical op, and Not over a
// Bool const. Anything touching loads, calls, or effects is left for the
// VM/JIT. Optimize is a no-op semantic pass: a folded program and its
// unfolded source must produce identical observables.
package ir

import (
	"math"

	"domscript/internal/runtime"
)

// Optimize folds constants in f (recursively) after normalizing control
// flow, and returns f.
func Optimize(f *Func) *Func {
	Rewrite(f)
	fold(f)
	for _, sub := range f.Funcs {
		Optimize(sub)
	}
	return f
}

func fold(f *Func) {
	i := 0
	for i+2 < len(f.Code) || (i+1 < len(f.Code) && f.Code[i+1].Op == OpNot) {
		if isConstAt(f, i) && isConstAt(f, i+1) && i+2 < len(f.Code) {
			l, r := constAt(f, i), constAt(f, i+1)
			switch op := f.Code[i+2].Op; op {
			case OpArith:
				if v, ok := foldArith(f.Code[i+2].A, l, r); ok {
					replaceWithConst(f, i, v)
					continue
				}
			case OpEq:
				replaceWithConst(f, i, runtime.Bool(runtime.Equals(l, r)))
				continue
			case OpNe:
				replaceWithConst(f, i, runtime.Bool(!runtime.Equals(l, r)))
				continue
			case OpCmp:
				if v, ok := foldCmp(f.Code[i+2].A, l, r); ok {
					replaceWithConst(f, i, v)
					continue
				}
			}
		}
		if isConstAt(f, i) && f.Code[i+1].Op == OpNot {
			if v := constAt(f, i); v.Kind() == runtime.KindBool {
				replaceWithConst(f, i, runtime.Bool(!v.BoolValue()))
				continue
			}
		}
		i++
	}
}

func isConstAt(f *Func, i int) bool {
	return f.Code[i].Op == OpConst && f.Code[i].A >= 0 && f.Code[i].A < int64(len(f.Consts))
}

func constAt(f *Func, i int) runtime.Value { return f.Consts[f.Code[i].A] }

// replaceWithConst rewrites instrs [i, i+3) (or [i, i+2) for unary forms)
// into a single Const of v. Jump targets elsewhere in the function that
// pointed into the replaced range are impossible: those positions were
// mid-expression, and lowering only emits labels between statements.
func replaceWithConst(f *Func, i int, v runtime.Value) {
	idx := int64(len(f.Consts))
	f.Consts = append(f.Consts, v)
	end := i + 3
	if end > len(f.Code) {
		end = len(f.Code)
	}
	f.Code[i] = Instr{Op: OpConst, A: idx}
	f.Line[i] = f.Line[end-1]
	rest := append([]Instr{}, f.Code[end:]...)
	restLine := append([]int32{}, f.Line[end:]...)
	f.Code = append(f.Code[:i+1], rest...)
	f.Line = append(f.Line[:i+1], restLine...)
}

func foldArith(op int64, l, r runtime.Value) (runtime.Value, bool) {
	lk, rk := l.Kind(), r.Kind()
	num := (lk == runtime.KindInt || lk == runtime.KindFloat) &&
		(rk == runtime.KindInt || rk == runtime.KindFloat)
	if !num {
		return runtime.Unit(), false
	}
	switch op {
	case ArithAdd, ArithSub, ArithMul, ArithDiv, ArithMod:
	default:
		return runtime.Unit(), false
	}
	if lk == runtime.KindInt && rk == runtime.KindInt {
		a, b := l.IntValue(), r.IntValue()
		switch op {
		case ArithAdd:
			return runtime.Int(a + b), true
		case ArithSub:
			return runtime.Int(a - b), true
		case ArithMul:
			return runtime.Int(a * b), true
		case ArithDiv:
			if b == 0 {
				return runtime.Unit(), false
			}
			return runtime.Int(a / b), true
		case ArithMod:
			if b == 0 {
				return runtime.Unit(), false
			}
			return runtime.Int(a % b), true
		}
	}
	a, b := l.FloatValue(), r.FloatValue()
	switch op {
	case ArithAdd:
		return runtime.Float(a + b), true
	case ArithSub:
		return runtime.Float(a - b), true
	case ArithMul:
		return runtime.Float(a * b), true
	case ArithDiv:
		return runtime.Float(a / b), true
	case ArithMod:
		return runtime.Float(math.Mod(a, b)), true
	}
	return runtime.Unit(), false
}

func foldCmp(op int64, l, r runtime.Value) (runtime.Value, bool) {
	lk, rk := l.Kind(), r.Kind()
	ln := lk == runtime.KindInt || lk == runtime.KindFloat
	rn := rk == runtime.KindInt || rk == runtime.KindFloat
	if ln && rn {
		a, b := l.FloatValue(), r.FloatValue()
		return runtime.Bool(cmpOK(op, a < b, a <= b, a > b, a >= b)), true
	}
	if lk == runtime.KindString && rk == runtime.KindString {
		a, b := l.StrValue(), r.StrValue()
		return runtime.Bool(cmpOK(op, a < b, a <= b, a > b, a >= b)), true
	}
	return runtime.Unit(), false
}

func cmpOK(op int64, lt, le, gt, ge bool) bool {
	switch op {
	case 0:
		return lt
	case 1:
		return le
	case 2:
		return gt
	case 3:
		return ge
	}
	return false
}
