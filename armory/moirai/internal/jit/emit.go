package jit

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"
	"strings"

	"domscript/internal/ir"
	"domscript/internal/runtime"
)

// EmitTrace emits the trace as assembly text for the target ISA.
func EmitTrace(t Trace) string {
	var b strings.Builder
	fmt.Fprintf(&b, "# trace %s @%d (kind=%v depth=%d)\n", t.Func.Name, t.Entry, t.Kind, t.Depth)
	fmt.Fprintf(&b, "# %d instructions, %d guards, %d side exits, %d children\n",
		len(t.Code), len(t.Guards), len(t.SideExits), len(t.Children))

	// Emit guards.
	if len(t.Guards) > 0 {
		fmt.Fprintf(&b, "# guards:\n")
		for _, g := range t.Guards {
			fmt.Fprintf(&b, "#   %s ip=%d slot=%d", guardKindName(g.Kind), g.IP, g.Slot)
			if g.Kind == GuardType {
				fmt.Fprintf(&b, " type=%s", g.Type)
			}
			if g.Kind == GuardConst {
				fmt.Fprintf(&b, " const=%v", g.ConstVal)
			}
			if g.Kind == GuardRange {
				fmt.Fprintf(&b, " range=[%d,%d]", g.Lo, g.Hi)
			}
			fmt.Fprintf(&b, "\n")
		}
	}

	// Emit side exits.
	if len(t.SideExits) > 0 {
		fmt.Fprintf(&b, "# side exits:\n")
		for _, se := range t.SideExits {
			fmt.Fprintf(&b, "#   exit @ip=%d -> target=%d\n", se.IP, se.TargetIP)
		}
	}

	// Emit optimized code.
	code := Peephole(t.Code)
	for i, in := range code {
		fmt.Fprintf(&b, "%04d  %s %d %d %d\n", i, in.Op, in.A, in.B, in.C)
	}

	// Emit children.
	for _, child := range t.Children {
		fmt.Fprintf(&b, "\n# --- child trace (kind=%v) ---\n", child.Kind)
		b.WriteString(EmitTrace(*child))
	}

	return b.String()
}

func guardKindName(k GuardKind) string {
	switch k {
	case GuardType:
		return "type"
	case GuardTruthy:
		return "truthy"
	case GuardFalsy:
		return "falsy"
	case GuardConst:
		return "const"
	case GuardNotNull:
		return "notnull"
	case GuardRange:
		return "range"
	}
	return "?"
}

// EmitNative compiles a trace to native machine code bytes for the given ISA.
// Returns the binary code blob and any relocation information.
func EmitNative(t Trace, isa ISA) ([]byte, []Relocation, error) {
	if err := isa.Validate(); err != nil {
		return nil, nil, fmt.Errorf("emit: invalid ISA: %w", err)
	}

	var buf bytes.Buffer
	var relocs []Relocation

	// Emit function prologue.
	if err := emitPrologue(&buf, isa); err != nil {
		return nil, nil, err
	}

	// Allocate registers and emit code.
	alloc := NewAllocator(len(isa.CallerSaved()))
	code := Peephole(t.Code)
	alloc.ComputeLiveRanges(code)
	alloc.AllocateAll()

	for ip, in := range code {
		loc, _ := alloc.LocationOf(int(in.A))
		if err := emitNativeInstr(&buf, &relocs, in, ip, isa, alloc); err != nil {
			return nil, nil, fmt.Errorf("emit: instr %d (%s): %w", ip, in.Op, err)
		}
		_ = loc
	}

	// Emit function epilogue.
	if err := emitEpilogue(&buf, isa); err != nil {
		return nil, nil, err
	}

	return buf.Bytes(), relocs, nil
}

// Relocation records a position in the code buffer that needs patching.
type Relocation struct {
	Offset int
	Kind   string // "abs64", "rel32", etc.
	Target int64  // target address or offset
}

