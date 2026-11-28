package jit

import (
	"strings"
	"testing"

	"domscript/internal/ir"
	"domscript/internal/runtime"
)

// ---------------------------------------------------------------------------
// ISA parsing tests
// ---------------------------------------------------------------------------

func TestParseISA_AMD64(t *testing.T) {
	isa, err := ParseISA(amd64ISA)
	if err != nil {
		t.Fatalf("ParseISA(amd64): %v", err)
	}
	if isa.Arch != "amd64" {
		t.Errorf("Arch = %q, want amd64", isa.Arch)
	}
	if isa.WordSize != 64 {
		t.Errorf("WordSize = %d, want 64", isa.WordSize)
	}
	if isa.Endian != "little" {
		t.Errorf("Endian = %q, want little", isa.Endian)
	}
	if len(isa.Registers) == 0 {
		t.Error("no registers parsed")
	}
	if len(isa.RegClasses) == 0 {
		t.Error("no register classes parsed")
	}

	// Check GPR class.
	rc, ok := isa.RegClass("GPR")
	if !ok {
		t.Fatal("GPR regclass not found")
	}
	if len(rc.Registers) == 0 {
		t.Error("GPR has no registers")
	}

	// Check register lookup.
	rax, ok := isa.Register("RAX")
	if !ok {
		t.Error("RAX not found")
	}
	if rax.Number != 0 {
		t.Errorf("RAX number = %d, want 0", rax.Number)
	}

	// Check alias.
	rbx, ok := isa.Register("ebx")
	if !ok {
		t.Error("ebx alias not found")
	}
	if rbx.Name != "RBX" {
		t.Errorf("ebx resolved to %q, want RBX", rbx.Name)
	}
}

func TestParseISA_ARM64(t *testing.T) {
	isa, err := ParseISA(arm64ISA)
	if err != nil {
		t.Fatalf("ParseISA(arm64): %v", err)
	}
	if isa.Arch != "arm64" {
		t.Errorf("Arch = %q, want arm64", isa.Arch)
	}
	if isa.WordSize != 64 {
		t.Errorf("WordSize = %d, want 64", isa.WordSize)
	}
	if len(isa.Registers) == 0 {
		t.Error("no registers parsed")
	}
}

func TestParseISA_RV64(t *testing.T) {
	isa, err := ParseISA(rv64ISA)
	if err != nil {
		t.Fatalf("ParseISA(rv64): %v", err)
	}
	if isa.Arch != "rv64" {
		t.Errorf("Arch = %q, want rv64", isa.Arch)
	}
}

func TestParseISA_Invalid(t *testing.T) {
	_, err := ParseISA("garbage")
	if err == nil {
		t.Error("expected error for invalid ISA")
	}
}

func TestISA_Helpers(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)

	// CallerSaved
	cs := isa.CallerSaved()
	if len(cs) == 0 {
		t.Error("CallerSaved: empty")
	}

	// CalleeSaved
	cal := isa.CalleeSaved()
	if len(cal) == 0 {
		t.Error("CalleeSaved: empty")
	}

	// FramePointer
	fp, ok := isa.FramePointer()
	if !ok {
		t.Error("FramePointer: not found")
	}
	if fp.Name != "RBP" {
		t.Errorf("FramePointer = %q, want RBP", fp.Name)
	}

	// StackPointer
	sp, ok := isa.StackPointer()
	if !ok {
		t.Error("StackPointer: not found")
	}
	if sp.Name != "RSP" {
		t.Errorf("StackPointer = %q, want RSP", sp.Name)
	}

	// ReturnReg
	rr, ok := isa.ReturnReg()
	if !ok {
		t.Error("ReturnReg: not found")
	}
	if rr.Name != "RAX" {
		t.Errorf("ReturnReg = %q, want RAX", rr.Name)
	}
}

func TestISA_Validate(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	if err := isa.Validate(); err != nil {
		t.Errorf("Validate: %v", err)
	}
}

