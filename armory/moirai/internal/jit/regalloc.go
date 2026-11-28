package jit

import "domscript/internal/ir"

// Location describes where a virtual register lives.
type Location struct {
	Register int
	Spill    int
}

// LiveRange is the interval [Start, End) during which a virtual register is live.
// Uses are the instruction indices where the value is read.
type LiveRange struct {
	VReg     int
	Start    int
	End      int
	Uses     []int
	Spilled  bool
}

// Allocator performs linear-scan register allocation over a trace.
type Allocator struct {
	// Physical register count (caller-saved subset).
	Width int

	// Spill slot counter.
	spills int

	// Active intervals sorted by increasing end point.
	active []*LiveRange

	// Map from virtual register to allocated location.
	loc map[int]Location

	// Live ranges for all virtual registers in the trace.
	ranges []*LiveRange
}

// NewAllocator creates a register allocator with the given number of physical registers.
func NewAllocator(registers int) *Allocator {
	if registers < 0 {
		registers = 0
	}
	return &Allocator{
		Width: registers,
		loc:   make(map[int]Location),
	}
}

// ComputeLiveRanges computes live ranges for all virtual registers in the given instructions.
// Virtual registers are identified by the A, B, C operands of instructions.
func (a *Allocator) ComputeLiveRanges(code []ir.Instr) {
	// Find max virtual register.
	maxVReg := -1
	for _, in := range code {
		for _, v := range []int64{in.A, in.B, in.C} {
			if v > int64(maxVReg) {
				maxVReg = int(v)
			}
		}
	}
	if maxVReg < 0 {
		return
	}

	// Initialize ranges: each virtual register starts at its first def, ends at its last use.
	ranges := make([]*LiveRange, maxVReg+1)
	for i := range ranges {
		ranges[i] = &LiveRange{VReg: i, Start: -1, End: -1}
	}

	for ip, in := range code {
		// Track uses and defs. For simplicity, treat A as def, B/C as use for most ops.
		// A more precise analysis would track def/use per op, but this is a good start.
		defs, uses := defUse(in)
		for _, u := range uses {
			if u >= 0 && u < len(ranges) {
				if ranges[u].Start < 0 {
					ranges[u].Start = ip
				}
				ranges[u].End = ip + 1
				ranges[u].Uses = append(ranges[u].Uses, ip)
			}
		}
		for _, d := range defs {
			if d >= 0 && d < len(ranges) {
				if ranges[d].Start < 0 {
					ranges[d].Start = ip
				}
				if ranges[d].End < ip+1 {
					ranges[d].End = ip + 1
				}
			}
		}
	}

	// Filter out unused virtual registers.
	a.ranges = make([]*LiveRange, 0, len(ranges))
	for _, lr := range ranges {
		if lr.Start >= 0 {
			a.ranges = append(a.ranges, lr)
		}
	}
}

// defUse returns the def and use virtual registers for an instruction.
// Returns (defs, uses) where each is a slice of virtual register indices.
func defUse(in ir.Instr) (defs, uses []int) {
	switch in.Op {
	case ir.OpConst, ir.OpLoad, ir.OpDup, ir.OpNeg, ir.OpNot, ir.OpSome,
		ir.OpMakeList, ir.OpMakeTagged, ir.OpNewTable, ir.OpMakeSelector,
		ir.OpMakeRegex, ir.OpInterp, ir.OpRange, ir.OpMakeFn, ir.OpEffectBegin,
		ir.OpNodeEl, ir.OpNodeText, ir.OpNodeComment, ir.OpNodeFragment,
		ir.OpEffectEnd, ir.OpEffectCatch, ir.OpQuery:
		// These produce a value on the stack. Virtual register = A or B.
		defs = append(defs, int(in.A))
	case ir.OpArith, ir.OpEq, ir.OpNe, ir.OpCmp, ir.OpTableSet, ir.OpIndex,
		ir.OpSetIndex, ir.OpField, ir.OpSetField, ir.OpForInit, ir.OpForNext,
		ir.OpNodeSplice, ir.OpNodeAttr, ir.OpSubscribe, ir.OpNodeSpread:
		// These consume stack values and may produce. Def=A, Uses=B,C.
		defs = append(defs, int(in.A))
		uses = append(uses, int(in.B), int(in.C))
	case ir.OpStore, ir.OpPop, ir.OpJumpFalse, ir.OpJumpTrue, ir.OpCall,
		ir.OpReturn, ir.OpMatch, ir.OpPopIter, ir.OpNodeEnd, ir.OpRaise:
		// These consume stack values.
		uses = append(uses, int(in.A), int(in.B), int(in.C))
	case ir.OpJump, ir.OpLine, ir.OpTestBool:
		// No virtual register usage.
	}
	// Filter out zero/negative (unused operands).
	defs = filterNonNeg(defs)
	uses = filterNonNeg(uses)
	return
}