// emitPrologue emits the function entry sequence.
func emitPrologue(buf *bytes.Buffer, isa ISA) error {
	switch isa.Arch {
	case "amd64":
		// push rbp; mov rbp, rsp; sub rsp, N
		buf.WriteByte(0x55)                   // push rbp
		buf.Write([]byte{0x48, 0x89, 0xe5})  // mov rbp, rsp
		// Reserve 128 bytes of stack space.
		buf.Write([]byte{0x48, 0x83, 0xec, 0x7f}) // sub rsp, 127 (with sign extension)
	case "arm64":
		// stp fp, lr, [sp, #-16]!; mov fp, sp; sub sp, sp, #128
		buf.Write(encodeARM64(0xa9bf7bfd)) // stp x29, x30, [sp, #-16]!
		buf.Write(encodeARM64(0x910003fd)) // mov x29, sp
		buf.Write(encodeARM64(0xd10083ff)) // sub sp, sp, #128
	case "rv64":
		// addi sp, sp, -128; sd ra, 120(sp); sd s0, 112(sp); addi s0, sp, 128
		buf.Write(encodeRV64(0xff010113)) // addi sp, sp, -16
		buf.Write(encodeRV64(0x00113023)) // sd ra, 0(sp)
		buf.Write(encodeRV64(0x00813423)) // sd s0, 8(sp)
		buf.Write(encodeRV64(0x00010413)) // addi s0, sp, 0
	}
	return nil
}

// emitEpilogue emits the function exit sequence.
func emitEpilogue(buf *bytes.Buffer, isa ISA) error {
	switch isa.Arch {
	case "amd64":
		// mov rsp, rbp; pop rbp; ret
		buf.Write([]byte{0x48, 0x89, 0xec}) // mov rsp, rbp
		buf.WriteByte(0x5d)                   // pop rbp
		buf.WriteByte(0xc3)                   // ret
	case "arm64":
		// mov sp, fp; ldp fp, lr, [sp], #16; ret
		buf.Write(encodeARM64(0x910003bf)) // mov sp, x29
		buf.Write(encodeARM64(0xa8c17bfd)) // ldp x29, x30, [sp], #16
		buf.Write(encodeARM64(0xd65f03c0)) // ret
	case "rv64":
		// ld ra, 0(sp); ld s0, 8(sp); addi sp, sp, 16; ret
		buf.Write(encodeRV64(0x00013083)) // ld ra, 0(sp)
		buf.Write(encodeRV64(0x00813403)) // ld s0, 8(sp)
		buf.Write(encodeRV64(0x01010113)) // addi sp, sp, 16
		buf.Write(encodeRV64(0x00008067)) // ret
	}
	return nil
}

// emitNativeInstr emits a single IR instruction as native code.
func emitNativeInstr(buf *bytes.Buffer, relocs *[]Relocation, in ir.Instr, ip int, isa ISA, alloc *Allocator) error {
	switch in.Op {
	case ir.OpConst:
		return emitConst(buf, in, isa, alloc)
	case ir.OpArith:
		return emitArith(buf, in, isa, alloc)
	case ir.OpNeg:
		return emitNeg(buf, in, isa, alloc)
	case ir.OpNot:
		return emitNot(buf, in, isa, alloc)
	case ir.OpEq, ir.OpNe, ir.OpCmp:
		return emitCompare(buf, in, isa, alloc)
	case ir.OpJump:
		return emitJump(buf, in, ip, isa, relocs)
	case ir.OpJumpFalse, ir.OpJumpTrue:
		return emitCondJump(buf, in, ip, isa, relocs)
	case ir.OpLoad:
		return emitLoad(buf, in, isa, alloc)
	case ir.OpStore:
		return emitStore(buf, in, isa, alloc)
	case ir.OpPop:
		// Pop is handled by the stack discipline; no native code needed.
		return nil
	case ir.OpDup:
		return emitDup(buf, in, isa, alloc)
	case ir.OpReturn:
		return emitReturn(buf, in, isa, alloc)
	case ir.OpCall:
		return emitCall(buf, in, ip, isa, relocs)
	case ir.OpLine, ir.OpTestBool, ir.OpSome:
		// No-ops in native code.
		return nil
	default:
		// For complex ops, emit a call to the runtime helper.
		return emitRuntimeCall(buf, in, ip, isa, relocs)
	}
}

