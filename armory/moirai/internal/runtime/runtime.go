// Package runtime: runtime.go ties the runtime together: the per-state
// global environment, the builtin (prelude) environment, the event-
// subscription registry, and the pending-asynchronous-effect table.
//
// A State is NOT safe for concurrent use; distinct states are fully
// independent and may run on separate goroutines against separate
// documents (Section 18.1). The debug-mode owner check below makes
// cross-goroutine misuse fail loudly during development without costing
// anything when Debug is false.
package runtime

import (
	"bytes"
	"fmt"
	"io"
	stdruntime "runtime"
	"strconv"
	"sync/atomic"
)

// CallFn is the interpreter hook the exec layer installs so the runtime
// can invoke Domscript function values (event handlers, api.Call) without
// importing exec.
type CallFn func(fn *Fn, args []Value) (Value, error)

// Subscription is one `on` registration (Section 11): the value Off takes.
type Subscription struct {
	ID     int64
	NodeID int64
	Event  string // tag name, e.g. "click"
	Fn     *Fn
}

// PendingAsync is one outstanding asynchronous effect: the token the host
// will resolve, plus the opaque parked continuation the exec layer owns.
type PendingAsync struct {
	Token   uint64
	Runner  any // exec-owned coroutine state
	Result  Value
	Err     error
	Resumed bool
}

// State is a single logical interpreter bound to one host-owned document
// tree. Create with NewState; the host must call BindDocument before
// evaluating chunks that reference `document`.
type State struct {
	Global   *Env
	Builtins *Env

	Nodes NodeTable
	Host  Host // nil: internal stub document only

	// CallDomscript is installed by exec; event dispatch and api.Call
	// route through it.
	CallDomscript CallFn

	// Print output sink; nil discards.
	Output io.Writer

	// Subscriptions, ordered by registration for deterministic dispatch.
	subs   []*Subscription
	subSeq int64

	// Pending asynchronous effects keyed by token; resolved records the
	// resolution order Pump observes.
	pending  map[uint64]*PendingAsync
	resolved []uint64
	tokenSeq uint64

	// defaultAsync tracks the built-in asynchronous natives' outstanding
	// tokens (prelude.go) so AutoResolveDefaults can drain them.
	defaultAsync map[uint64]string

	// Name sets for sema's extra-scope seeding: prelude names are fixed
	// (SymBuiltin, not assignable), natives are host-registered overrides
	// or extensions, and everything else in Global.index is a chunk-declared
	// global (assignable, Section 17).
	preludeNames map[string]bool
	nativeNames  map[string]bool

	// Modules loaded into this state (loader cache).
	Modules map[string]any

	// Grammars and absyns declared by `grammar`/`absyn` blocks, keyed by
	// name (referenced as `:Name` tags by DeriveParser/DeriveTree/...).
	Grammars map[string]*Grammar
	Absyns   map[string]*Absyn

	// Debug enables the single-threaded owner assertion.
	Debug bool
	owner atomic.Value // string goroutine id, when Debug

	// LastError carries the most recent uncaught error from a resumed
	// coroutine (Eval/Pump surface it).
	LastError error

	// XST enables dialect and extension-block parsing.
	XST bool

	closed bool
}

// NewState creates a state with the Section 20.2 prelude bound in the
// global environment (host re-registration replaces any entry).
func NewState() *State {
	st := &State{
		Global:      &Env{},
		Builtins:    &Env{},
		Nodes:       *NewNodeTable(),
		pending:     make(map[uint64]*PendingAsync),
		Modules:     make(map[string]any),
		Grammars:    make(map[string]*Grammar),
		Absyns:      make(map[string]*Absyn),
		Output:      io.Discard,
		nativeNames: map[string]bool{},
	}
	st.installPrelude()
	// The standalone interpreter uses an internal document tree when no
	// host document is bound, so scripts can safely reference `document`.
	st.BindGlobal("document", st.CreateDocument())
	return st
}

// Close releases everything the Go collector cannot see: the node-handle
// table, event subscriptions, pinned bridge values, and pending async
// entries (gc.go documents the full rooting discipline).
func (st *State) Close() {
	st.closed = true
	st.Nodes.table = nil
	st.Nodes.byHost = nil
	st.subs = nil
	st.pending = make(map[uint64]*PendingAsync)
	st.resolved = nil
	st.Modules = nil
}

// Closed reports whether Close ran.
func (st *State) Closed() bool { return st.closed }

// enter asserts single-threaded use when Debug is set.
func (st *State) enter() {
	if !st.Debug {
		return
	}
	id := goroutineID()
	if prev, ok := st.owner.Load().(string); ok {
		if prev != id {
			panic(fmt.Sprintf("domscript: state used concurrently (owner goroutine %s, caller %s)", prev, id))
		}
		return
	}
	st.owner.Store(id)
}

