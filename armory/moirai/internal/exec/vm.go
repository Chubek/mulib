// Package exec: vm.go — the baseline stack-based virtual machine.
//
// The VM executes compiled ir.Func bytecode one instruction at a time.
// Values flow on a single operand stack; each activation owns a local Env
// frame. Closures capture the defining Env, so references to outer-scope
// bindings walk the env chain. The VM is the reference evaluator: with
// the JIT disabled, every observable behavior of a Domscript program must
// come from this loop.
//
// Error handling: runtime failures thread as Go errors out of the VM
// loop. The loop unwinds the explicit frame stack to the nearest
// OpEffectBegin handler, restoring the operand stack to the depth saved
// at the handler, so a suspended coroutine can be resumed with its
// frames intact (Section 10.1).
package exec

import (
	"fmt"
	"math"
	"strings"
	"unsafe"

	"domscript/internal/ir"
	"domscript/internal/jit"
	"domscript/internal/runtime"
)

// activation is one call frame: the compiled Func, the Env for its
// locals, and the saved interpreter state for resumption.
type activation struct {
	fn  *ir.Func
	env *runtime.Env
	ip  int
}

// VM runs one chunk activation (or a call) against a state.
type VM struct {
	st       *runtime.State
	jit      *jit.Compiler
	jitHeads map[*ir.Func]map[int]struct{}

	// Coroutine frames: the outermost activation is the chunk, calls
	// stack on top. The bottom of this stack is the coroutine's root.
	frames []*activation

	// Operand stack: one value stack shared across calls (caller pushes
	// args, callee pops them). Grow as needed.
	stack []runtime.Value
	sp    int // next free slot

	// error is the pending raise (set by OpRaise, OpTestBool fail, etc.).
	// unwinding pops frames until an OpEffectBegin handler.
	raiseErr error

	// for-iterator auxiliary stack: each entry holds the iterator state
	// (list index, list, table key index, etc.) for one active for loop.
	iters []iterState

	// node stack: during node-literal evaluation, the "current node"
	// being built. Each NodeEl/NodeFragment pushes a node, and
	// NodeSplice/NodeSpread/NodeAttr operate on the top.
	nodeStack []nodeFrame

	nativeLocals []int64
	nativeStack  []int64
}

type vmReturn struct{ value runtime.Value }

func (*vmReturn) Error() string { return "vm: return" }

type iterState struct {
	kind     iterKind
	list     []runtime.Value // List elements
	keys     []runtime.Value // Table keys
	idx      int
	node     runtime.Value // :Node being iterated
	nodeKids []runtime.Value
	owner    *activation // effect-handler owner
}

type iterKind uint8

const (
	iterList iterKind = iota
	iterTable
	iterNode
	iterRange
)

type nodeFrame struct {
	node runtime.Value
}

// NewVM creates a VM for a state. The chunk activation is pushed by the
// caller (exec.go / VMRun).
func NewVM(st *runtime.State) *VM {
	return &VM{
		st:       st,
		jit:      compilerFor(st),
		jitHeads: make(map[*ir.Func]map[int]struct{}),
	}
}

// Run executes the current activation until it returns (or suspends on an
// async effect). Returns the result value and any error.
func (vm *VM) Run() (result runtime.Value, err error) {
	defer func() {
		if r := recover(); r != nil {
			if e, ok := r.(*vmPanic); ok {
				err = e.err
				result = runtime.Unit()
				return
			}
			panic(r)
		}
	}()
	for {
		if len(vm.frames) == 0 {
			return runtime.Unit(), fmt.Errorf("vm: empty frame stack")
		}
		act := vm.frames[len(vm.frames)-1]
		if act.ip >= len(act.fn.Code) {
			return runtime.Unit(), fmt.Errorf("vm: ip out of range in %s", act.fn.Name)
		}
		if vm.jit != nil && vm.isJITHead(act.fn, act.ip) {
			if trace := vm.jit.Lookup(act.fn, act.ip); trace != nil && trace.Trace.CanExecute() {
				ran, traceErr := vm.runTrace(trace, act)
				if ran {
					if traceErr != nil {
						if ret, ok := traceErr.(*vmReturn); ok {
							return ret.value, nil
						}
						if vm.handleError(traceErr) {
							continue
						}
						return runtime.Unit(), traceErr
					}
					continue
				}
			}
			if vm.jit.Notify(act.fn, act.ip) {
				_, _ = vm.jit.Compile(act.fn, act.ip)
			}
		}
		in := act.fn.Code[act.ip]
		act.ip++
		err := vm.execSafe(in, act)
		if err != nil {
			if ret, ok := err.(*vmReturn); ok {
				return ret.value, nil
			}
			if vm.handleError(err) {
				continue
			}
			return runtime.Unit(), err
		}
	}
}

func (vm *VM) isJITHead(fn *ir.Func, ip int) bool {
	heads, ok := vm.jitHeads[fn]
	if !ok {
		heads = make(map[int]struct{})
		for _, head := range jit.TraceHeads(fn) {
			heads[head] = struct{}{}
		}
		vm.jitHeads[fn] = heads
	}
	_, ok = heads[ip]
	return ok
}

func (vm *VM) runTrace(compiled *jit.CompiledTrace, act *activation) (bool, error) {
	trace := &compiled.Trace
	if len(trace.Code) == 0 || len(trace.Code) != len(trace.IPs) || act.ip != trace.Entry {
		return false, nil
	}
	if ran, err := vm.runNativeTrace(compiled, act); ran || err != nil {
		return ran, err
	}
	for i, in := range trace.Code {
		ip := trace.IPs[i]
		if i == 0 && act.ip != ip {
			return false, nil
		}
		act.ip = ip + 1
		if err := vm.execSafe(in, act); err != nil {
			return true, err
		}
		if i+1 < len(trace.IPs) && act.ip != trace.IPs[i+1] {
			return true, nil
		}
	}
	return true, nil
}