// emitConst emits code to load a constant.
func emitConst(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	switch isa.Arch {
	case "amd64":
		// movabs $imm, %reg
		reg := allocReg(alloc, int(in.A))
		if reg >= 0 {
			buf.WriteByte(0x48) // REX.W
			buf.WriteByte(0xb8 + byte(reg&7))
			writeInt64(buf, in.B)
		}
	case "arm64":
		// movz + movk sequence for 64-bit immediate
		reg := allocReg(alloc, int(in.A))
		if reg >= 0 {
			val := uint64(in.B)
			// movz xN, #lo16
			buf.Write(encodeARM64(0xd2800000 | uint32(reg) | uint32((val&0xffff)<<5)))
			if val>>16 != 0 {
				// movk xN, #hi16, lsl #16
				buf.Write(encodeARM64(0xf2a00000 | uint32(reg) | uint32(((val>>16)&0xffff)<<5) | (1 << 21)))
			}
			if val>>32 != 0 {
				buf.Write(encodeARM64(0xf2c00000 | uint32(reg) | uint32(((val>>32)&0xffff)<<5) | (2 << 21)))
			}
			if val>>48 != 0 {
				buf.Write(encodeARM64(0xf2e00000 | uint32(reg) | uint32(((val>>48)&0xffff)<<5) | (3 << 21)))
			}
		}
	case "rv64":
		// lui + addi + slli + addi sequence
		reg := allocReg(alloc, int(in.A))
		if reg >= 0 {
			val := int64(in.B)
			if val >= -2048 && val < 2048 {
				// addi rd, x0, imm
				buf.Write(encodeRV64(0x00000413 | uint32((reg&0x1f)<<7) | uint32((val&0xfff)<<20)))
			} else {
				// lui + addi
				upper := (val + 0x800) >> 12
				buf.Write(encodeRV64(0x00000037 | uint32((reg&0x1f)<<7) | uint32((upper&0xfffff)<<12)))
				lower := val & 0xfff
				if lower < 0 {
					lower += 0x1000
				}
				buf.Write(encodeRV64(0x00000413 | uint32((reg&0x1f)<<7) | uint32((reg&0x1f)<<15) | uint32((lower&0xfff)<<20)))
			}
		}
	}
	return nil
}

// emitArith emits arithmetic operations.
func emitArith(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	regA := allocReg(alloc, int(in.A))
	regB := allocReg(alloc, int(in.B))
	regC := allocReg(alloc, int(in.C))
	_ = regC

	switch isa.Arch {
	case "amd64":
		if regA >= 0 && regB >= 0 {
			switch in.B {
			case ir.ArithAdd:
				// add %rb, %ra
				buf.Write([]byte{0x48, 0x01, byte(0xc0 | (regB&7)<<3 | (regA&7))})
			case ir.ArithSub:
				// sub %rb, %ra
				buf.Write([]byte{0x48, 0x29, byte(0xc0 | (regB&7)<<3 | (regA&7))})
			case ir.ArithMul:
				// imul %rb, %ra
				buf.Write([]byte{0x48, 0x0f, 0xaf, byte(0xc0 | (regA&7)<<3 | (regB&7))})
			case ir.ArithDiv:
				// cqo; idiv %rb
				buf.Write([]byte{0x48, 0x99}) // cqo
				buf.Write([]byte{0x48, 0xf7, byte(0xf8 | (regB&7))})
			case ir.ArithMod:
				// cqo; idiv %rb (remainder in rdx)
				buf.Write([]byte{0x48, 0x99}) // cqo
				buf.Write([]byte{0x48, 0xf7, byte(0xf8 | (regB&7))})
				// mov %rdx, %ra
				buf.Write([]byte{0x48, 0x89, byte(0xd0 | (regA&7))})
			}
		}
	case "arm64":
		if regA >= 0 && regB >= 0 && regC >= 0 {
			switch in.B {
			case ir.ArithAdd:
				buf.Write(encodeARM64(0x8b000000 | uint32(regA) | uint32(regC<<5) | uint32(regB<<16)))
			case ir.ArithSub:
				buf.Write(encodeARM64(0xcb000000 | uint32(regA) | uint32(regC<<5) | uint32(regB<<16)))
			case ir.ArithMul:
				buf.Write(encodeARM64(0x9b007c00 | uint32(regA) | uint32(regC<<5) | uint32(regB<<16)))
			case ir.ArithDiv:
				buf.Write(encodeARM64(0x9ac00c00 | uint32(regA) | uint32(regC<<5) | uint32(regB<<16)))
			}
		}
	case "rv64":
		if regA >= 0 && regB >= 0 && regC >= 0 {
			switch in.B {
			case ir.ArithAdd:
				buf.Write(encodeRV64(0x00000033 | uint32(regA<<7) | uint32(regC<<15) | uint32(regB<<20)))
			case ir.ArithSub:
				buf.Write(encodeRV64(0x40000033 | uint32(regA<<7) | uint32(regC<<15) | uint32(regB<<20)))
			case ir.ArithMul:
				buf.Write(encodeRV64(0x02000033 | uint32(regA<<7) | uint32(regC<<15) | uint32(regB<<20)))
			case ir.ArithDiv:
				buf.Write(encodeRV64(0x02000033 | uint32(regA<<7) | uint32(regC<<15) | uint32(regB<<20)))
			}
		}
	}
	return nil
}