func (st *State) leave() {}

func goroutineID() string {
	var buf [64]byte
	n := stdruntime.Stack(buf[:], false)
	// "goroutine 123 [running]:..."
	fields := bytes.Fields(buf[:n])
	if len(fields) >= 2 {
		return string(fields[1])
	}
	return strconv.Itoa(-1)
}

// BindGlobal defines name in the global environment.
func (st *State) BindGlobal(name string, v Value) {
	i := st.GlobalSlot(name)
	st.Global.SetSlot(i, v)
}

// GlobalSlot returns the slot index for name, allocating one if needed
// (used by the compiler for top-level bindings across chunks).
func (st *State) GlobalSlot(name string) int {
	if st.Global.index == nil {
		st.Global.index = make(map[string]int)
	}
	if i, ok := st.Global.index[name]; ok {
		return i
	}
	i := st.Global.Size()
	st.Global.index[name] = i
	return i
}

// GlobalIndex returns a copy of the global name->slot index (the compiler
// seeds a chunk's global slots from it so cross-chunk references agree).
func (st *State) GlobalIndex() map[string]int {
	out := make(map[string]int, len(st.Global.index))
	for name, slot := range st.Global.index {
		out[name] = slot
	}
	return out
}

// LookupGlobal finds a global binding by name.
func (st *State) LookupGlobal(name string) (Value, bool) {
	if st.Global.index == nil {
		return Unit(), false
	}
	i, ok := st.Global.index[name]
	if !ok {
		return Unit(), false
	}
	return st.Global.Slot(i)
}

// ------------------------------------------------------------- natives

// RegisterNative binds a synchronous native function under name,
// replacing any prelude binding with the host's version.
func (st *State) RegisterNative(name string, fn GoNative) {
	delete(st.preludeNames, name)
	st.nativeNames[name] = true
	st.BindGlobal(name, FnOf(&Fn{Name: name, Native: fn, NParams: -1}))
}

// RegisterAsyncNative binds an asynchronous native under name,
// replacing any prelude binding with the host's version.
func (st *State) RegisterAsyncNative(name string, start GoAsyncStart) {
	delete(st.preludeNames, name)
	st.dropDefaultAsync(name)
	st.nativeNames[name] = true
	st.BindGlobal(name, FnOf(&Fn{Name: name, NativeAsync: start, NParams: -1}))
}

// dropDefaultAsync discards this name's outstanding default tokens so a
// host-registered replacement is not resolved by AutoResolveDefaults.
func (st *State) dropDefaultAsync(name string) {
	for tok, n := range st.defaultAsync {
		if n == name {
			delete(st.defaultAsync, tok)
		}
	}
}

// RegisteredNames lists names a script may reference beyond its own
// declarations: the prelude plus host-registered natives.
func (st *State) RegisteredNames() []string {
	seen := make(map[string]bool, len(st.nativeNames)+len(st.preludeNames))
	var names []string
	for n := range st.preludeNames {
		if !seen[n] {
			seen[n] = true
			names = append(names, n)
		}
	}
	for n := range st.nativeNames {
		if !seen[n] {
			seen[n] = true
			names = append(names, n)
		}
	}
	return names
}

// ChunkGlobalNames lists top-level bindings earlier chunks created —
// assignable globals, unlike prelude/natives (Section 17). Host bindings
// such as `document` also appear here: the spec treats them as ordinary
// global state a script may capture.
func (st *State) ChunkGlobalNames() []string {
	var names []string
	if st.Global.index == nil {
		return names
	}
	for n := range st.Global.index {
		if !st.preludeNames[n] && !st.nativeNames[n] {
			names = append(names, n)
		}
	}
	return names
}

// InstallGlobals records the chunk compiler's name->slot map into the
// global frame with the exact slot numbers the compiler assigned, so
// later chunks and host lookups agree (Section 17).
func (st *State) InstallGlobals(slots map[string]int) {
	if st.Global.index == nil {
		st.Global.index = make(map[string]int, len(slots))
	}
	max := st.Global.Size()
	for name, slot := range slots {
		if prev, ok := st.Global.index[name]; ok && prev != slot {
			// The compiler seeded from GlobalIndex, so a mismatch means a
			// later binding reused a name with a fresh slot; keep the
			// compiler's numbering and re-point the name.
			_ = prev
		}
		st.Global.index[name] = slot
		if slot >= max {
			max = slot + 1
		}
	}
	st.Global.Grow(max)
}

// ------------------------------------------------------------- async