// ---------------------------------------------------------------------------
// Peephole tests
// ---------------------------------------------------------------------------

func TestPeephole_DupPop(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpConst, A: 0},
		{Op: ir.OpDup},
		{Op: ir.OpPop},
	}
	result := Peephole(code)
	if len(result) != 1 {
		t.Errorf("DupPop: got %d instrs, want 1", len(result))
	}
}

func TestPeephole_AdjacentLine(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpLine, A: 1},
		{Op: ir.OpLine, A: 2},
		{Op: ir.OpConst, A: 0},
	}
	result := Peephole(code)
	if len(result) != 2 {
		t.Errorf("AdjacentLine: got %d instrs, want 2", len(result))
	}
	if result[0].A != 2 {
		t.Errorf("last line kept should be A=2, got A=%d", result[0].A)
	}
}

func TestPeephole_ConstPop(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpConst, A: 0},
		{Op: ir.OpPop},
		{Op: ir.OpConst, A: 1},
	}
	result := Peephole(code)
	if len(result) != 1 {
		t.Errorf("ConstPop: got %d instrs, want 1: %v", len(result), result)
	}
}

func TestPeephole_JumpToNext(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpJump, A: 1}, // jump to next instruction
		{Op: ir.OpConst, A: 0},
		{Op: ir.OpReturn},
	}
	result := Peephole(code)
	if len(result) != 2 {
		t.Errorf("JumpToNext: got %d instrs, want 2", len(result))
	}
}

func TestPeephole_NegNeg(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpConst, A: 0},
		{Op: ir.OpNeg},
		{Op: ir.OpNeg},
	}
	result := Peephole(code)
	if len(result) != 1 {
		t.Errorf("NegNeg: got %d instrs, want 1", len(result))
	}
}

func TestPeephole_NotNot(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpConst, A: 0},
		{Op: ir.OpNot},
		{Op: ir.OpNot},
	}
	result := Peephole(code)
	if len(result) != 1 {
		t.Errorf("NotNot: got %d instrs, want 1", len(result))
	}
}

func TestPeephole_RedundantLoadStore(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpStore, A: 0, B: 1}, // store to slot 1
		{Op: ir.OpLoad, A: 0, B: 1},  // load from same slot
	}
	result := Peephole(code)
	if len(result) != 2 {
		t.Errorf("RedundantLoadStore: got %d instrs, want 2", len(result))
	}
	if result[1].Op != ir.OpDup {
		t.Errorf("Load should be replaced with Dup, got %s", result[1].Op)
	}
}

// ---------------------------------------------------------------------------
// Trace building tests
// ---------------------------------------------------------------------------

func TestBuildTrace_Linear(t *testing.T) {
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 0},
			{Op: ir.OpConst, A: 1},
			{Op: ir.OpArith, A: ir.ArithAdd, B: 10, C: 11},
			{Op: ir.OpReturn},
		},
	}
	trace := BuildTrace(f, 0, 100)
	if len(trace.Code) != 4 {
		t.Errorf("linear trace: got %d instrs, want 4", len(trace.Code))
	}
	if trace.Exit != 3 {
		t.Errorf("Exit = %d, want 3", trace.Exit)
	}
}

func TestBuildTrace_WithJump(t *testing.T) {
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 0},
			{Op: ir.OpJump, A: 3},
			{Op: ir.OpConst, A: 1}, // dead
			{Op: ir.OpConst, A: 2},
			{Op: ir.OpReturn},
		},
	}
	trace := BuildTrace(f, 0, 100)
	if len(trace.Code) != 4 {
		t.Errorf("trace with jump: got %d instrs, want 4", len(trace.Code))
	}
}

func TestBuildTrace_Guards(t *testing.T) {
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 0},
			{Op: ir.OpConst, A: 1},
			{Op: ir.OpArith, A: ir.ArithAdd, B: 10, C: 11},
			{Op: ir.OpReturn},
		},
	}
	cfg := DefaultTraceConfig()
	cfg.RecordGuards = true
	trace := BuildTraceWithConfig(f, 0, cfg)
	if len(trace.Guards) == 0 {
		t.Error("expected guards for arithmetic, got none")
	}
}

