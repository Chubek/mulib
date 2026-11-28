package jit

import (
	"domscript/internal/ir"
	"domscript/internal/runtime"
)

// TraceKind classifies a trace by its role in the trace tree.
type TraceKind int

const (
	TraceRoot TraceKind = iota // top-level entry trace
	TraceLoop                  // trace for a loop body
	TraceSide                  // trace for an alternate path
	TraceTail                  // trace that continues after a call
)

// GuardKind identifies what kind of assumption a guard checks.
type GuardKind int

const (
	GuardType    GuardKind = iota // value is of a specific runtime.Kind
	GuardTruthy                   // value is truthy (for branch prediction)
	GuardFalsy                    // value is falsy
	GuardConst                    // value equals a specific constant
	GuardNotNull                  // value is not nil/unit
	GuardRange                    // value is within a numeric range
)

// Guard records one assumption the trace makes about a runtime value.
type Guard struct {
	Kind     GuardKind
	IP       int           // instruction index in the original function
	Slot     int           // virtual register slot this guard checks
	Type     runtime.Kind  // for GuardType
	ConstVal runtime.Value // for GuardConst
	Lo, Hi   int64         // for GuardRange
}

// SideExit records a point where the trace can exit to the interpreter.
type SideExit struct {
	IP        int    // instruction index in the original function where exit occurs
	TargetIP  int    // where to resume in the interpreter
	GuardIdx  int    // index into Guards that guards this exit
	NextTrace *Trace // child trace that continues from this exit, if any
}

// Trace is a single compiled trace: a linear sequence of instructions
// with guards, side exits, and optional child traces.
type Trace struct {
	Func      *ir.Func
	Kind      TraceKind
	Entry     int
	Code      []ir.Instr
	IPs       []int      // original bytecode IP for each Code entry
	Exit      int        // last instruction index in the trace
	Guards    []Guard    // assumptions the trace makes
	SideExits []SideExit // points where the trace can bail out
	Children  []*Trace   // nested traces (loop bodies, side traces)
	Parent    *Trace     // parent trace in the tree
	LoopCount int        // estimated trip count for loop traces
	Depth     int        // nesting depth in the trace tree
	MaxDepth  int        // maximum allowed depth
}

// TraceConfig controls trace building behavior.
type TraceConfig struct {
	MaxInstrs     int  // maximum instructions per trace (default 256)
	MaxDepth      int  // maximum nesting depth (default 4)
	MaxLoopUnroll int  // maximum loop unroll factor (default 8)
	RecordGuards  bool // whether to insert type/value guards
}

// DefaultTraceConfig returns sensible defaults.
func DefaultTraceConfig() TraceConfig {
	return TraceConfig{
		MaxInstrs:     256,
		MaxDepth:      4,
		MaxLoopUnroll: 8,
		RecordGuards:  true,
	}
}

// BuildTrace builds a trace starting at entry in f, using the given config.
func BuildTrace(f *ir.Func, entry, limit int) Trace {
	return BuildTraceWithConfig(f, entry, DefaultTraceConfig())
}

// BuildTraceWithConfig builds a trace with explicit configuration.
func BuildTraceWithConfig(f *ir.Func, entry int, cfg TraceConfig) Trace {
	t := Trace{
		Func:     f,
		Kind:     TraceRoot,
		Entry:    entry,
		Exit:     entry,
		MaxDepth: cfg.MaxDepth,
	}
	if f == nil || entry < 0 || entry >= len(f.Code) || cfg.MaxInstrs <= 0 {
		return t
	}

	visited := make(map[int]int) // ip -> visit count, for loop detection
	t.buildLinear(f, entry, cfg, visited, 0)
	return t
}