func (vm *VM) runNativeTrace(compiled *jit.CompiledTrace, act *activation) (bool, error) {
	if compiled == nil || compiled.Entry == 0 || !compiled.Trace.CanNativeExecute() {
		return false, nil
	}
	if act == nil || act.env == nil {
		return false, nil
	}
	if vm.jit != nil {
		vm.jit.RecordNativeRunAttempt()
	}

	envSize := act.env.Size()
	localCount := envSize
	if localCount == 0 {
		localCount = 1
	}
	if cap(vm.nativeLocals) < localCount {
		vm.nativeLocals = make([]int64, localCount)
	}
	locals := vm.nativeLocals[:localCount]
	for i := 0; i < envSize; i++ {
		v, ok := act.env.Slot(i)
		if !ok {
			return false, nil
		}
		switch v.Kind() {
		case runtime.KindInt:
			locals[i] = v.IntValue()
		case runtime.KindBool:
			if v.BoolValue() {
				locals[i] = 1
			}
		case runtime.KindUnit:
			locals[i] = 0
		default:
			locals[i] = 0
		}
	}

	stackCap := vm.sp + len(compiled.Trace.Code) + 8
	if stackCap < 8 {
		stackCap = 8
	}
	if cap(vm.nativeStack) < stackCap {
		vm.nativeStack = make([]int64, stackCap)
	}
	stack := vm.nativeStack[:stackCap]
	for i := 0; i < vm.sp; i++ {
		v := vm.stack[i]
		switch v.Kind() {
		case runtime.KindInt:
			stack[i] = v.IntValue()
		case runtime.KindBool:
			if v.BoolValue() {
				stack[i] = 1
			}
		case runtime.KindUnit:
			stack[i] = 0
		default:
			stack[i] = 0
		}
	}

	spOut, nextIP, exitKind, ret, ok, err := jit.CallNativeTrace(
		compiled.Entry,
		uintptr(unsafe.Pointer(&locals[0])),
		uintptr(unsafe.Pointer(&stack[0])),
		uint64(vm.sp),
	)
	if err != nil || !ok {
		return false, err
	}
	if vm.jit != nil {
		vm.jit.RecordNativeExec()
	}

	vm.sp = int(spOut)
	if vm.sp > len(stack) {
		if vm.jit != nil {
			vm.jit.RecordNativeOverflow()
		}
		return false, fmt.Errorf("jit: stack overflow after native trace")
	}
	if cap(vm.stack) < len(stack) {
		vm.stack = make([]runtime.Value, len(stack))
	}
	vm.stack = vm.stack[:len(stack)]
	for i := 0; i < vm.sp; i++ {
		vm.stack[i] = runtime.Int(stack[i])
	}
	for i := 0; i < envSize; i++ {
		act.env.SetSlot(i, runtime.Int(locals[i]))
	}

	if exitKind == 1 {
		return true, &vmReturn{value: runtime.Int(int64(ret))}
	}
	act.ip = int(nextIP)
	return true, nil
}

func (vm *VM) execSafe(in ir.Instr, act *activation) (err error) {
	defer func() {
		if r := recover(); r != nil {
			if p, ok := r.(*vmPanic); ok {
				err = p.err
				return
			}
			panic(r)
		}
	}()
	return vm.exec(in, act)
}

// SuspendInfo captures the suspended state for async resumption.
type SuspendInfo struct {
	Frames []*activation
	Stack  []runtime.Value
	SP     int
	Iters  []iterState
	Nodes  []nodeFrame
}

// Suspend captures the current VM state so the coroutine can be parked.
func (vm *VM) Suspend() *SuspendInfo {
	frames := make([]*activation, len(vm.frames))
	copy(frames, vm.frames)
	stack := make([]runtime.Value, len(vm.stack))
	copy(stack, vm.stack)
	iters := make([]iterState, len(vm.iters))
	copy(iters, vm.iters)
	nodes := make([]nodeFrame, len(vm.nodeStack))
	copy(nodes, vm.nodeStack)
	return &SuspendInfo{
		Frames: frames,
		Stack:  stack,
		SP:     vm.sp,
		Iters:  iters,
		Nodes:  nodes,
	}
}

// Resume restores the VM from a suspended state.
func (vm *VM) Resume(si *SuspendInfo) {
	vm.frames = si.Frames
	vm.stack = si.Stack
	vm.sp = si.SP
	vm.iters = si.Iters
	vm.nodeStack = si.Nodes
	vm.raiseErr = nil
}

// ---------------------------------------------------------------- stack

func (vm *VM) push(v runtime.Value) {
	if vm.sp < len(vm.stack) {
		vm.stack[vm.sp] = v
	} else {
		vm.stack = append(vm.stack, v)
	}
	vm.sp++
}

func (vm *VM) pop() runtime.Value {
	if vm.sp == 0 {
		panic(&vmPanic{fmt.Errorf("vm: stack underflow")})
	}
	vm.sp--
	return vm.stack[vm.sp]
}

func (vm *VM) top() runtime.Value {
	if vm.sp == 0 {
		panic(&vmPanic{fmt.Errorf("vm: stack underflow")})
	}
	return vm.stack[vm.sp-1]
}

func (vm *VM) peek(n int) runtime.Value {
	return vm.stack[vm.sp-1-n]
}