func filterNonNeg(v []int) []int {
	out := make([]int, 0, len(v))
	for _, x := range v {
		if x > 0 {
			out = append(out, x)
		}
	}
	return out
}

// Allocate performs linear-scan register allocation.
func (a *Allocator) Allocate(virtual int) Location {
	if loc, ok := a.loc[virtual]; ok {
		return loc
	}
	// Default: spill to a new slot.
	s := a.spills
	a.spills++
	loc := Location{Register: -1, Spill: s}
	a.loc[virtual] = loc
	return loc
}

// AllocateAll runs linear scan over all computed live ranges.
func (a *Allocator) AllocateAll() {
	if len(a.ranges) == 0 {
		return
	}

	// Sort ranges by start point.
	sorted := make([]*LiveRange, len(a.ranges))
	copy(sorted, a.ranges)
	sortRanges(sorted)

	a.active = nil
	for _, lr := range sorted {
		// Expire old intervals.
		a.expireOld(lr.Start)

		if len(a.active) < a.Width {
			// Assign a free register.
			reg := a.findFreeReg()
			a.loc[lr.VReg] = Location{Register: reg, Spill: -1}
			a.active = append(a.active, lr)
		} else {
			// Spill the interval whose end is farthest.
			a.spillAt(lr)
		}
	}
}

// expireOld removes intervals that end before position.
func (a *Allocator) expireOld(pos int) {
	n := 0
	for _, lr := range a.active {
		if lr.End > pos {
			a.active[n] = lr
			n++
		}
	}
	a.active = a.active[:n]
}

// findFreeReg returns the first unassigned physical register.
func (a *Allocator) findFreeReg() int {
	used := make([]bool, a.Width)
	for _, lr := range a.active {
		if loc, ok := a.loc[lr.VReg]; ok && loc.Register >= 0 {
			used[loc.Register] = true
		}
	}
	for i := 0; i < a.Width; i++ {
		if !used[i] {
			return i
		}
	}
	return -1
}

// spillAt spills the interval with the farthest end among active intervals + the current one.
func (a *Allocator) spillAt(cur *LiveRange) {
	// Find the interval with the farthest end.
	farthest := -1
	farthestIdx := -1
	for i, lr := range a.active {
		if lr.End > farthest {
			farthest = lr.End
			farthestIdx = i
		}
	}

	if farthest > cur.End {
		// Spill the farthest interval, take its register.
		spilled := a.active[farthestIdx]
		reg := a.loc[spilled.VReg].Register
		spilled.Spilled = true
		a.loc[spilled.VReg] = Location{Register: -1, Spill: a.spills}
		a.spills++

		// Remove spilled from active, add current.
		a.active = append(a.active[:farthestIdx], a.active[farthestIdx+1:]...)
		a.loc[cur.VReg] = Location{Register: reg, Spill: -1}
		a.active = append(a.active, cur)
	} else {
		// Spill current.
		cur.Spilled = true
		a.loc[cur.VReg] = Location{Register: -1, Spill: a.spills}
		a.spills++
	}
}

// SpillCount returns the number of spill slots used.
func (a *Allocator) SpillCount() int { return a.spills }

// LocationOf returns the assigned location for a virtual register.
func (a *Allocator) LocationOf(virtual int) (Location, bool) {
	loc, ok := a.loc[virtual]
	return loc, ok
}

// IsSpilled returns whether a virtual register is spilled.
func (a *Allocator) IsSpilled(virtual int) bool {
	if lr := a.liveRange(virtual); lr != nil {
		return lr.Spilled
	}
	return false
}

func (a *Allocator) liveRange(virtual int) *LiveRange {
	for _, lr := range a.ranges {
		if lr.VReg == virtual {
			return lr
		}
	}
	return nil
}

// sortRanges sorts live ranges by start position (stable by length for ties).
func sortRanges(ranges []*LiveRange) {
	for i := 1; i < len(ranges); i++ {
		j := i
		for j > 0 && ranges[j].Start < ranges[j-1].Start {
			ranges[j], ranges[j-1] = ranges[j-1], ranges[j]
			j--
		}
	}
}