// buildLinear traces forward from ip, building a linear trace.
func (t *Trace) buildLinear(f *ir.Func, ip int, cfg TraceConfig, visited map[int]int, depth int) {
	for n := 0; ip >= 0 && ip < len(f.Code) && n < cfg.MaxInstrs; n++ {
		// Loop detection: if we've visited this IP before, we're in a loop.
		if visited[ip] > 0 {
			if depth < cfg.MaxDepth {
				child := t.buildLoopTrace(f, ip, cfg, depth+1)
				if child != nil {
					t.Children = append(t.Children, child)
				}
			}
			// Record a side exit for the loop continuation.
			t.SideExits = append(t.SideExits, SideExit{
				IP:       len(t.Code) - 1,
				TargetIP: ip,
			})
			return
		}
		visited[ip]++

		in := f.Code[ip]
		t.Code = append(t.Code, in)
		t.IPs = append(t.IPs, ip)
		t.Exit = ip

		switch in.Op {
		case ir.OpJump:
			ip = int(in.A)
			continue

		case ir.OpLine:
			ip++
			continue

		case ir.OpJumpFalse:
			// Follow the fallthrough path; the jump target becomes the side exit.
			if cfg.RecordGuards {
				t.Guards = append(t.Guards, Guard{
					Kind: GuardTruthy,
					IP:   ip,
					Slot: int(in.A),
				})
			}
			t.SideExits = append(t.SideExits, SideExit{
				IP:       len(t.Code) - 1,
				TargetIP: int(in.A),
			})
			ip++
			continue

		case ir.OpJumpTrue:
			if cfg.RecordGuards {
				t.Guards = append(t.Guards, Guard{
					Kind: GuardFalsy,
					IP:   ip,
					Slot: int(in.A),
				})
			}
			t.SideExits = append(t.SideExits, SideExit{
				IP:       len(t.Code) - 1,
				TargetIP: int(in.A),
			})
			ip++
			continue

		case ir.OpCall:
			// Calls are trace boundaries — we stop here, but record a child trace for the
			// continuation after the call returns.
			if ip+1 < len(f.Code) && depth < cfg.MaxDepth {
				child := t.buildTailTrace(f, ip+1, cfg, depth+1)
				if child != nil {
					t.Children = append(t.Children, child)
				}
			}
			return

		case ir.OpReturn:
			return

		case ir.OpArith, ir.OpCmp, ir.OpEq, ir.OpNe:
			// Record type guards for arithmetic/comparison operations.
			if cfg.RecordGuards {
				t.recordTypeGuards(f, ip, in)
			}
			ip++

		case ir.OpStore:
			// Record the slot being written to.
			if cfg.RecordGuards {
				t.recordStoreGuard(f, ip, in)
			}
			ip++

		case ir.OpForInit:
			// For loops: record the init, then the loop body becomes a child trace.
			if ip+1 < len(f.Code) && depth < cfg.MaxDepth {
				child := t.buildLoopBodyTrace(f, ip+1, cfg, depth+1)
				if child != nil {
					t.Children = append(t.Children, child)
				}
			}
			ip++

		case ir.OpForNext:
			// For-next: if we loop back, record the back-edge.
			nextIP := int(in.B)
			if nextIP < ip {
				// Backwards jump — loop detected.
				t.SideExits = append(t.SideExits, SideExit{
					IP:       len(t.Code) - 1,
					TargetIP: nextIP,
				})
			}
			// Fall through is loop exit.
			ip++

		default:
			ip++
		}
	}
}

// buildLoopTrace creates a child trace for a loop body.
func (t *Trace) buildLoopTrace(f *ir.Func, ip int, cfg TraceConfig, depth int) *Trace {
	child := &Trace{
		Func:     f,
		Kind:     TraceLoop,
		Entry:    ip,
		Exit:     ip,
		Parent:   t,
		Depth:    depth,
		MaxDepth: cfg.MaxDepth,
	}
	visited := make(map[int]int)
	child.buildLinear(f, ip, cfg, visited, depth)
	if len(child.Code) == 0 {
		return nil
	}
	return child
}

// buildTailTrace creates a child trace for code after a call returns.
func (t *Trace) buildTailTrace(f *ir.Func, ip int, cfg TraceConfig, depth int) *Trace {
	child := &Trace{
		Func:     f,
		Kind:     TraceTail,
		Entry:    ip,
		Exit:     ip,
		Parent:   t,
		Depth:    depth,
		MaxDepth: cfg.MaxDepth,
	}
	visited := make(map[int]int)
	child.buildLinear(f, ip, cfg, visited, depth)
	if len(child.Code) == 0 {
		return nil
	}
	return child
}

// buildLoopBodyTrace creates a child trace for the body of a for loop.
func (t *Trace) buildLoopBodyTrace(f *ir.Func, ip int, cfg TraceConfig, depth int) *Trace {
	child := &Trace{
		Func:     f,
		Kind:     TraceLoop,
		Entry:    ip,
		Exit:     ip,
		Parent:   t,
		Depth:    depth,
		MaxDepth: cfg.MaxDepth,
	}
	visited := make(map[int]int)
	child.buildLinear(f, ip, cfg, visited, depth)
	if len(child.Code) == 0 {
		return nil
	}
	return child
}

// recordTypeGuards adds type guards for arithmetic/comparison instructions.
func (t *Trace) recordTypeGuards(f *ir.Func, ip int, in ir.Instr) {
	// Guard that the top-of-stack values are numeric (Int or Float).
	// The B and C operands typically reference the stack slots.
	// We guard the kind of values that these ops consume.
	if in.A > 0 {
		t.Guards = append(t.Guards, Guard{
			Kind: GuardType,
			IP:   ip,
			Slot: int(in.A),
		})
	}
	if in.B > 0 {
		t.Guards = append(t.Guards, Guard{
			Kind: GuardType,
			IP:   ip,
			Slot: int(in.B),
		})
	}
}