func (vm *VM) stackSize() int { return vm.sp }

// ---------------------------------------------------------------- exec

func (vm *VM) exec(in ir.Instr, act *activation) error {
	if vm.raiseErr != nil && in.Op != ir.OpEffectBegin && in.Op != ir.OpEffectCatch {
		// Propagate: pop frames until a handler.
		return vm.raiseErr
	}
	switch in.Op {
	case ir.OpConst:
		if in.A < 0 || in.A >= int64(len(act.fn.Consts)) {
			return fmt.Errorf("vm: const index out of range")
		}
		vm.push(act.fn.Consts[in.A])

	case ir.OpLoad:
		v, ok := vm.loadEnv(act, int(in.A), int(in.B))
		if !ok {
			return fmt.Errorf("vm: use of uninitialized variable at depth %d slot %d", in.A, in.B)
		}
		vm.push(v)

	case ir.OpStore:
		v := vm.pop()
		vm.storeEnv(act, int(in.A), int(in.B), v)

	case ir.OpPop:
		vm.pop()

	case ir.OpDup:
		vm.push(vm.top())

	case ir.OpArith:
		r := vm.pop()
		l := vm.pop()
		v, err := vm.arith(in.A, l, r)
		if err != nil {
			return err
		}
		vm.push(v)

	case ir.OpNeg:
		v := vm.pop()
		nv, err := vm.neg(v)
		if err != nil {
			return err
		}
		vm.push(nv)

	case ir.OpNot:
		v := vm.pop()
		if v.Kind() != runtime.KindBool {
			return fmt.Errorf("not requires :Bool, got %s", v.Kind())
		}
		vm.push(runtime.Bool(!v.BoolValue()))

	case ir.OpTestBool:
		v := vm.top()
		if v.Kind() != runtime.KindBool {
			return fmt.Errorf("condition must be :Bool, got %s", v.Kind())
		}

	case ir.OpEq:
		r := vm.pop()
		l := vm.pop()
		vm.push(runtime.Bool(runtime.Equals(l, r)))

	case ir.OpNe:
		r := vm.pop()
		l := vm.pop()
		vm.push(runtime.Bool(!runtime.Equals(l, r)))

	case ir.OpCmp:
		r := vm.pop()
		l := vm.pop()
		v, err := vm.cmp(in.A, l, r)
		if err != nil {
			return err
		}
		vm.push(v)

	case ir.OpSome:
		v := vm.pop()
		vm.push(runtime.TaggedOf("Some", []runtime.Value{v}))

	case ir.OpJump:
		act.ip = int(in.A)

	case ir.OpJumpFalse:
		v := vm.pop()
		if v.Kind() != runtime.KindBool {
			return fmt.Errorf("condition must be :Bool, got %s", v.Kind())
		}
		if !v.BoolValue() {
			act.ip = int(in.A)
		}

	case ir.OpJumpTrue:
		v := vm.pop()
		if v.Kind() != runtime.KindBool {
			return fmt.Errorf("condition must be :Bool, got %s", v.Kind())
		}
		if v.BoolValue() {
			act.ip = int(in.A)
		}

	case ir.OpMakeList:
		n := int(in.A)
		elems := make([]runtime.Value, n)
		for i := n - 1; i >= 0; i-- {
			elems[i] = vm.pop()
		}
		vm.push(runtime.ListOf(elems))

	case ir.OpMakeTagged:
		n := int(in.A)
		elems := make([]runtime.Value, n)
		for i := n - 1; i >= 0; i-- {
			elems[i] = vm.pop()
		}
		tag := act.fn.Consts[in.B].StrValue()
		vm.push(runtime.TaggedOf(tag, elems))

	case ir.OpNewTable:
		vm.push(runtime.Value{}) // placeholder
		// re-wrap: need a proper Table value
		vm.stack[vm.sp-1] = runtime.TableOf(nil)
		// Actually, we need a mutable table. Let's use the raw Table.
		vm.stack[vm.sp-1] = runtime.Value{}
		vm.stack[vm.sp-1] = runtime.TableOf([][2]runtime.Value{})

	case ir.OpTableSet:
		v := vm.pop()
		k := vm.pop()
		t := vm.pop()
		if t.Kind() != runtime.KindTable {
			return fmt.Errorf("table set on non-table: %s", t.Kind())
		}
		if err := t.Table().Set(k, v); err != nil {
			return err
		}
		vm.push(t)

	case ir.OpMakeSelector:
		src := act.fn.Consts[in.A].StrValue()
		sel, err := runtime.CompileSelector(src)
		if err != nil {
			return fmt.Errorf("selector %q: %v", src, err)
		}
		vm.push(runtime.SelectorOf(sel))

	case ir.OpMakeRegex:
		pat := act.fn.Consts[in.A].StrValue()
		re, err := runtime.NewRegex(pat)
		if err != nil {
			return fmt.Errorf("regex %q: %v", pat, err)
		}
		vm.push(runtime.RegexOf(re))

	case ir.OpInterp:
		n := int(in.A)
		if n < 0 || n > vm.sp {
			return fmt.Errorf("vm: interp count out of range")
		}
		parts := make([]runtime.Value, n)
		for i := n - 1; i >= 0; i-- {
			parts[i] = vm.pop()
		}
		var sb strings.Builder
		for _, pv := range parts {
			sb.WriteString(runtime.Display(pv))
		}
		vm.push(runtime.Str(sb.String()))

	case ir.OpIndex:
		i := vm.pop()
		x := vm.pop()
		v, err := vm.index(x, i)
		if err != nil {
			return err
		}
		vm.push(v)

	case ir.OpSetIndex:
		v := vm.pop()
		i := vm.pop()
		x := vm.pop()
		if err := vm.setIndex(x, i, v); err != nil {
			return err
		}

	case ir.OpField:
		name := act.fn.Consts[in.A].StrValue()
		x := vm.pop()
		v, err := vm.field(x, name)
		if err != nil {
			return err
		}
		vm.push(v)

	case ir.OpSetField:
		name := act.fn.Consts[in.A].StrValue()
		v := vm.pop()
		x := vm.pop()
		if err := vm.setField(x, name, v); err != nil {
			return err
		}

	case ir.OpRange:
		b := vm.pop()
		a := vm.pop()
		rv, err := vm.rangeOp(a, b)
		if err != nil {
			return err
		}
		vm.push(rv)

	case ir.OpMakeFn:
		idx := int(in.A)
		if idx < 0 || idx >= len(act.fn.Funcs) {
			return fmt.Errorf("vm: func index out of range")
		}
		sub := act.fn.Funcs[idx]
		fn := &runtime.Fn{
			Name:    sub.Name,
			NParams: len(sub.Params),
			Rest:    sub.Rest,
			Code:    sub,
			Env:     act.env,
		}
		vm.push(runtime.FnOf(fn))

	case ir.OpCall:
		nargs := int(in.A)
		args := make([]runtime.Value, nargs)
		for i := nargs - 1; i >= 0; i-- {
			args[i] = vm.pop()
		}
		fnV := vm.pop()
		if err := vm.call(fnV, args); err != nil {
			return err
		}

	case ir.OpReturn:
		v := vm.pop()
		vm.popFrame()
		if len(vm.frames) == 0 {
			return &vmReturn{value: v}
		}
		vm.push(v)

	case ir.OpEffectBegin:
		// Save stack depth and catch IP.
		// We'll push a handler frame marker.
		vm.pushEffectHandler(act, int(in.A), int(in.B))

	case ir.OpEffectEnd:
		// Wrap the result in :Success and pop the handler.
		slot := int(in.A)
		result := vm.pop()
		vm.popEffectHandler()
		// Store into result slot
		vm.storeEnv(act, 0, slot, runtime.TaggedOf("Success", []runtime.Value{result}))
		// Push the effect result for the follower statement
		vm.push(runtime.TaggedOf("Success", []runtime.Value{result}))

	case ir.OpEffectCatch:
		slot := int(in.A)
		// The raise error is already set; pop the handler and push the
		// tagged error. Typed errors (`*Error`) keep their tag so
		// `match ~ { :IOError[_] => ... }` works; all else is `:Error`.
		errVal := runtime.TaggedOf("Error", []runtime.Value{runtime.Str("")})
		if vm.raiseErr != nil {
			errVal = runtime.TypedErrorValue(vm.raiseErr)
		}
		vm.popEffectHandler()
		vm.storeEnv(act, 0, slot, errVal)
		vm.push(errVal)
		vm.raiseErr = nil

	case ir.OpMatch:
		patIdx := int(in.A)
		failIP := int(in.B)
		if err := vm.match(act, patIdx, failIP); err != nil {
			return err
		}

	case ir.OpForInit:
		iterable := vm.pop()
		if err := vm.forInit(iterable); err != nil {
			return err
		}

	case ir.OpForNext:
		slot := int(in.A)
		endIP := int(in.B)
		if err := vm.forNext(act, slot, endIP); err != nil {
			return err
		}

	case ir.OpPopIter:
		if len(vm.iters) == 0 {
			return fmt.Errorf("vm: PopIter with empty iterator stack")
		}
		vm.iters = vm.iters[:len(vm.iters)-1]

	case ir.OpNodeEl:
		tag := act.fn.Consts[in.A].StrValue()
		node := vm.st.CreateElement(tag)
		vm.nodeStack = append(vm.nodeStack, nodeFrame{node: node})
		vm.push(node)

	case ir.OpNodeText:
		text := act.fn.Consts[in.A].StrValue()
		node := vm.st.CreateText(text)
		vm.push(node)

	case ir.OpNodeComment:
		text := act.fn.Consts[in.A].StrValue()
		node := vm.st.CreateComment(text)
		vm.push(node)

	case ir.OpNodeFragment:
		node := vm.st.CreateFragment()
		vm.nodeStack = append(vm.nodeStack, nodeFrame{node: node})
		vm.push(node)

	case ir.OpNodeSplice:
		v := vm.pop()
		if len(vm.nodeStack) == 0 {
			return fmt.Errorf("vm: node splice without active node")
		}
		parent := vm.nodeStack[len(vm.nodeStack)-1].node
		if err := vm.nodeSplice(parent, v); err != nil {
			return err
		}

	case ir.OpNodeSpread:
		v := vm.pop()
		if len(vm.nodeStack) == 0 {
			return fmt.Errorf("vm: node spread without active node")
		}
		parent := vm.nodeStack[len(vm.nodeStack)-1].node
		if v.Kind() != runtime.KindList {
			return fmt.Errorf("spread requires a list, got %s", v.Kind())
		}
		for _, child := range v.List().Elems {
			if err := vm.nodeSplice(parent, child); err != nil {
				return err
			}
		}

	case ir.OpNodeEnd:
		if len(vm.nodeStack) == 0 {
			return fmt.Errorf("vm: node end without active node")
		}
		vm.nodeStack = vm.nodeStack[:len(vm.nodeStack)-1]

	case ir.OpNodeAttr:
		name := act.fn.Consts[in.A].StrValue()
		v := vm.pop()
		if len(vm.nodeStack) == 0 {
			return fmt.Errorf("vm: node attr without active node")
		}
		node := vm.nodeStack[len(vm.nodeStack)-1].node
		if err := vm.st.SetAttr(node, name, v); err != nil {
			return err
		}

	case ir.OpSubscribe:
		event := act.fn.Consts[in.A].StrValue()
		fnV := vm.pop()
		target := vm.pop()
		if fnV.Kind() != runtime.KindFn {
			return fmt.Errorf("on handler must be a function, got %s", fnV.Kind())
		}
		sub, err := vm.st.Subscribe(target, event, fnV.Fn())
		if err != nil {
			return err
		}
		vm.push(sub)

	case ir.OpQuery:
		selV := vm.pop()
		nodeV := vm.pop()
		if selV.Kind() != runtime.KindSelector {
			return fmt.Errorf("query requires a :Selector, got %s", selV.Kind())
		}
		result, err := vm.st.QueryAll(nodeV, selV.Selector())
		if err != nil {
			return err
		}
		vm.push(result)

	case ir.OpRaise:
		msg := act.fn.Consts[in.A].StrValue()
		return fmt.Errorf("%s", msg)

	case ir.OpLine:
		// No-op: diagnostics only.

	default:
		return fmt.Errorf("vm: unknown opcode %d", in.Op)
	}
	return nil
}

