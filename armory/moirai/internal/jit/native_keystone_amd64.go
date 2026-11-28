//go:build keystone && amd64 && linux && cgo

package jit

/*
#cgo CFLAGS: -I${SRCDIR}/../../../third_party/keystone/include
#cgo LDFLAGS: -L${SRCDIR}/../../build/keystone/install/lib -lkeystone -lstdc++ -lm

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <keystone/keystone.h>

typedef struct {
	uintptr_t locals;
	uintptr_t stack;
	uint64_t sp;
	uint64_t next_ip;
	uint64_t exit_kind;
	uint64_t ret;
} ds_jit_frame;

enum {
	ds_jit_frame_locals_off = offsetof(ds_jit_frame, locals),
	ds_jit_frame_stack_off = offsetof(ds_jit_frame, stack),
	ds_jit_frame_sp_off = offsetof(ds_jit_frame, sp),
	ds_jit_frame_next_ip_off = offsetof(ds_jit_frame, next_ip),
	ds_jit_frame_exit_kind_off = offsetof(ds_jit_frame, exit_kind),
	ds_jit_frame_ret_off = offsetof(ds_jit_frame, ret),
};

typedef uint64_t (*ds_jit_entry_fn)(ds_jit_frame *frame);

static inline uint64_t ds_jit_call(void *fn, ds_jit_frame *frame) {
	return ((ds_jit_entry_fn)fn)(frame);
}

*/
import "C"

import (
	"errors"
	"fmt"
	"strings"
	"syscall"
	"unsafe"

	"domscript/internal/ir"
	"domscript/internal/runtime"
)

var errNativeUnsupported = errors.New("jit: native backend unsupported")

func buildNativeTrace(t Trace, isa ISA) (uintptr, []byte, bool, error) {
	if isa.Arch != "amd64" || !t.CanNativeExecute() {
		return 0, nil, false, nil
	}
	asm, ok, err := renderNativeAMD64(t)
	if err != nil || !ok {
		return 0, nil, ok, err
	}
	code, err := assembleKeystoneAMD64(asm)
	if err != nil {
		return 0, nil, false, err
	}
	exec, err := makeExecutable(code)
	if err != nil {
		return 0, nil, false, err
	}
	return uintptr(unsafe.Pointer(&exec[0])), exec, true, nil
}

func releaseExecutable(buf []byte) error {
	if len(buf) == 0 {
		return nil
	}
	return syscall.Munmap(buf)
}

func CallNativeTrace(entry uintptr, localsPtr, stackPtr uintptr, sp uint64) (spOut uint64, nextIP uint64, exitKind uint64, ret uint64, ok bool, err error) {
	if entry == 0 {
		return 0, 0, 0, 0, false, nil
	}
	var frame C.ds_jit_frame
	frame.locals = C.uintptr_t(localsPtr)
	frame.stack = C.uintptr_t(stackPtr)
	frame.sp = C.uint64_t(sp)
	frame.next_ip = 0
	frame.exit_kind = 0
	frame.ret = 0
	C.ds_jit_call(unsafe.Pointer(entry), &frame)
	return uint64(frame.sp), uint64(frame.next_ip), uint64(frame.exit_kind), uint64(frame.ret), true, nil
}

