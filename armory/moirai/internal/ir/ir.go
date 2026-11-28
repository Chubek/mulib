// Package ir defines the bytecode Domscript lowers to — the shared
// interface between the baseline interpreter (internal/exec) and the
// tracing JIT (internal/jit). The instruction set is a fixed-width stack
// machine: every instruction is one opcode plus up to three integer
// operands, which keeps decoding a table index and lets the JIT map
// operands onto machine registers directly.
//
// Environments. One activation owns one Env of NLocals slots; block scopes
// are flattened at compile time, so a binding is (activation depth, slot).
// Closures capture the defining Env pointer, which keeps depth arithmetic
// correct across captures. The top-level chunk's activation uses the
// state's global Env, so top-level bindings persist across chunks
// (Section 17).
//
// Errors. Runtime failures thread as Go errors out of the VM loop; the
// loop unwinds its explicit frame stack to the nearest OpEffectBegin
// handler rather than using panics, because a parked coroutine must be
// resumable with its frames intact (Section 10.1).
package ir

import "domscript/internal/runtime"

// Op is one bytecode operation.
type Op uint8

const (
	OpConst       Op = iota // push Consts[A]
	OpLoad                  // push env A hops up, slot B
	OpStore                 // pop into env A hops up, slot B

	OpPop                   // drop top
	OpDup                   // copy top

	OpArith                 // A: Arith* ordinal; pop r, l
	OpNeg                   // numeric negate
	OpNot                   // :Bool negate
	OpTestBool              // top must be :Bool, else raise
	OpEq                    // Equals (Section 4.3)
	OpNe                    // !Equals
	OpCmp                   // A: 0=lt 1=le 2=gt 3=ge; numbers cross, strings
	OpSome                  // wrap :Some[v]

	OpJump                  // ip = A
	OpJumpFalse             // pop; :Bool false (or non-:Bool raise) -> ip = A
	OpJumpTrue              // pop; :Bool true  (or non-:Bool raise) -> ip = A

	OpMakeList              // pop A elems, push List
	OpMakeTagged            // pop A elems, push Tagged List tagged Consts[B]
	OpNewTable              // push empty Table
	OpTableSet              // pop v, k; set into Table beneath
	OpMakeSelector          // push compiled selector for Consts[A] (cached)
	OpMakeRegex             // push compiled regex for Consts[A] (catchable)
	OpInterp                // pop A parts, push interpolated String (Display each)

	OpIndex                 // pop i, x; push x[i]
	OpSetIndex              // pop v, i, x; x[i] = v
	OpField                 // A: name const; pop x; push x.:name (all-or-raise)
	OpSetField              // A: name const; pop v, x; set

	OpRange                 // pop b, a; push Iota range List [a, b)

	OpMakeFn                // push closure over Funcs[A] capturing this Env

	OpCall                  // pop A args, then fn; call
	OpReturn                // pop value, return from activation

	OpEffectBegin           // A: catch ip, B: result slot — open handler
	OpEffectEnd             // A: result slot — wrap, store, push :Success
	OpEffectCatch           // A: result slot — push :Error from pending raise

	OpMatch                 // A: pattern pool idx, B: fail ip; binds slots

	OpForInit               // pop iterable, push iterator (frame aux stack)
	OpForNext               // A: slot, B: end ip; bind next or jump B (pops iter)
	OpPopIter               // pop one iterator (break out of a loop)

	OpNodeEl                // A: tag const; push fresh element (node frame)
	OpNodeText              // A: text const; push fresh text node
	OpNodeComment           // A: text const; push fresh comment node
	OpNodeFragment          // push fresh fragment (node frame)
	OpNodeEnd               // pop the node frame opened by El/Fragment
	OpNodeSplice            // pop v; append to node beneath (Node or String)
	OpNodeSpread            // pop list; append each to node beneath
	OpNodeAttr              // A: name const; pop v; set attribute

	OpSubscribe             // A: event const; pop fn, target; push :Subscription(s)

	OpQuery                 // pop sel, node; push Query list

	OpRaise                 // A: message const; raise (catchable by ~)

	OpLine                  // A: source line (diagnostics only; no-op)
)

// Arith ordinals for OpArith's A operand (stable: the JIT matches on them).
const (
	ArithAdd = 0
	ArithSub = 1
	ArithMul = 2
	ArithDiv = 3
	ArithMod = 4
)