// ---------------------------------------------------------------- helpers

func (vm *VM) loadEnv(act *activation, depth, slot int) (runtime.Value, bool) {
	env := act.env
	for d := 0; d < depth; d++ {
		if env == nil {
			return runtime.Unit(), false
		}
		env = env.Walk(1) // parent
	}
	if env == nil {
		return runtime.Unit(), false
	}
	return env.Slot(slot)
}

func (vm *VM) storeEnv(act *activation, depth, slot int, v runtime.Value) {
	env := act.env
	for d := 0; d < depth; d++ {
		if env == nil {
			return
		}
		env = env.Walk(1)
	}
	if env == nil {
		return
	}
	env.SetSlot(slot, v)
}

func (vm *VM) arith(op int64, l, r runtime.Value) (runtime.Value, error) {
	lk, rk := l.Kind(), r.Kind()
	isInt := lk == runtime.KindInt && rk == runtime.KindInt
	isNum := (lk == runtime.KindInt || lk == runtime.KindFloat) &&
		(rk == runtime.KindInt || rk == runtime.KindFloat)
	if !isNum {
		// String concatenation (strings and characters coerce via Display
		// so `result += Head(reader)` and interpolation-style `"a" + #\b`
		// work without explicit conversion).
		if op == ir.ArithAdd &&
			(lk == runtime.KindString || lk == runtime.KindChar) &&
			(rk == runtime.KindString || rk == runtime.KindChar) {
			return runtime.Str(runtime.Display(l) + runtime.Display(r)), nil
		}
		return runtime.Unit(), fmt.Errorf("arithmetic on non-numeric types: %s, %s", lk, rk)
	}
	if isInt {
		a, b := l.IntValue(), r.IntValue()
		switch op {
		case ir.ArithAdd:
			return runtime.Int(a + b), nil
		case ir.ArithSub:
			return runtime.Int(a - b), nil
		case ir.ArithMul:
			return runtime.Int(a * b), nil
		case ir.ArithDiv:
			if b == 0 {
				return runtime.Unit(), fmt.Errorf("division by zero")
			}
			return runtime.Int(a / b), nil
		case ir.ArithMod:
			if b == 0 {
				return runtime.Unit(), fmt.Errorf("modulo by zero")
			}
			return runtime.Int(a % b), nil
		}
	}
	a, b := l.Numeric(), r.Numeric()
	switch op {
	case ir.ArithAdd:
		return runtime.Float(a + b), nil
	case ir.ArithSub:
		return runtime.Float(a - b), nil
	case ir.ArithMul:
		return runtime.Float(a * b), nil
	case ir.ArithDiv:
		return runtime.Float(a / b), nil
	case ir.ArithMod:
		return runtime.Float(math.Mod(a, b)), nil
	}
	return runtime.Unit(), fmt.Errorf("unknown arithmetic op %d", op)
}

