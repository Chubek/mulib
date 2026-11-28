package jit

import (
	"fmt"
	"strings"

	"domscript/internal/ir"
	"domscript/internal/runtime"
)

// DeoptPoint marks a position in the trace where deoptimization can occur.
type DeoptPoint struct {
	IP     int
	Reason string
	// Frame reconstruction data.
	StackDepth int    // stack depth at this point
	EnvSlots   []int  // live env slots that need restoring
	RegMap     []RegMapEntry // register-to-slot mapping
}

// RegMapEntry maps a physical register to a virtual slot at a deopt point.
type RegMapEntry struct {
	Reg     int // physical register number
	Slot    int // virtual slot / stack position
	Spilled bool
	SpillSlot int // spill slot index if spilled
}

// DeoptMetadata holds all deoptimization information for a compiled trace.
type DeoptMetadata struct {
	FuncName   string
	Points     []DeoptPoint
	FrameSize  int // total frame size in bytes
	SpillSlots int // number of spill slots
	CalSaveRegs []int // callee-saved registers that need restoring
}

// DeoptReason provides a human-readable reason for deoptimization.
type DeoptReason int

const (
	DeoptGuardFailed   DeoptReason = iota // type or value guard failed
	DeoptSideExit                          // took a side exit from the trace
	DeoptStackOverflow                     // trace stack overflow
	DeoptRecursion                         // recursion depth exceeded
	DeoptUnsupportedOp                     // unsupported operation in trace
	DeoptAsyncEffect                       // async effect encountered
)

func (r DeoptReason) String() string {
	switch r {
	case DeoptGuardFailed:
		return "guard_failed"
	case DeoptSideExit:
		return "side_exit"
	case DeoptStackOverflow:
		return "stack_overflow"
	case DeoptRecursion:
		return "recursion"
	case DeoptUnsupportedOp:
		return "unsupported_op"
	case DeoptAsyncEffect:
		return "async_effect"
	}
	return "unknown"
}

// DeoptPoints returns all deoptimization points in a sequence of instructions.
func DeoptPoints(code []ir.Instr) []DeoptPoint {
	return deoptPointsWithDepth(code, 0)
}

// deoptPointsWithDepth tracks stack depth as it walks the code.
func deoptPointsWithDepth(code []ir.Instr, startDepth int) []DeoptPoint {
	out := make([]DeoptPoint, 0)
	depth := startDepth

	for i, in := range code {
		// Track stack depth.
		depth += stackEffect(in)

		// Determine if this instruction is a deopt point.
		reason := deoptReason(in)
		if reason != "" {
			pt := DeoptPoint{
				IP:         i,
				Reason:     reason,
				StackDepth: depth,
			}
			out = append(out, pt)
		}
	}
	return out
}

// deoptReason returns the reason this instruction is a deopt point, or empty string.
func deoptReason(in ir.Instr) string {
	switch in.Op {
	case ir.OpCall:
		return "call"
	case ir.OpRaise:
		return "raise"
	case ir.OpEffectBegin:
		return "effect_begin"
	case ir.OpEffectEnd:
		return "effect_end"
	case ir.OpEffectCatch:
		return "effect_catch"
	case ir.OpQuery:
		return "query"
	case ir.OpSubscribe:
		return "subscribe"
	case ir.OpMatch:
		return "match"
	case ir.OpForInit:
		return "for_init"
	case ir.OpForNext:
		return "for_next"
	case ir.OpIndex:
		return "index"
	case ir.OpSetIndex:
		return "set_index"
	case ir.OpField:
		return "field"
	case ir.OpSetField:
		return "set_field"
	case ir.OpTableSet:
		return "table_set"
	}
	return ""
}