// recordStoreGuard adds a guard for store operations.
func (t *Trace) recordStoreGuard(f *ir.Func, ip int, in ir.Instr) {
	// Record that we're storing to an env slot.
	// The slot index is in.B.
	if in.B > 0 {
		t.Guards = append(t.Guards, Guard{
			Kind: GuardRange,
			IP:   ip,
			Slot: int(in.B),
			Lo:   0,
			Hi:   int64(f.NLocals),
		})
	}
}

// HasLoops returns whether the trace contains any loop children.
func (t *Trace) HasLoops() bool {
	for _, c := range t.Children {
		if c.Kind == TraceLoop {
			return true
		}
	}
	return false
}

// TotalInstructions returns the total number of instructions across the trace tree.
func (t *Trace) TotalInstructions() int {
	n := len(t.Code)
	for _, c := range t.Children {
		n += c.TotalInstructions()
	}
	return n
}

// CanExecute reports whether the trace can be run by the runtime trace tier.
// Native emission supports more instructions as artifacts, but the runtime
// tier deliberately excludes operations that can suspend or cross activation
// boundaries.
func (t *Trace) CanExecute() bool {
	if len(t.Code) == 0 || len(t.Code) != len(t.IPs) {
		return false
	}
	for _, in := range t.Code {
		switch in.Op {
		case ir.OpCall, ir.OpEffectBegin, ir.OpEffectEnd, ir.OpEffectCatch,
			ir.OpRaise, ir.OpSubscribe:
			return false
		}
	}
	return true
}

// CanNativeExecute reports whether the trace is simple enough for the
// amd64 Keystone-backed native executor.
func (t *Trace) CanNativeExecute() bool {
	if len(t.Code) == 0 || len(t.Code) != len(t.IPs) {
		return false
	}
	for _, in := range t.Code {
		switch in.Op {
		case ir.OpConst, ir.OpLoad, ir.OpStore, ir.OpPop, ir.OpDup,
			ir.OpArith, ir.OpNeg, ir.OpNot, ir.OpEq, ir.OpNe, ir.OpCmp,
			ir.OpJump, ir.OpJumpFalse, ir.OpJumpTrue, ir.OpReturn, ir.OpLine:
		default:
			return false
		}
		if (in.Op == ir.OpLoad || in.Op == ir.OpStore) && in.A != 0 {
			return false
		}
	}
	return true
}

// Walk traverses the trace tree in pre-order, calling fn for each trace.
func (t *Trace) Walk(fn func(*Trace)) {
	fn(t)
	for _, c := range t.Children {
		c.Walk(fn)
	}
}

// TraceHead returns the entry IP that should be used as a trace head for f.
// Trace heads are the start of loops (backwards-branch targets) and function entries.
func TraceHeads(f *ir.Func) []int {
	if f == nil || len(f.Code) == 0 {
		return nil
	}
	heads := map[int]bool{0: true} // function entry is always a head
	for i, in := range f.Code {
		switch in.Op {
		case ir.OpJump:
			target := int(in.A)
			if target < i {
				heads[target] = true // backwards jump → loop head
			}
		case ir.OpJumpFalse, ir.OpJumpTrue:
			target := int(in.A)
			if target < i {
				heads[target] = true
			}
			// Fall-through after a conditional is also a head.
			if i+1 < len(f.Code) {
				heads[i+1] = true
			}
		case ir.OpForNext:
			target := int(in.B)
			if target < i {
				heads[target] = true
			}
		case ir.OpEffectBegin:
			heads[int(in.A)] = true
		case ir.OpMatch:
			heads[int(in.B)] = true
		}
	}
	// Also: targets of OpForNext and OpEffectBegin that are in range.
	for _, in := range f.Code {
		switch in.Op {
		case ir.OpEffectBegin:
			if int(in.A) < len(f.Code) {
				heads[int(in.A)] = true
			}
		case ir.OpMatch:
			if int(in.B) < len(f.Code) {
				heads[int(in.B)] = true
			}
		}
	}
	// Sort the heads.
	out := make([]int, 0, len(heads))
	for h := range heads {
		out = append(out, h)
	}
	sortInts(out)
	return out
}

// TraceRegion returns the region of code between a trace head and the next head.
// This is useful for identifying code that should be traced together.
func TraceRegion(f *ir.Func, head int) []ir.Instr {
	heads := TraceHeads(f)
	nextHead := len(f.Code)
	for _, h := range heads {
		if h > head && h < nextHead {
			nextHead = h
		}
	}
	return f.Code[head:nextHead]
}

func sortInts(a []int) {
	for i := 1; i < len(a); i++ {
		j := i
		for j > 0 && a[j] < a[j-1] {
			a[j], a[j-1] = a[j-1], a[j]
			j--
		}
	}
}