func (vm *VM) neg(v runtime.Value) (runtime.Value, error) {
	switch v.Kind() {
	case runtime.KindInt:
		return runtime.Int(-v.IntValue()), nil
	case runtime.KindFloat:
		return runtime.Float(-v.FloatValue()), nil
	}
	return runtime.Unit(), fmt.Errorf("negation on non-numeric type: %s", v.Kind())
}

func (vm *VM) cmp(op int64, l, r runtime.Value) (runtime.Value, error) {
	lk, rk := l.Kind(), r.Kind()
	ln := lk == runtime.KindInt || lk == runtime.KindFloat
	rn := rk == runtime.KindInt || rk == runtime.KindFloat
	if ln && rn {
		a, b := l.Numeric(), r.Numeric()
		switch op {
		case 0:
			return runtime.Bool(a < b), nil
		case 1:
			return runtime.Bool(a <= b), nil
		case 2:
			return runtime.Bool(a > b), nil
		case 3:
			return runtime.Bool(a >= b), nil
		}
	}
	if lk == runtime.KindString && rk == runtime.KindString {
		a, b := l.StrValue(), r.StrValue()
		switch op {
		case 0:
			return runtime.Bool(a < b), nil
		case 1:
			return runtime.Bool(a <= b), nil
		case 2:
			return runtime.Bool(a > b), nil
		case 3:
			return runtime.Bool(a >= b), nil
		}
	}
	return runtime.Unit(), fmt.Errorf("comparison on incompatible types: %s, %s", lk, rk)
}