func TestBuildTrace_Branch(t *testing.T) {
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 0},
			{Op: ir.OpJumpFalse, A: 4},
			{Op: ir.OpConst, A: 1},
			{Op: ir.OpReturn},
			{Op: ir.OpConst, A: 2},
			{Op: ir.OpReturn},
		},
	}
	cfg := DefaultTraceConfig()
	cfg.RecordGuards = true
	trace := BuildTraceWithConfig(f, 0, cfg)
	if len(trace.SideExits) == 0 {
		t.Error("expected side exits for branch")
	}
}

func TestBuildTrace_Loop(t *testing.T) {
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 0},    // 0
			{Op: ir.OpConst, A: 1},    // 1
			{Op: ir.OpArith, A: ir.ArithAdd, B: 10, C: 11}, // 2
			{Op: ir.OpJump, A: 1}, // 3: loop back
		},
	}
	cfg := DefaultTraceConfig()
	cfg.MaxInstrs = 20
	cfg.MaxDepth = 2
	trace := BuildTraceWithConfig(f, 0, cfg)
	if len(trace.Children) == 0 {
		t.Error("expected loop child trace")
	}
}

func TestTraceHeads(t *testing.T) {
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 0},    // 0: entry head
			{Op: ir.OpConst, A: 1},    // 1
			{Op: ir.OpJump, A: 1},     // 2: back edge to 1
		},
	}
	heads := TraceHeads(f)
	if len(heads) < 2 {
		t.Errorf("expected at least 2 heads, got %d: %v", len(heads), heads)
	}
	// Entry at 0 is always a head.
	if heads[0] != 0 {
		t.Errorf("first head should be 0, got %d", heads[0])
	}
}

func TestTrace_TotalInstructions(t *testing.T) {
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 0},
			{Op: ir.OpConst, A: 1},
			{Op: ir.OpArith, A: ir.ArithAdd, B: 10, C: 11},
			{Op: ir.OpReturn},
		},
	}
	trace := BuildTrace(f, 0, 100)
	if trace.TotalInstructions() != 4 {
		t.Errorf("TotalInstructions = %d, want 4", trace.TotalInstructions())
	}
}

// ---------------------------------------------------------------------------
// Register allocator tests
// ---------------------------------------------------------------------------

func TestAllocator_Basic(t *testing.T) {
	alloc := NewAllocator(8)
	loc := alloc.Allocate(1)
	if loc.Register != -1 || loc.Spill != 0 {
		t.Errorf("initial allocate: got %+v, want spill 0", loc)
	}
}

func TestAllocator_LinearScan(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpConst, A: 1, B: 42},     // def v1
		{Op: ir.OpConst, A: 2, B: 10},     // def v2
		{Op: ir.OpArith, A: 3, B: 1, C: 2}, // def v3, use v1, v2
		{Op: ir.OpReturn, A: 3},            // use v3
	}
	alloc := NewAllocator(8)
	alloc.ComputeLiveRanges(code)
	alloc.AllocateAll()

	// v1, v2, v3 should all be allocated to registers (we have 8 regs for 3 live ranges).
	for _, v := range []int{1, 2, 3} {
		loc, ok := alloc.LocationOf(v)
		if !ok {
			t.Errorf("v%d not allocated", v)
		}
		if loc.Register < 0 {
			t.Errorf("v%d spilled (reg=%d, spill=%d) but should fit in 8 regs", v, loc.Register, loc.Spill)
		}
	}
}