// NewAsyncToken allocates a token for an asynchronous native's operation.
func (st *State) NewAsyncToken() uint64 {
	st.tokenSeq++
	return st.tokenSeq
}

// ParkAsync records a suspended coroutine waiting on token.
func (st *State) ParkAsync(token uint64, runner any) {
	st.pending[token] = &PendingAsync{Token: token, Runner: runner}
}

// ResolveAsync completes the operation for token: result/err become the
// :Success/:Error the suspended `~` observes on the next Pump.
func (st *State) ResolveAsync(token uint64, result Value, err error) {
	st.enter()
	defer st.leave()
	p, ok := st.pending[token]
	if !ok || p.Resumed {
		return
	}
	p.Result = result
	p.Err = err
	st.resolved = append(st.resolved, token)
}

// TakeResolved drains resolved tokens in resolution order (deterministic
// resume order per Section 10.1).
func (st *State) TakeResolved() []uint64 {
	out := st.resolved
	st.resolved = nil
	return out
}

// PendingCount reports unresolved asynchronous effects.
func (st *State) PendingCount() int { return len(st.pending) }

// PopPending removes one pending entry for token (the exec layer's Pump,
// after TakeResolved handed it the token). Resumed marks it consumed so a
// late ResolveAsync is a no-op.
func (st *State) PopPending(token uint64) *PendingAsync {
	p, ok := st.pending[token]
	if !ok {
		return nil
	}
	p.Resumed = true
	delete(st.pending, token)
	return p
}

// ------------------------------------------------------------ events

// Subscribe registers an `on` handler; returns the :Subscription value.
func (st *State) Subscribe(nodeV Value, event string, fn *Fn) (Value, error) {
	e, err := st.nodeEntry(nodeV)
	if err != nil {
		return Unit(), err
	}
	st.subSeq++
	sub := &Subscription{ID: st.subSeq, NodeID: e.ID, Event: event, Fn: fn}
	st.subs = append(st.subs, sub)
	return TaggedOf("Subscription", []Value{nodeV, Tag(event), FnOf(fn)}), nil
}

// Off detaches a subscription.
func (st *State) Off(subV Value) error {
	if !vIsSubscription(subV) {
		return fmt.Errorf("Off expects a :Subscription")
	}
	payload := subV.ref.(*List).Elems
	if len(payload) != 3 || payload[1].kind != KindTag {
		return fmt.Errorf("malformed :Subscription")
	}
	nodeID := nodeID(payload[0])
	event := payload[1].s
	for i, s := range st.subs {
		if s.NodeID == nodeID && s.Event == event {
			st.subs = append(st.subs[:i], st.subs[i+1:]...)
			return nil
		}
	}
	return nil
}

func vIsSubscription(v Value) bool {
	l, ok := v.ref.(*List)
	return ok && l.Tag == "Subscription" && len(l.Elems) == 3
}

// DispatchEvent invokes handlers registered for (node, event) with the
// host-supplied event Table. Handlers run under normal call semantics:
// they may suspend, and errors propagate to the caller (the host).
func (st *State) DispatchEvent(nodeV Value, event string, eventTable Value) error {
	st.enter()
	defer st.leave()
	e, err := st.nodeEntry(nodeV)
	if err != nil {
		return err
	}
	// Snapshot: a handler may subscribe/unsubscribe during dispatch.
	targets := make([]*Subscription, 0, 2)
	for _, s := range st.subs {
		if s.NodeID == e.ID && s.Event == event {
			targets = append(targets, s)
		}
	}
	for _, s := range targets {
		if st.CallDomscript == nil {
			return fmt.Errorf("no interpreter installed to run event handlers")
		}
		_, err := st.CallDomscript(s.Fn, []Value{eventTable})
		if err != nil {
			return err
		}
	}
	return nil
}

// Emit synthesizes and dispatches an event (Section 11).
func (st *State) Emit(nodeV Value, tag string, detail Value) error {
	t := NewTable()
	if err := t.Set(Tag("type"), Tag(tag)); err != nil {
		return err
	}
	if err := t.Set(Tag("target"), nodeV); err != nil {
		return err
	}
	if err := t.Set(Tag("detail"), detail); err != nil {
		return err
	}
	return st.DispatchEvent(nodeV, tag, Value{kind: KindTable, ref: t})
}

// Print writes a line to the state's output sink.
func (st *State) Print(v Value) {
	if st.Output == nil {
		return
	}
	fmt.Fprintln(st.Output, Display(v))
}

// ErrorValue is a Go error carrying a Domscript :Error value.
type ErrorValue struct {
	Msg string
}

func (e *ErrorValue) Error() string { return e.Msg }