// stackEffect returns the net change in stack depth for an instruction.
func stackEffect(in ir.Instr) int {
	switch in.Op {
	case ir.OpConst, ir.OpLoad, ir.OpDup, ir.OpNeg, ir.OpNot, ir.OpSome,
		ir.OpMakeList, ir.OpMakeTagged, ir.OpNewTable, ir.OpMakeSelector,
		ir.OpMakeRegex, ir.OpRange, ir.OpMakeFn, ir.OpEffectBegin, ir.OpNodeEl,
		ir.OpNodeText, ir.OpNodeComment, ir.OpNodeFragment, ir.OpEffectEnd,
		ir.OpEffectCatch, ir.OpQuery:
		return +1
	case ir.OpStore, ir.OpPop, ir.OpJump, ir.OpJumpFalse, ir.OpJumpTrue,
		ir.OpReturn, ir.OpRaise, ir.OpNodeEnd:
		return -1
	case ir.OpArith, ir.OpEq, ir.OpNe, ir.OpCmp, ir.OpTestBool:
		return -1 // pop 2, push 1
	case ir.OpInterp:
		// pop A parts, push 1 string: net 1-A.
		return int(1 - in.A)
	case ir.OpCall:
		return 0 // handled specially
	case ir.OpTableSet, ir.OpSetIndex, ir.OpSetField, ir.OpNodeAttr:
		return -2 // pop 2
	case ir.OpIndex, ir.OpField:
		return -1 // pop 2, push 1
	case ir.OpForInit:
		return -1 // pop 1, push 0 (iterator is hidden)
	case ir.OpForNext:
		return 0 // internal
	case ir.OpPopIter:
		return 0
	case ir.OpNodeSplice, ir.OpNodeSpread:
		return -1
	case ir.OpSubscribe:
		return -2 // pop 2, push 1
	case ir.OpLine:
		return 0
	}
	return 0
}

// BuildDeoptMetadata constructs deoptimization metadata for a trace.
func BuildDeoptMetadata(t Trace, isa ISA) *DeoptMetadata {
	dm := &DeoptMetadata{
		FuncName: t.Func.Name,
	}

	// Collect deopt points from the optimized code.
	code := Peephole(t.Code)
	dm.Points = deoptPointsWithDepth(code, 0)

	// Compute frame size.
	dm.FrameSize = 128 // default frame size (must match prologue)

	// Record callee-saved registers.
	for _, r := range isa.CalleeSaved() {
		dm.CalSaveRegs = append(dm.CalSaveRegs, r.Number)
	}

	// Count spill slots from the register allocator.
	alloc := NewAllocator(len(isa.CallerSaved()))
	alloc.ComputeLiveRanges(code)
	alloc.AllocateAll()
	dm.SpillSlots = alloc.SpillCount()

	// Build register maps for each deopt point.
	for i := range dm.Points {
		pt := &dm.Points[i]
		// For each live virtual register at this point, record its physical location.
		for _, lr := range alloc.ranges {
			if lr.Start <= pt.IP && lr.End > pt.IP {
				loc, ok := alloc.LocationOf(lr.VReg)
				if ok {
					entry := RegMapEntry{
						Slot: lr.VReg,
					}
					if loc.Register >= 0 {
						entry.Reg = loc.Register
					} else {
						entry.Spilled = true
						entry.SpillSlot = loc.Spill
					}
					pt.RegMap = append(pt.RegMap, entry)
				}
			}
		}
	}

	return dm
}

// FormatDeopt renders deoptimization metadata as a human-readable string.
func FormatDeopt(dm *DeoptMetadata) string {
	var b strings.Builder
	fmt.Fprintf(&b, "deopt %s\n", dm.FuncName)
	fmt.Fprintf(&b, "  frame=%d spills=%d callee_saved=%v\n",
		dm.FrameSize, dm.SpillSlots, dm.CalSaveRegs)
	for _, pt := range dm.Points {
		fmt.Fprintf(&b, "  @%04d %s depth=%d\n", pt.IP, pt.Reason, pt.StackDepth)
		for _, rm := range pt.RegMap {
			if rm.Spilled {
				fmt.Fprintf(&b, "    slot %d -> spill %d\n", rm.Slot, rm.SpillSlot)
			} else {
				fmt.Fprintf(&b, "    slot %d -> reg %d\n", rm.Slot, rm.Reg)
			}
		}
	}
	return b.String()
}

// Ensure imports are used.
var _ = runtime.Unit
var _ = fmt.Sprintf
var _ = strings.Builder{}