func renderNativeAMD64(t Trace) (string, bool, error) {
	var b strings.Builder
	labels := make(map[int]string, len(t.IPs))
	for i, ip := range t.IPs {
		labels[ip] = fmt.Sprintf("L%d", i)
	}
	slotRegs, useSlotRegs := nativeLocalRegisters(t)

	b.WriteString("mov r10, qword ptr [rdi + ")
	b.WriteString(imm(C.ds_jit_frame_locals_off))
	b.WriteString("]\n")
	b.WriteString("mov r11, qword ptr [rdi + ")
	b.WriteString(imm(C.ds_jit_frame_stack_off))
	b.WriteString("]\n")
	b.WriteString("mov rcx, qword ptr [rdi + ")
	b.WriteString(imm(C.ds_jit_frame_sp_off))
	b.WriteString("]\n")
	if useSlotRegs {
		emitNativeLocalLoads(&b, slotRegs)
	}

	for i, in := range t.Code {
		ip := t.IPs[i]
		if label, ok := labels[ip]; ok {
			b.WriteString(label)
			b.WriteString(":\n")
		}
		switch in.Op {
		case ir.OpLine:
			continue
		case ir.OpConst:
			if t.Func == nil || int(in.A) < 0 || int(in.A) >= len(t.Func.Consts) {
				return "", false, nil
			}
			val := t.Func.Consts[in.A]
			switch val.Kind() {
			case runtime.KindInt, runtime.KindBool:
			default:
				return "", false, nil
			}
			b.WriteString("mov rax, ")
			b.WriteString(imm(constValue64(val)))
			b.WriteString("\n")
			pushRAX(&b)
		case ir.OpLoad:
			if reg, ok := slotRegs[int(in.B)]; ok {
				b.WriteString("mov rax, ")
				b.WriteString(reg)
				b.WriteString("\n")
			} else {
				b.WriteString("mov rax, qword ptr [r10 + ")
				b.WriteString(imm(in.B * 8))
				b.WriteString("]\n")
			}
			pushRAX(&b)
		case ir.OpStore:
			popRAX(&b)
			if reg, ok := slotRegs[int(in.B)]; ok {
				b.WriteString("mov ")
				b.WriteString(reg)
				b.WriteString(", rax\n")
			} else {
				b.WriteString("mov qword ptr [r10 + ")
				b.WriteString(imm(in.B * 8))
				b.WriteString("], rax\n")
			}
		case ir.OpPop:
			b.WriteString("dec rcx\n")
		case ir.OpDup:
			b.WriteString("mov rax, qword ptr [r11 + rcx*8 - 8]\n")
			pushRAX(&b)
		case ir.OpArith:
			if err := renderArith(&b, in.A); err != nil {
				return "", false, err
			}
		case ir.OpNeg:
			popRAX(&b)
			b.WriteString("neg rax\n")
			pushRAX(&b)
		case ir.OpNot:
			popRAX(&b)
			b.WriteString("xor rax, 1\n")
			pushRAX(&b)
		case ir.OpEq, ir.OpNe, ir.OpCmp:
			if err := renderCompare(&b, in); err != nil {
				return "", false, err
			}
		case ir.OpJump:
			target := int(in.A)
			if label, ok := labels[target]; ok {
				b.WriteString("jmp ")
				b.WriteString(label)
				b.WriteString("\n")
				continue
			}
			if useSlotRegs {
				emitNativeLocalFlush(&b, slotRegs)
			}
			renderSideExit(&b, fmt.Sprintf("side_%d", i), target)
			continue
		case ir.OpJumpFalse, ir.OpJumpTrue:
			target := int(in.A)
			popRAX(&b)
			if label, ok := labels[target]; ok {
				contLabel := fmt.Sprintf("cont_%d", i)
				b.WriteString("test rax, rax\n")
				if in.Op == ir.OpJumpFalse {
					b.WriteString("jnz ")
					b.WriteString(contLabel)
					b.WriteString("\n")
				} else {
					b.WriteString("jz ")
					b.WriteString(contLabel)
					b.WriteString("\n")
				}
				b.WriteString("jmp ")
				b.WriteString(label)
				b.WriteString("\n")
				b.WriteString(contLabel)
				b.WriteString(":\n")
				continue
			}
			contLabel := fmt.Sprintf("cont_%d", i)
			b.WriteString("test rax, rax\n")
			if in.Op == ir.OpJumpFalse {
				b.WriteString("jnz ")
			} else {
				b.WriteString("jz ")
			}
			b.WriteString(contLabel)
			b.WriteString("\n")
			if useSlotRegs {
				emitNativeLocalFlush(&b, slotRegs)
			}
			renderSideExit(&b, fmt.Sprintf("side_%d", i), target)
			b.WriteString(contLabel)
			b.WriteString(":\n")
			continue
		case ir.OpReturn:
			popRAX(&b)
			if useSlotRegs {
				emitNativeLocalFlush(&b, slotRegs)
			}
			b.WriteString("mov qword ptr [rdi + ")
			b.WriteString(imm(C.ds_jit_frame_sp_off))
			b.WriteString("], rcx\n")
			b.WriteString("mov qword ptr [rdi + ")
			b.WriteString(imm(C.ds_jit_frame_ret_off))
			b.WriteString("], rax\n")
			b.WriteString("mov qword ptr [rdi + ")
			b.WriteString(imm(C.ds_jit_frame_exit_kind_off))
			b.WriteString("], 1\n")
			b.WriteString("ret\n")
		default:
			return "", false, errNativeUnsupported
		}
	}

	return b.String(), true, nil
}