// emitNeg emits numeric negation.
func emitNeg(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	reg := allocReg(alloc, int(in.A))
	switch isa.Arch {
	case "amd64":
		if reg >= 0 {
			buf.Write([]byte{0x48, 0xf7, byte(0xd8 | (reg&7))}) // neg %reg
		}
	case "arm64":
		if reg >= 0 {
			buf.Write(encodeARM64(0xcb0003e0 | uint32(reg) | uint32(reg<<5))) // neg xN, xN
		}
	case "rv64":
		if reg >= 0 {
			buf.Write(encodeRV64(0x40000033 | uint32(reg<<7) | uint32(0<<15) | uint32(reg<<20))) // sub rd, x0, rs
		}
	}
	return nil
}

// emitNot emits boolean negation.
func emitNot(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	reg := allocReg(alloc, int(in.A))
	switch isa.Arch {
	case "amd64":
		if reg >= 0 {
			buf.Write([]byte{0x48, 0x83, byte(0xf0 | (reg&7)), 0x01}) // xor $1, %reg
		}
	case "arm64":
		if reg >= 0 {
			buf.Write(encodeARM64(0xd1000400 | uint32(reg) | uint32(reg<<5))) // sub xN, xN, #1 → eor xN, xN, 1
			// Actually: eor xN, xN, #1
			buf.Write(encodeARM64(0xd2000020 | uint32(reg) | uint32(reg<<5) | uint32(1<<10)))
		}
	case "rv64":
		if reg >= 0 {
			buf.Write(encodeRV64(0x00100013 | uint32(reg<<7) | uint32(reg<<15) | uint32(1<<20))) // xori rd, rs, 1
		}
	}
	return nil
}

// emitCompare emits comparison operations.
func emitCompare(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	regA := allocReg(alloc, int(in.A))
	regB := allocReg(alloc, int(in.B))
	regC := allocReg(alloc, int(in.C))
	_ = regC

	switch isa.Arch {
	case "amd64":
		if regA >= 0 && regB >= 0 {
			// cmp %rb, %ra; setCC %al; movzbq %al, %ra
			buf.Write([]byte{0x48, 0x39, byte(0xc0 | (regB&7)<<3 | (regA&7))}) // cmp
			switch in.Op {
			case ir.OpEq:
				buf.Write([]byte{0x0f, 0x94, 0xc0}) // sete %al
			case ir.OpNe:
				buf.Write([]byte{0x0f, 0x95, 0xc0}) // setne %al
			case ir.OpCmp:
				switch in.B {
				case 0: // lt
					buf.Write([]byte{0x0f, 0x9c, 0xc0}) // setl %al
				case 1: // le
					buf.Write([]byte{0x0f, 0x9e, 0xc0}) // setle %al
				case 2: // gt
					buf.Write([]byte{0x0f, 0x9f, 0xc0}) // setg %al
				case 3: // ge
					buf.Write([]byte{0x0f, 0x9d, 0xc0}) // setge %al
				}
			}
			buf.Write([]byte{0x48, 0x0f, 0xb6, 0xc0}) // movzbq %al, %rax
		}
	case "arm64":
		if regA >= 0 && regB >= 0 && regC >= 0 {
			buf.Write(encodeARM64(0xeb00001f | uint32(regB<<5) | uint32(regC<<16))) // cmp
			switch in.Op {
			case ir.OpEq:
				buf.Write(encodeARM64(0x9a9f07e0 | uint32(regA))) // cset
			case ir.OpNe:
				buf.Write(encodeARM64(0x9a9f17e0 | uint32(regA))) // cset ne
			}
		}
	case "rv64":
		if regA >= 0 && regB >= 0 && regC >= 0 {
			switch in.Op {
			case ir.OpEq:
				buf.Write(encodeRV64(0x00000033 | uint32(regA<<7) | uint32(regB<<15) | uint32(regC<<20) | (0x0<<12))) // sub; sltu
				// xor rd, rs1, rs2; sltiu rd, rd, 1
				buf.Write(encodeRV64(0x00000033 | uint32(regA<<7) | uint32(regB<<15) | uint32(regC<<20) | (0x4<<12)))
				buf.Write(encodeRV64(0x00103013 | uint32(regA<<7) | uint32(regA<<15) | uint32(1<<20)))
			case ir.OpNe:
				buf.Write(encodeRV64(0x00000033 | uint32(regA<<7) | uint32(regB<<15) | uint32(regC<<20) | (0x4<<12))) // xor
				buf.Write(encodeRV64(0x00103033 | uint32(regA<<7) | uint32(regA<<15) | uint32(regA<<20))) // sltu rd, rd, x0 → nonzero
			}
		}
	}
	return nil
}