func (vm *VM) index(x, i runtime.Value) (runtime.Value, error) {
	switch x.Kind() {
	case runtime.KindList:
		if i.Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("list index must be :Int, got %s", i.Kind())
		}
		idx := int(i.IntValue())
		l := x.List()
		if idx < 0 || idx >= len(l.Elems) {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{l.Elems[idx]}), nil
	case runtime.KindString:
		if i.Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("string index must be :Int, got %s", i.Kind())
		}
		idx := int(i.IntValue())
		s := x.StrValue()
		if idx < 0 || idx >= len(s) {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{runtime.Str(string(s[idx]))}), nil
	case runtime.KindChar:
		// A character indexes as its single character (only [0] is valid).
		if i.Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("character index must be :Int, got %s", i.Kind())
		}
		if i.IntValue() != 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{x}), nil
	case runtime.KindReader:
		// `reader[n]` is lookahead: the character n runes past Pos as a
		// bare :Character (directly comparable to `#\x`), or :None at EOF.
		if i.Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("reader index must be :Int, got %s", i.Kind())
		}
		off := int(i.IntValue())
		if off < 0 {
			return runtime.Unit(), fmt.Errorf("reader index must be non-negative")
		}
		if ch, ok := x.Reader().Peek(off); ok {
			return runtime.Char(ch), nil
		}
		return runtime.TaggedOf("None", nil), nil
	case runtime.KindTable:
		v, ok := x.Table().Get(i)
		if !ok {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{v}), nil
	default:
		return runtime.Unit(), fmt.Errorf("index on non-indexable type: %s", x.Kind())
	}
}

func (vm *VM) setIndex(x, i, v runtime.Value) error {
	switch x.Kind() {
	case runtime.KindList:
		if i.Kind() != runtime.KindInt {
			return fmt.Errorf("list index must be :Int, got %s", i.Kind())
		}
		idx := int(i.IntValue())
		l := x.List()
		if idx < 0 || idx >= len(l.Elems) {
			return fmt.Errorf("list index %d out of bounds (len %d)", idx, len(l.Elems))
		}
		l.Elems[idx] = v
		return nil
	case runtime.KindTable:
		return x.Table().Set(i, v)
	default:
		return fmt.Errorf("set index on non-indexable type: %s", x.Kind())
	}
}

func (vm *VM) field(x runtime.Value, name string) (runtime.Value, error) {
	switch x.Kind() {
	case runtime.KindTable:
		v, ok := x.Table().Get(runtime.Tag(name))
		if !ok {
			v, ok = x.Table().Get(runtime.Str(name))
		}
		if !ok {
			return runtime.Unit(), fmt.Errorf("field %q not found", name)
		}
		return v, nil
	case runtime.KindReader:
		// `reader.advance` is a bound method (see `Advance(reader)`).
		if name == "advance" {
			r := x.Reader()
			return runtime.FnOf(&runtime.Fn{
				Name:    "advance",
				NParams: 0,
				Native: func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
					r.Advance()
					return runtime.Unit(), nil
				},
			}), nil
		}
		if name == "head" {
			r := x.Reader()
			return runtime.FnOf(&runtime.Fn{
				Name:    "head",
				NParams: 0,
				Native: func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
					if ch, ok := r.Head(); ok {
						return runtime.Char(ch), nil
					}
					return runtime.TaggedOf("None", nil), nil
				},
			}), nil
		}
		return runtime.Unit(), fmt.Errorf("field %q not found on :Reader", name)
	case runtime.KindNode:
		v, err := vm.st.GetAttr(x, name)
		if err != nil {
			return runtime.Unit(), err
		}
		if v.List().Tag == "None" {
			return runtime.Unit(), fmt.Errorf("attribute %q not found", name)
		}
		return v.List().Elems[0], nil
	default:
		return runtime.Unit(), fmt.Errorf("field access on %s", x.Kind())
	}
}

func (vm *VM) setField(x runtime.Value, name string, v runtime.Value) error {
	switch x.Kind() {
	case runtime.KindTable:
		return x.Table().Set(runtime.Tag(name), v)
	case runtime.KindNode:
		return vm.st.SetAttr(x, name, v)
	default:
		return fmt.Errorf("field set on %s", x.Kind())
	}
}

func (vm *VM) rangeOp(a, b runtime.Value) (runtime.Value, error) {
	if a.Kind() != runtime.KindInt || b.Kind() != runtime.KindInt {
		return runtime.Unit(), fmt.Errorf("range requires :Int operands, got %s and %s", a.Kind(), b.Kind())
	}
	start, end := a.IntValue(), b.IntValue()
	var elems []runtime.Value
	if start < end {
		for i := start; i < end; i++ {
			elems = append(elems, runtime.Int(i))
		}
	} else {
		for i := start; i > end; i-- {
			elems = append(elems, runtime.Int(i))
		}
	}
	return runtime.ListOf(elems), nil
}