func TestAllocator_Spill(t *testing.T) {
	// Create many virtual registers to force spilling.
	code := []ir.Instr{
		{Op: ir.OpConst, A: 1, B: 1},
		{Op: ir.OpConst, A: 2, B: 2},
		{Op: ir.OpConst, A: 3, B: 3},
		{Op: ir.OpConst, A: 4, B: 4},
		{Op: ir.OpConst, A: 5, B: 5},
		{Op: ir.OpArith, A: 6, B: 1, C: 2},
		{Op: ir.OpArith, A: 7, B: 3, C: 4},
		{Op: ir.OpArith, A: 8, B: 5, C: 6},
		{Op: ir.OpReturn, A: 8},
	}
	alloc := NewAllocator(3) // only 3 regs, should spill
	alloc.ComputeLiveRanges(code)
	alloc.AllocateAll()

	if alloc.SpillCount() == 0 {
		t.Error("expected spills with only 3 registers for 8 live ranges")
	}
}

func TestAllocator_Empty(t *testing.T) {
	alloc := NewAllocator(8)
	alloc.ComputeLiveRanges(nil)
	alloc.AllocateAll()
	if alloc.SpillCount() != 0 {
		t.Error("empty code should not spill")
	}
}

// ---------------------------------------------------------------------------
// Deopt tests
// ---------------------------------------------------------------------------

func TestDeoptPoints(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpConst, A: 0},
		{Op: ir.OpCall, A: 1},
		{Op: ir.OpEffectBegin, A: 2},
		{Op: ir.OpRaise, A: 3},
		{Op: ir.OpReturn},
	}
	points := DeoptPoints(code)
	if len(points) < 3 {
		t.Errorf("expected at least 3 deopt points, got %d", len(points))
	}
}

func TestBuildDeoptMetadata(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpCall, A: 0},
			{Op: ir.OpReturn, A: 1},
		},
	}
	trace := BuildTrace(f, 0, 100)
	dm := BuildDeoptMetadata(trace, isa)
	if dm == nil {
		t.Fatal("BuildDeoptMetadata returned nil")
	}
	if dm.FuncName != "test" {
		t.Errorf("FuncName = %q, want test", dm.FuncName)
	}
	if len(dm.CalSaveRegs) == 0 {
		t.Error("CalleeSaved registers empty")
	}
}

func TestFormatDeopt(t *testing.T) {
	dm := &DeoptMetadata{
		FuncName:    "test",
		FrameSize:   128,
		SpillSlots:  2,
		CalSaveRegs: []int{3, 4, 5},
		Points: []DeoptPoint{
			{IP: 0, Reason: "call", StackDepth: 1},
			{IP: 2, Reason: "return", StackDepth: 0},
		},
	}
	s := FormatDeopt(dm)
	if !strings.Contains(s, "test") {
		t.Error("FormatDeopt missing function name")
	}
	if !strings.Contains(s, "frame=128") {
		t.Error("FormatDeopt missing frame size")
	}
}

// ---------------------------------------------------------------------------
// Scheduling tests
// ---------------------------------------------------------------------------

func TestSchedule_Basic(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpConst, A: 1, B: 10},
		{Op: ir.OpConst, A: 2, B: 20},
		{Op: ir.OpArith, A: 3, B: 1, C: 2},
		{Op: ir.OpReturn, A: 3},
	}
	result := Schedule(code)
	if len(result) != len(code) {
		t.Errorf("Schedule: got %d results, want %d", len(result), len(code))
	}
	// The arith should be scheduled after both consts.
	if result[2].Cycle <= result[0].Cycle || result[2].Cycle <= result[1].Cycle {
		t.Error("Arith should be scheduled after its operands")
	}
}

func TestSchedule_Independent(t *testing.T) {
	code := []ir.Instr{
		{Op: ir.OpConst, A: 1, B: 10},
		{Op: ir.OpConst, A: 2, B: 20},
		{Op: ir.OpConst, A: 3, B: 30},
	}
	result := Schedule(code)
	// Independent instructions can all be issued in the same cycle.
	// They should all have the same cycle (0).
	if len(result) != 3 {
		t.Fatalf("Schedule: got %d results, want 3", len(result))
	}
}

func TestSchedule_Empty(t *testing.T) {
	result := Schedule(nil)
	if result != nil {
		t.Error("Schedule of nil should return nil")
	}
	result = Schedule([]ir.Instr{})
	if result != nil {
		t.Error("Schedule of empty should return nil")
	}
}