var opNames = map[Op]string{
	OpConst: "const", OpLoad: "load", OpStore: "store",
	OpPop: "pop", OpDup: "dup",
	OpArith: "arith", OpNeg: "neg", OpNot: "not", OpTestBool: "testbool",
	OpEq: "eq", OpNe: "ne", OpCmp: "cmp", OpSome: "some",
	OpJump: "jump", OpJumpFalse: "jumpf", OpJumpTrue: "jumpt",
	OpMakeList: "mklist", OpMakeTagged: "mktag", OpNewTable: "mktable",
	OpTableSet: "tset", OpMakeSelector: "mksel", OpMakeRegex: "mkregex",
	OpInterp: "interp",
	OpIndex: "index", OpSetIndex: "setindex", OpField: "field", OpSetField: "setfield",
	OpRange: "range", OpMakeFn: "mkfn", OpCall: "call", OpReturn: "ret",
	OpEffectBegin: "effbegin", OpEffectEnd: "effend", OpEffectCatch: "effcatch",
	OpMatch: "match",
	OpForInit: "forinit", OpForNext: "fornext", OpPopIter: "popiter",
	OpNodeEl: "nodel", OpNodeText: "nodet", OpNodeComment: "nodec",
	OpNodeFragment: "nodefrag", OpNodeEnd: "nodeend",
	OpNodeSplice: "splice", OpNodeSpread: "spread",
	OpNodeAttr:   "nodeattr",
	OpSubscribe: "subscribe", OpQuery: "query", OpRaise: "raise", OpLine: "line",
}

func (o Op) String() string {
	if n, ok := opNames[o]; ok {
		return n
	}
	return "?"
}

// Instr is one instruction. A/B/C are generic integer operands whose
// meaning is op-specific (see the Op constants).
type Instr struct {
	Op Op
	A  int64
	B  int64
	C  int64
}

// Func is one compiled activation: a chunk, a function literal, or an
// `on` handler body.
type Func struct {
	Name    string
	Params  []string
	Rest    bool
	NLocals int // params + flattened block locals

	Code   []Instr
	Consts []runtime.Value
	Funcs  []*Func    // nested closures
	Pats   []*Pattern // match patterns
	Line   []int32    // per-instruction source line
}

// PatKind enumerates pattern node kinds (Section 16).
type PatKind uint8

const (
	PatWild PatKind = iota
	PatLit
	PatBind
	PatList
	PatTagged
	PatOptAny // `?`: matches :Some[anything] or :None
)

// Pattern is one compiled pattern. Slots refer to the matching
// activation's Env; a failed match leaves no bindings observable because
// arms are tried exclusively.
type Pattern struct {
	Kind     PatKind
	Lit      runtime.Value // PatLit
	Slot     int           // PatBind
	Elems    []*Pattern    // PatList / PatTagged
	Rest     bool          // PatList: `..rest` present; PatTagged: brackets written
	RestSlot int           // PatList: tail binding slot (-1 if none)
	Tag      string        // PatTagged
}

// Disassemble renders f for debugging (dsc --dump-ir).
func (f *Func) Disassemble() string {
	var sb []byte
	sb = appendFunc(sb, f, "")
	return string(sb)
}

func appendFunc(sb []byte, f *Func, indent string) []byte {
	sb = append(sb, indent...)
	sb = append(sb, "func "...)
	sb = append(sb, f.Name...)
	sb = append(sb, '\n')
	for i, in := range f.Code {
		sb = append(sb, indent...)
		sb = appendUint(sb, uint64(i))
		sb = append(sb, '\t')
		sb = append(sb, in.Op.String()...)
		sb = append(sb, ' ')
		sb = appendUint(sb, uint64(in.A))
		if in.B != 0 {
			sb = append(sb, ' ')
			sb = appendUint(sb, uint64(in.B))
		}
		sb = append(sb, '\n')
	}
	for _, n := range f.Funcs {
		sb = appendFunc(sb, n, indent+"  ")
	}
	return sb
}

func appendUint(sb []byte, n uint64) []byte {
	if n == 0 {
		return append(sb, '0')
	}
	var tmp [20]byte
	i := len(tmp)
	for n > 0 {
		i--
		tmp[i] = byte('0' + n%10)
		n /= 10
	}
	return append(sb, tmp[i:]...)
}