func nativeLocalRegisters(t Trace) (map[int]string, bool) {
	counts := make(map[int]int)
	order := make([]int, 0, 4)
	for _, in := range t.Code {
		if in.Op != ir.OpLoad && in.Op != ir.OpStore {
			continue
		}
		if in.A != 0 {
			return nil, false
		}
		slot := int(in.B)
		if _, ok := counts[slot]; !ok {
			order = append(order, slot)
		}
		counts[slot]++
	}
	if len(counts) == 0 {
		return map[int]string{}, true
	}
	first := order[0]
	second := -1
	for _, slot := range order[1:] {
		if counts[slot] > counts[first] {
			second = first
			first = slot
			continue
		}
		if second == -1 || counts[slot] > counts[second] {
			second = slot
		}
	}
	regs := map[int]string{first: "r8"}
	if second >= 0 {
		regs[second] = "r9"
	}
	return regs, true
}

func emitNativeLocalLoads(b *strings.Builder, regs map[int]string) {
	for slot, reg := range regs {
		b.WriteString("mov ")
		b.WriteString(reg)
		b.WriteString(", qword ptr [r10 + ")
		b.WriteString(imm(int64(slot * 8)))
		b.WriteString("]\n")
	}
}

func emitNativeLocalFlush(b *strings.Builder, regs map[int]string) {
	for slot, reg := range regs {
		b.WriteString("mov qword ptr [r10 + ")
		b.WriteString(imm(int64(slot * 8)))
		b.WriteString("], ")
		b.WriteString(reg)
		b.WriteString("\n")
	}
}

func renderArith(b *strings.Builder, op int64) error {
	popRAX(b)
	b.WriteString("dec rcx\n")
	b.WriteString("mov rdx, qword ptr [r11 + rcx*8]\n")
	switch op {
	case ir.ArithAdd:
		b.WriteString("add rax, rdx\n")
	case ir.ArithSub:
		b.WriteString("sub rdx, rax\n")
		b.WriteString("mov rax, rdx\n")
	case ir.ArithMul:
		b.WriteString("imul rax, rdx\n")
	case ir.ArithDiv:
		b.WriteString("mov r8, rax\n")
		b.WriteString("mov rax, rdx\n")
		b.WriteString("cqo\n")
		b.WriteString("idiv r8\n")
	case ir.ArithMod:
		b.WriteString("mov r8, rax\n")
		b.WriteString("mov rax, rdx\n")
		b.WriteString("cqo\n")
		b.WriteString("idiv r8\n")
		b.WriteString("mov rax, rdx\n")
	default:
		return errNativeUnsupported
	}
	pushRAX(b)
	return nil
}

func renderCompare(b *strings.Builder, in ir.Instr) error {
	popRAX(b)
	b.WriteString("dec rcx\n")
	b.WriteString("mov rdx, qword ptr [r11 + rcx*8]\n")
	b.WriteString("cmp rdx, rax\n")
	switch in.Op {
	case ir.OpEq:
		b.WriteString("sete al\n")
	case ir.OpNe:
		b.WriteString("setne al\n")
	case ir.OpCmp:
		switch in.A {
		case 0:
			b.WriteString("setl al\n")
		case 1:
			b.WriteString("setle al\n")
		case 2:
			b.WriteString("setg al\n")
		case 3:
			b.WriteString("setge al\n")
		default:
			return errNativeUnsupported
		}
	default:
		return errNativeUnsupported
	}
	b.WriteString("movzx rax, al\n")
	pushRAX(b)
	return nil
}