// ---------------------------------------------------------------------------
// Emit tests
// ---------------------------------------------------------------------------

func TestEmitTrace(t *testing.T) {
	f := &ir.Func{Name: "test"}
	trace := Trace{
		Func:  f,
		Entry: 0,
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 0},
			{Op: ir.OpReturn},
		},
		Guards: []Guard{
			{Kind: GuardType, IP: 0, Slot: 1, Type: runtime.KindInt},
		},
	}
	s := EmitTrace(trace)
	if !strings.Contains(s, "test") {
		t.Error("EmitTrace missing function name")
	}
	if !strings.Contains(s, "const") {
		t.Error("EmitTrace missing instruction")
	}
	if !strings.Contains(s, "guards") {
		t.Error("EmitTrace missing guards")
	}
}

func TestEmitNative_Amd64(t *testing.T) {
	isa, err := ParseISA(amd64ISA)
	if err != nil {
		t.Fatal(err)
	}
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpConst, A: 2, B: 10},
			{Op: ir.OpArith, A: 3, B: 1, C: 2},
			{Op: ir.OpReturn, A: 3},
		},
	}
	trace := BuildTraceWithConfig(f, 0, DefaultTraceConfig())
	code, relocs, err := EmitNative(trace, isa)
	if err != nil {
		t.Fatalf("EmitNative: %v", err)
	}
	if len(code) == 0 {
		t.Error("EmitNative produced empty code")
	}
	// Relocations may or may not be present depending on the trace.
	_ = relocs
}

func TestEmitNative_Arm64(t *testing.T) {
	isa, err := ParseISA(arm64ISA)
	if err != nil {
		t.Fatal(err)
	}
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpReturn, A: 1},
		},
	}
	trace := BuildTrace(f, 0, 100)
	code, _, err := EmitNative(trace, isa)
	if err != nil {
		t.Fatalf("EmitNative arm64: %v", err)
	}
	if len(code) == 0 {
		t.Error("EmitNative arm64 produced empty code")
	}
}

func TestEmitNative_RV64(t *testing.T) {
	isa, err := ParseISA(rv64ISA)
	if err != nil {
		t.Fatal(err)
	}
	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpReturn, A: 1},
		},
	}
	trace := BuildTrace(f, 0, 100)
	code, _, err := EmitNative(trace, isa)
	if err != nil {
		t.Fatalf("EmitNative rv64: %v", err)
	}
	if len(code) == 0 {
		t.Error("EmitNative rv64 produced empty code")
	}
}

// ---------------------------------------------------------------------------
// Compiler tests
// ---------------------------------------------------------------------------

func TestCompiler_New(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)
	if c == nil {
		t.Fatal("NewCompiler returned nil")
	}
	if c.HotThreshold != 50 {
		t.Errorf("HotThreshold = %d, want 50", c.HotThreshold)
	}
}

func TestCompiler_Profile(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)
	c.HotThreshold = 3

	f := &ir.Func{Name: "test"}

	// First two hits.
	if c.Notify(f, 0) {
		t.Error("should not be hot after 1 hit")
	}
	c.Notify(f, 0)
	if c.Notify(f, 0) {
		// Third hit should trigger.
	} else {
		t.Error("should be hot after 3 hits")
	}
}

func TestCompiler_Compile(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)
	c.HotThreshold = 1

	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpReturn, A: 1},
		},
	}

	c.Notify(f, 0)
	ct, err := c.Compile(f, 0)
	if err != nil {
		t.Fatalf("Compile: %v", err)
	}
	if ct == nil {
		t.Fatal("Compile returned nil")
	}
	if len(ct.Code) == 0 {
		t.Error("Compiled trace has empty code")
	}
	if ct.Deopt == nil {
		t.Error("Compiled trace has no deopt metadata")
	}
}