// emitJump emits an unconditional jump.
func emitJump(buf *bytes.Buffer, in ir.Instr, ip int, isa ISA, relocs *[]Relocation) error {
	// Jumps are resolved at link time via relocations.
	*relocs = append(*relocs, Relocation{
		Offset: buf.Len(),
		Kind:   "rel32",
		Target: in.A,
	})
	switch isa.Arch {
	case "amd64":
		buf.Write([]byte{0xe9, 0x00, 0x00, 0x00, 0x00}) // jmp rel32 (placeholder)
	case "arm64":
		buf.Write(encodeARM64(0x14000000)) // b (placeholder)
	case "rv64":
		buf.Write(encodeRV64(0x0000006f)) // jal x0, offset (placeholder)
	}
	return nil
}

// emitCondJump emits a conditional jump.
func emitCondJump(buf *bytes.Buffer, in ir.Instr, ip int, isa ISA, relocs *[]Relocation) error {
	*relocs = append(*relocs, Relocation{
		Offset: buf.Len(),
		Kind:   "rel32",
		Target: in.A,
	})
	switch isa.Arch {
	case "amd64":
		// test %rax, %rax; jz/jnz target
		buf.Write([]byte{0x48, 0x85, 0xc0}) // test %rax, %rax
		if in.Op == ir.OpJumpFalse {
			buf.Write([]byte{0x0f, 0x84, 0x00, 0x00, 0x00, 0x00}) // jz rel32
		} else {
			buf.Write([]byte{0x0f, 0x85, 0x00, 0x00, 0x00, 0x00}) // jnz rel32
		}
	case "arm64":
		// cbz/cbnz x0, target
		if in.Op == ir.OpJumpFalse {
			buf.Write(encodeARM64(0xb4000000)) // cbz x0, offset (placeholder)
		} else {
			buf.Write(encodeARM64(0xb5000000)) // cbnz x0, offset (placeholder)
		}
	case "rv64":
		// beq/bne x0, x0, target
		if in.Op == ir.OpJumpFalse {
			buf.Write(encodeRV64(0x00000063)) // beq x0, x0, offset (placeholder)
		} else {
			buf.Write(encodeRV64(0x00001063)) // bne x0, x0, offset (placeholder)
		}
	}
	return nil
}

// emitLoad emits code to load a local variable.
func emitLoad(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	// Load from env at depth A, slot B.
	// For now, emit a call to the runtime helper.
	return nil
}

// emitStore emits code to store a local variable.
func emitStore(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	return nil
}