func renderSideExit(b *strings.Builder, label string, target int) {
	b.WriteString(label)
	b.WriteString(":\n")
	b.WriteString("mov qword ptr [rdi + ")
	b.WriteString(imm(C.ds_jit_frame_sp_off))
	b.WriteString("], rcx\n")
	b.WriteString("mov qword ptr [rdi + ")
	b.WriteString(imm(C.ds_jit_frame_next_ip_off))
	b.WriteString("], ")
	b.WriteString(imm(int64(target)))
	b.WriteString("\n")
	b.WriteString("mov qword ptr [rdi + ")
	b.WriteString(imm(C.ds_jit_frame_exit_kind_off))
	b.WriteString("], 0\n")
	b.WriteString("ret\n")
}

// Keystone's Intel parser uses hexadecimal radix for bare numeric operands.
// Emit an explicit 0x prefix so offsets, constants, and bytecode targets keep
// their intended values (e.g. 32 must encode as 0x20, not 0x32).
func imm(v int64) string {
	if v < 0 {
		return fmt.Sprintf("-0x%x", uint64(-v))
	}
	return fmt.Sprintf("0x%x", uint64(v))
}

func pushRAX(b *strings.Builder) {
	b.WriteString("mov qword ptr [r11 + rcx*8], rax\n")
	b.WriteString("inc rcx\n")
}

func popRAX(b *strings.Builder) {
	b.WriteString("dec rcx\n")
	b.WriteString("mov rax, qword ptr [r11 + rcx*8]\n")
}

func constValue64(v runtime.Value) int64 {
	switch v.Kind() {
	case runtime.KindInt:
		return v.IntValue()
	case runtime.KindBool:
		if v.BoolValue() {
			return 1
		}
		return 0
	default:
		return 0
	}
}

func assembleKeystoneAMD64(src string) ([]byte, error) {
	var ks *C.ks_engine
	if err := C.ks_open(C.KS_ARCH_X86, C.KS_MODE_64, &ks); err != C.KS_ERR_OK {
		return nil, fmt.Errorf("keystone open: %s", C.GoString(C.ks_strerror(C.ks_err(err))))
	}
	defer C.ks_close(ks)

	if err := C.ks_option(ks, C.KS_OPT_SYNTAX, C.KS_OPT_SYNTAX_INTEL); err != C.KS_ERR_OK {
		return nil, fmt.Errorf("keystone syntax: %s", C.GoString(C.ks_strerror(C.ks_err(err))))
	}

	csrc := C.CString(src)
	defer C.free(unsafe.Pointer(csrc))

	var encoding *C.uchar
	var size C.size_t
	var count C.size_t
	if err := C.ks_asm(ks, csrc, 0, &encoding, &size, &count); err != C.KS_ERR_OK {
		return nil, fmt.Errorf("keystone asm: %s", C.GoString(C.ks_strerror(C.ks_err(err))))
	}
	defer C.ks_free(encoding)

	return C.GoBytes(unsafe.Pointer(encoding), C.int(size)), nil
}

func makeExecutable(code []byte) ([]byte, error) {
	if len(code) == 0 {
		return nil, errNativeUnsupported
	}
	page := syscall.Getpagesize()
	size := (len(code) + page - 1) & ^(page - 1)
	buf, err := syscall.Mmap(-1, 0, size, syscall.PROT_READ|syscall.PROT_WRITE, syscall.MAP_PRIVATE|syscall.MAP_ANON)
	if err != nil {
		return nil, err
	}
	copy(buf, code)
	if err := syscall.Mprotect(buf, syscall.PROT_READ|syscall.PROT_EXEC); err != nil {
		_ = syscall.Munmap(buf)
		return nil, err
	}
	return buf, nil
}