func (vm *VM) call(fnV runtime.Value, args []runtime.Value) error {
	if fnV.Kind() == runtime.KindFn {
		fn := fnV.Fn()
		if fn.Native != nil {
			// Synchronous native call.
			result, err := fn.Native(vm.st, args)
			if err != nil {
				return err
			}
			vm.push(result)
			return nil
		}
		if fn.NativeAsync != nil {
			// Asynchronous native call: suspend the coroutine.
			token, err := fn.NativeAsync(vm.st, args)
			if err != nil {
				return err
			}
			return &AsyncSuspend{Token: token}
		}
		if fn.Code == nil {
			return fmt.Errorf("call to uninitialized function %q", fn.Name)
		}
		// Domscript function call.
		code, ok := fn.Code.(*ir.Func)
		if !ok || code == nil {
			return fmt.Errorf("call to uninitialized function %q", fn.Name)
		}
		env := fn.Env
		if env == nil {
			env = runtime.NewEnv(nil, code.NLocals)
		} else {
			env = runtime.NewEnv(env, code.NLocals)
		}
		// Bind parameters.
		for i := range code.Params {
			if i < len(args) {
				env.SetSlot(i, args[i])
			} else {
				env.SetSlot(i, runtime.Unit())
			}
		}
		// Rest parameter: collect remaining args.
		if code.Rest && len(code.Params) > 0 {
			restIdx := len(code.Params)
			if len(args) > len(code.Params) {
				rest := args[len(code.Params):]
				env.SetSlot(restIdx, runtime.ListOf(rest))
			} else {
				env.SetSlot(restIdx, runtime.ListOf(nil))
			}
		}
		act := &activation{fn: code, env: env, ip: 0}
		vm.frames = append(vm.frames, act)
		return nil
	}
	if fnV.Kind() == runtime.KindIota {
		return vm.callIota(args)
	}
	return fmt.Errorf("cannot call %s", fnV.Kind())
}

func (vm *VM) callIota(args []runtime.Value) error {
	switch len(args) {
	case 1:
		n := args[0]
		if n.Kind() != runtime.KindInt {
			vm.push(runtime.TaggedOf("None", nil))
			return nil
		}
		count := n.IntValue()
		if count <= 0 {
			vm.push(runtime.ListOf(nil))
			return nil
		}
		elems := make([]runtime.Value, count)
		for i := int64(0); i < count; i++ {
			elems[i] = runtime.Int(i)
		}
		vm.push(runtime.ListOf(elems))
		return nil
	case 2:
		a, b := args[0], args[1]
		if a.Kind() != runtime.KindInt || b.Kind() != runtime.KindInt {
			vm.push(runtime.TaggedOf("None", nil))
			return nil
		}
		start, end := a.IntValue(), b.IntValue()
		var elems []runtime.Value
		if start < end {
			for i := start; i < end; i++ {
				elems = append(elems, runtime.Int(i))
			}
		} else {
			for i := start; i > end; i-- {
				elems = append(elems, runtime.Int(i))
			}
		}
		vm.push(runtime.ListOf(elems))
		return nil
	case 3:
		a, b, s := args[0], args[1], args[2]
		if a.Kind() != runtime.KindInt || b.Kind() != runtime.KindInt || s.Kind() != runtime.KindInt {
			vm.push(runtime.TaggedOf("None", nil))
			return nil
		}
		start, end, step := a.IntValue(), b.IntValue(), s.IntValue()
		if step == 0 {
			vm.push(runtime.TaggedOf("None", nil))
			return nil
		}
		var elems []runtime.Value
		if step > 0 {
			for i := start; i < end; i += step {
				elems = append(elems, runtime.Int(i))
			}
		} else {
			for i := start; i > end; i += step {
				elems = append(elems, runtime.Int(i))
			}
		}
		vm.push(runtime.ListOf(elems))
		return nil
	default:
		vm.push(runtime.TaggedOf("None", nil))
		return nil
	}
}

func (vm *VM) popFrame() {
	if len(vm.frames) == 0 {
		return
	}
	vm.frames = vm.frames[:len(vm.frames)-1]
}

// ---------------------------------------------------------------- effects

type effectHandler struct {
	catchIP    int
	resultSlot int
	savedSP    int
}

func (vm *VM) pushEffectHandler(owner *activation, catchIP, resultSlot int) {
	// We store the handler on the iterator stack as a marker.
	// Actually, we need a separate mechanism. Let's use a dedicated stack.
	// For now, we encode it as a negative iterator index: the effect
	// handler is stored as a special iterState with kind > 3.
	vm.iters = append(vm.iters, iterState{
		kind:  iterKind(4), // marker for effect handler
		idx:   catchIP,
		owner: owner,
	})
	// Store the saved SP as the list length
	vm.iters[len(vm.iters)-1].list = make([]runtime.Value, vm.sp) // reuse list field for saved SP marker
	_ = resultSlot
}

// handleError unwinds to the innermost active effect handler.
func (vm *VM) handleError(err error) bool {
	for i := len(vm.iters) - 1; i >= 0; i-- {
		if vm.iters[i].kind != iterKind(4) {
			continue
		}
		h := vm.iters[i]
		// Restore the saved operand stack and unwind calls to the owner.
		saved := len(h.list)
		vm.sp = saved
		vm.raiseErr = err
		if h.owner == nil {
			return false
		}
		for len(vm.frames) > 0 && vm.frames[len(vm.frames)-1] != h.owner {
			vm.frames = vm.frames[:len(vm.frames)-1]
		}
		if len(vm.frames) == 0 {
			return false
		}
		vm.frames[len(vm.frames)-1].ip = h.idx
		return true
	}
	return false
}

func (vm *VM) popEffectHandler() {
	// Remove the effect handler marker.
	for i := len(vm.iters) - 1; i >= 0; i-- {
		if vm.iters[i].kind == 4 {
			vm.iters = vm.iters[:i]
			return
		}
	}
}

// ---------------------------------------------------------------- match