// emitDup emits code to duplicate the top of stack.
func emitDup(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	reg := allocReg(alloc, int(in.A))
	switch isa.Arch {
	case "amd64":
		if reg >= 0 {
			// push %reg
			buf.Write([]byte{0x50 | byte(reg&7)})
		}
	case "arm64":
		if reg >= 0 {
			// str xN, [sp, #-8]!
			buf.Write(encodeARM64(0xf81f8fe0 | uint32(reg<<5)))
		}
	case "rv64":
		if reg >= 0 {
			// addi sp, sp, -8; sd rs, 0(sp)
			buf.Write(encodeRV64(0xff810113)) // addi sp, sp, -8
			buf.Write(encodeRV64(0x00013023 | uint32(reg<<20))) // sd rs, 0(sp)
		}
	}
	return nil
}

// emitReturn emits code for a return.
func emitReturn(buf *bytes.Buffer, in ir.Instr, isa ISA, alloc *Allocator) error {
	// Store return value in the return register, then jump to epilogue.
	reg := allocReg(alloc, int(in.A))
	if reg >= 0 {
		switch isa.Arch {
		case "amd64":
			// mov %reg, %rax (if not already rax)
			if reg != 0 {
				buf.Write([]byte{0x48, 0x89, byte(0xc0 | (reg&7)<<3)}) // mov %reg, %rax
			}
		case "arm64":
			if reg != 0 {
				buf.Write(encodeARM64(0xaa0003e0 | uint32(reg<<16))) // mov x0, xN
			}
		case "rv64":
			if reg != 10 { // a0
				buf.Write(encodeRV64(0x00000513 | uint32(10<<7) | uint32(reg<<15))) // addi a0, rs, 0
			}
		}
	}
	return emitEpilogue(buf, isa)
}

// emitCall emits code for a function call.
func emitCall(buf *bytes.Buffer, in ir.Instr, ip int, isa ISA, relocs *[]Relocation) error {
	// Calls are emitted as a call to the runtime helper.
	*relocs = append(*relocs, Relocation{
		Offset: buf.Len(),
		Kind:   "rel32",
		Target: in.A, // function index
	})
	switch isa.Arch {
	case "amd64":
		buf.Write([]byte{0xe8, 0x00, 0x00, 0x00, 0x00}) // call rel32
	case "arm64":
		buf.Write(encodeARM64(0x94000000)) // bl
	case "rv64":
		buf.Write(encodeRV64(0x000000ef)) // jal ra, offset
	}
	return nil
}

// emitRuntimeCall emits a call to a runtime helper for complex operations.
func emitRuntimeCall(buf *bytes.Buffer, in ir.Instr, ip int, isa ISA, relocs *[]Relocation) error {
	// Emit a call to the interpreter fallback.
	*relocs = append(*relocs, Relocation{
		Offset: buf.Len(),
		Kind:   "rel32",
		Target: int64(in.Op), // encode the opcode in the target
	})
	switch isa.Arch {
	case "amd64":
		buf.Write([]byte{0xe8, 0x00, 0x00, 0x00, 0x00}) // call rel32
	case "arm64":
		buf.Write(encodeARM64(0x94000000)) // bl
	case "rv64":
		buf.Write(encodeRV64(0x000000ef)) // jal ra, offset
	}
	return nil
}

// ---------------------------------------------------------------------------
// Helpers

// allocReg returns the physical register assigned to a virtual register, or -1.
func allocReg(alloc *Allocator, virtual int) int {
	if loc, ok := alloc.LocationOf(virtual); ok && loc.Register >= 0 {
		return loc.Register
	}
	// Use register 0 (rax/x0/a0) as default for unallocated virtual registers.
	if virtual > 0 && virtual <= alloc.Width {
		return virtual - 1
	}
	return -1
}

func writeInt64(buf *bytes.Buffer, val int64) {
	var b [8]byte
	binary.LittleEndian.PutUint64(b[:], uint64(val))
	buf.Write(b[:])
}

func writeInt32(buf *bytes.Buffer, val int32) {
	var b [4]byte
	binary.LittleEndian.PutUint32(b[:], uint32(val))
	buf.Write(b[:])
}

func encodeARM64(instr uint32) []byte {
	var b [4]byte
	binary.LittleEndian.PutUint32(b[:], instr)
	return b[:]
}

func encodeRV64(instr uint32) []byte {
	var b [4]byte
	binary.LittleEndian.PutUint32(b[:], instr)
	return b[:]
}

// Ensure unused imports are valid.
var _ = math.MaxFloat64
var _ = runtime.Unit