func TestCompiler_Cache(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)

	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpReturn, A: 1},
		},
	}

	ct1, err := c.Compile(f, 0)
	if err != nil {
		t.Fatalf("first Compile: %v", err)
	}

	// Second compilation should return the cached version.
	ct2, err := c.Compile(f, 0)
	if err != nil {
		t.Fatalf("second Compile: %v", err)
	}

	if ct1 != ct2 {
		t.Error("Compile should return cached trace")
	}
}

func TestCompiler_Lookup(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)

	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpReturn, A: 1},
		},
	}

	ct, _ := c.Compile(f, 0)
	found := c.Lookup(f, 0)
	if found != ct {
		t.Error("Lookup should return cached trace")
	}

	missing := c.Lookup(f, 999)
	if missing != nil {
		t.Error("Lookup should return nil for unknown entry")
	}
}

func TestCompiler_Invalidate(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)

	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpReturn, A: 1},
		},
	}

	c.Compile(f, 0)
	c.Invalidate(f, 0)

	if ct := c.Lookup(f, 0); ct != nil {
		t.Error("Invalidated trace should not be in cache")
	}
}

func TestCompiler_CompileHeads(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)
	c.HotThreshold = 1

	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 10},
			{Op: ir.OpConst, A: 2, B: 20},
			{Op: ir.OpArith, A: 3, B: 1, C: 2},
			{Op: ir.OpReturn, A: 3},
		},
	}

	compiled, err := c.CompileHeads(f)
	if err != nil {
		t.Fatalf("CompileHeads: %v", err)
	}
	if len(compiled) == 0 {
		t.Error("CompileHeads should compile at least entry head")
	}
}

func TestCompiler_Stats(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)

	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpReturn, A: 1},
		},
	}

	c.Compile(f, 0)
	s := c.StatsString()
	if !strings.Contains(s, "JIT:") {
		t.Error("StatsString should start with 'JIT:'")
	}
}

func TestCompiledTrace_Disassemble(t *testing.T) {
	isa, _ := ParseISA(amd64ISA)
	c := NewCompiler(isa)

	f := &ir.Func{
		Name: "test",
		Code: []ir.Instr{
			{Op: ir.OpConst, A: 1, B: 42},
			{Op: ir.OpReturn, A: 1},
		},
	}

	ct, _ := c.Compile(f, 0)
	s := ct.Disassemble()
	if !strings.Contains(s, "test") {
		t.Error("Disassemble missing function name")
	}
	if !strings.Contains(s, "Native code") {
		t.Error("Disassemble missing code section")
	}
}

// ---------------------------------------------------------------------------
// ISA snippet fixtures
// ---------------------------------------------------------------------------

const amd64ISA = `
arch amd64 {
    word_size = 64;
    addr_size = 64;
    endian    = little;
    align     = 8;
}

regclass GPR {
    RAX(64)=0,
    RCX(64)=1,
    RDX(64)=2,
    RBX(64)=3,
    RSP(64)=4,
    RBP(64)=5,
    RSI(64)=6,
    RDI(64)=7,
}

regclass FPR {
    XMM0(128)=0,
    XMM1(128)=1,
}

alias ebx = RBX;
alias eax = RAX;
`

const arm64ISA = `
arch arm64 {
    word_size = 64;
    addr_size = 64;
    endian    = little;
    align     = 8;
}

regclass GPR {
    R0(64)=0,
    R1(64)=1,
    R2(64)=2,
    R3(64)=3,
    R4(64)=4,
    R5(64)=5,
    R6(64)=6,
    R7(64)=7,
    SP(64)=31,
}

regclass FPR {
    V0(128)=0,
    V1(128)=1,
}
`

const rv64ISA = `
arch rv64 {
    word_size = 64;
    addr_size = 64;
    endian    = little;
    align     = 8;
}

regclass GPR {
    T0(64),
    T1(64),
    T2(64),
    A0(64),
    A1(64),
    SP(64),
    RA(64),
}

regclass FPR {
    FT0(64),
    FT1(64),
}
`