func (vm *VM) match(act *activation, patIdx, failIP int) error {
	if patIdx < 0 || patIdx >= len(act.fn.Pats) {
		return fmt.Errorf("vm: pattern index out of range")
	}
	pat := act.fn.Pats[patIdx]
	scrutinee := vm.top()
	matched, binds := vm.matchPattern(pat, scrutinee)
	if !matched {
		act.ip = failIP
		return nil
	}
	vm.pop() // discard scrutinee
	// Bind pattern variables.
	for _, b := range binds {
		vm.storeEnv(act, 0, b.slot, b.value)
	}
	return nil
}

type patternBind struct {
	slot  int
	value runtime.Value
}

func (vm *VM) matchPattern(pat *ir.Pattern, v runtime.Value) (bool, []patternBind) {
	switch pat.Kind {
	case ir.PatWild:
		return true, nil
	case ir.PatLit:
		if runtime.Equals(pat.Lit, v) {
			return true, nil
		}
		return false, nil
	case ir.PatBind:
		return true, []patternBind{{slot: pat.Slot, value: v}}
	case ir.PatOptAny:
		return true, nil
	case ir.PatList:
		if v.Kind() != runtime.KindList {
			return false, nil
		}
		l := v.List().Elems
		if !pat.Rest && len(l) != len(pat.Elems) {
			return false, nil
		}
		if pat.Rest && len(l) < len(pat.Elems) {
			return false, nil
		}
		var binds []patternBind
		for i, ep := range pat.Elems {
			ok, bs := vm.matchPattern(ep, l[i])
			if !ok {
				return false, nil
			}
			binds = append(binds, bs...)
		}
		if pat.Rest && pat.RestSlot >= 0 {
			tail := runtime.ListOf(l[len(pat.Elems):])
			binds = append(binds, patternBind{slot: pat.RestSlot, value: tail})
		}
		return true, binds
	case ir.PatTagged:
		if v.Kind() != runtime.KindList {
			return false, nil
		}
		l := v.List()
		if l.Tag != pat.Tag {
			return false, nil
		}
		if len(pat.Elems) == 0 {
			return true, nil
		}
		if len(l.Elems) < len(pat.Elems) && !pat.Rest {
			return false, nil
		}
		var binds []patternBind
		for i, ep := range pat.Elems {
			if i >= len(l.Elems) {
				return false, nil
			}
			ok, bs := vm.matchPattern(ep, l.Elems[i])
			// `?name` is commonly used as a nullable/wildcard binding
			// inside tagged payloads; accept the payload directly when it
			// is not itself an option value.
			if !ok && ep.Kind == ir.PatTagged && ep.Tag == "Some" && len(ep.Elems) == 1 {
				ok, bs = vm.matchPattern(ep.Elems[0], l.Elems[i])
			}
			if !ok {
				return false, nil
			}
			binds = append(binds, bs...)
		}
		return true, binds
	}
	return false, nil
}

// ---------------------------------------------------------------- for

func (vm *VM) forInit(iterable runtime.Value) error {
	switch iterable.Kind() {
	case runtime.KindList:
		l := iterable.List().Elems
		vm.iters = append(vm.iters, iterState{kind: iterList, list: l, idx: 0})
	case runtime.KindTable:
		keys := iterable.Table().Keys()
		vm.iters = append(vm.iters, iterState{kind: iterTable, keys: keys, idx: 0})
	case runtime.KindNode:
		kids, err := vm.st.ChildrenOf(iterable)
		if err != nil {
			return err
		}
		vm.iters = append(vm.iters, iterState{
			kind:     iterNode,
			node:     iterable,
			nodeKids: kids.List().Elems,
			idx:      0,
		})
	default:
		return fmt.Errorf("cannot iterate over %s", iterable.Kind())
	}
	return nil
}

func (vm *VM) forNext(act *activation, slot, endIP int) error {
	if len(vm.iters) == 0 {
		return fmt.Errorf("vm: ForNext with empty iterator stack")
	}
	it := &vm.iters[len(vm.iters)-1]
	switch it.kind {
	case iterList:
		if it.idx >= len(it.list) {
			vm.popIter()
			act.ip = endIP
			return nil
		}
		vm.storeEnv(act, 0, slot, it.list[it.idx])
		it.idx++
	case iterTable:
		if it.idx >= len(it.keys) {
			vm.popIter()
			act.ip = endIP
			return nil
		}
		vm.storeEnv(act, 0, slot, it.keys[it.idx])
		it.idx++
	case iterNode:
		if it.idx >= len(it.nodeKids) {
			vm.popIter()
			act.ip = endIP
			return nil
		}
		vm.storeEnv(act, 0, slot, it.nodeKids[it.idx])
		it.idx++
	}
	return nil
}

func (vm *VM) popIter() {
	if len(vm.iters) == 0 {
		return
	}
	vm.iters = vm.iters[:len(vm.iters)-1]
}

// ---------------------------------------------------------------- nodes

func (vm *VM) nodeSplice(parent, child runtime.Value) error {
	switch child.Kind() {
	case runtime.KindNode:
		return vm.st.AppendChild(parent, child)
	case runtime.KindString:
		txt := vm.st.CreateText(child.StrValue())
		return vm.st.AppendChild(parent, txt)
	default:
		return fmt.Errorf("cannot splice %s into a node", child.Kind())
	}
}

// ---------------------------------------------------------------- panic

type vmPanic struct {
	err error
}

func (e *vmPanic) Error() string { return e.err.Error() }

// AsyncSuspend signals that the coroutine should suspend for an async
// native call. The exec layer catches this and parks the coroutine.
type AsyncSuspend struct {
	Token uint64
}

func (e *AsyncSuspend) Error() string { return "async suspend" }
