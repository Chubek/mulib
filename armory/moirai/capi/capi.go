package main

/*
#include <stdlib.h>
#include <stdint.h>

typedef struct DomscriptState DomscriptState;
typedef uintptr_t DomscriptRef;

typedef enum {
	DOMSCRIPT_OK = 0,
	DOMSCRIPT_ERR = 1
} DomscriptStatus;

typedef enum {
	DOMSCRIPT_UNIT = 0, DOMSCRIPT_BOOL, DOMSCRIPT_INT, DOMSCRIPT_FLOAT,
	DOMSCRIPT_STRING, DOMSCRIPT_TAG, DOMSCRIPT_LIST, DOMSCRIPT_TABLE,
	DOMSCRIPT_FN, DOMSCRIPT_IOTA, DOMSCRIPT_NODE, DOMSCRIPT_SELECTOR
} DomscriptType;

typedef enum {
	DOMSCRIPT_ELEMENT = 0, DOMSCRIPT_TEXT, DOMSCRIPT_COMMENT,
	DOMSCRIPT_DOCUMENT, DOMSCRIPT_FRAGMENT
} DomscriptNodeKind;

typedef DomscriptStatus (*DomscriptCFn)(DomscriptState *st);

static DomscriptStatus domscript_call_cfn(DomscriptCFn fn, DomscriptState *st) {
	if (fn == NULL) return DOMSCRIPT_ERR;
	return fn(st);
}
*/
import "C"
import (
	"fmt"
	"sync"
	"unsafe"

	domapi "domscript/api"
	"domscript/internal/runtime"
	"domscript/stdlib"
)

// main is required when this package is built with -buildmode=c-shared.
// The shared library is initialized through its exported C ABI functions.
func main() {}

type cState struct {
	st    *runtime.State
	stack []runtime.Value
	sp    int
	id    uintptr
	mu    sync.Mutex
}

var (
	states   = map[uintptr]*cState{}
	statesMu sync.Mutex
)

func newCState() *cState {
	cs := &cState{st: domapi.Open(), stack: make([]runtime.Value, 0, 64)}
	statesMu.Lock()
	// Key by the state's own address: DomscriptOpen hands this pointer
	// to C, and every other entry point resolves it through getState.
	// (A sequential counter was used here before, which getState could
	// never resolve, so all C API calls failed.)
	key := uintptr(unsafe.Pointer(cs))
	states[key] = cs
	statesMu.Unlock()
	cs.id = key
	return cs
}

func (cs *cState) push(v runtime.Value) {
	if cs.sp < len(cs.stack) {
		cs.stack[cs.sp] = v
	} else {
		cs.stack = append(cs.stack, v)
	}
	cs.sp++
}

func (cs *cState) pop() runtime.Value {
	if cs.sp == 0 {
		return runtime.Unit()
	}
	cs.sp--
	return cs.stack[cs.sp]
}

func (cs *cState) top() runtime.Value {
	if cs.sp == 0 {
		return runtime.Unit()
	}
	return cs.stack[cs.sp-1]
}

func (cs *cState) at(ref C.DomscriptRef) (runtime.Value, bool) {
	idx := int(ref)
	if idx < 0 || idx >= cs.sp {
		return runtime.Unit(), false
	}
	return cs.stack[idx], true
}

func getState(p *C.DomscriptState) *cState {
	statesMu.Lock()
	defer statesMu.Unlock()
	return states[uintptr(unsafe.Pointer(p))]
}

//export DomscriptOpen
func DomscriptOpen() *C.DomscriptState {
	cs := newCState()
	return (*C.DomscriptState)(unsafe.Pointer(cs))
}

//export DomscriptEnableXST
func DomscriptEnableXST(st *C.DomscriptState, enabled C.int) {
	cs := getState(st)
	if cs == nil {
		return
	}
	cs.st.XST = enabled != 0
}

//export DomscriptClose
func DomscriptClose(st *C.DomscriptState) {
	cs := getState(st)
	if cs == nil {
		return
	}
	cs.st.Close()
	statesMu.Lock()
	delete(states, uintptr(unsafe.Pointer(st)))
	statesMu.Unlock()
}

//export DomscriptBindDocument
func DomscriptBindDocument(st *C.DomscriptState, hostNode unsafe.Pointer, kind C.DomscriptNodeKind) {
	cs := getState(st)
	if cs == nil {
		return
	}
	domapi.BindDocument(cs.st, domapi.WrapNode(cs.st, uintptr(hostNode), runtime.NodeKind(kind)))
}

//export DomscriptEval
func DomscriptEval(st *C.DomscriptState, src *C.char, chunkName *C.char) C.DomscriptStatus {
	cs := getState(st)
	if cs == nil {
		return C.DOMSCRIPT_ERR
	}
	v, err := domapi.Eval(cs.st, C.GoString(src), C.GoString(chunkName))
	if err != nil {
		cs.push(runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}))
		return C.DOMSCRIPT_ERR
	}
	cs.push(runtime.Value(v))
	return C.DOMSCRIPT_OK
}

//export DomscriptPump
func DomscriptPump(st *C.DomscriptState) C.DomscriptStatus {
	cs := getState(st)
	if cs == nil {
		return C.DOMSCRIPT_ERR
	}
	if err := domapi.Pump(cs.st); err != nil {
		return C.DOMSCRIPT_ERR
	}
	return C.DOMSCRIPT_OK
}

//export DomscriptTop
func DomscriptTop(st *C.DomscriptState) C.DomscriptRef {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	return C.DomscriptRef(cs.sp - 1)
}

//export DomscriptPop
func DomscriptPop(st *C.DomscriptState) {
	cs := getState(st)
	if cs != nil {
		cs.pop()
	}
}

//export DomscriptDup
func DomscriptDup(st *C.DomscriptState) {
	cs := getState(st)
	if cs != nil {
		cs.push(cs.top())
	}
}

//export DomscriptTypeOf
func DomscriptTypeOf(st *C.DomscriptState, ref C.DomscriptRef) C.DomscriptType {
	cs := getState(st)
	if cs == nil {
		return C.DOMSCRIPT_UNIT
	}
	v, ok := cs.at(ref)
	if !ok {
		return C.DOMSCRIPT_UNIT
	}
	return C.DomscriptType(v.Kind())
}

func pushHelper(st *C.DomscriptState, v runtime.Value) {
	cs := getState(st)
	if cs != nil {
		cs.push(v)
	}
}

//export DomscriptPush_unit
func DomscriptPush_unit(st *C.DomscriptState) { pushHelper(st, runtime.Unit()) }

//export DomscriptPush_bool
func DomscriptPush_bool(st *C.DomscriptState, b C.int) { pushHelper(st, runtime.Bool(b != 0)) }

//export DomscriptPush_int
func DomscriptPush_int(st *C.DomscriptState, n C.int64_t) { pushHelper(st, runtime.Int(int64(n))) }

//export DomscriptPush_float
func DomscriptPush_float(st *C.DomscriptState, f C.double) { pushHelper(st, runtime.Float(float64(f))) }

//export DomscriptPush_string
func DomscriptPush_string(st *C.DomscriptState, s *C.char) {
	pushHelper(st, runtime.Str(C.GoString(s)))
}

//export DomscriptPush_tag
func DomscriptPush_tag(st *C.DomscriptState, s *C.char) { pushHelper(st, runtime.Tag(C.GoString(s))) }

//export DomscriptPush_list
func DomscriptPush_list(st *C.DomscriptState, n C.int) {
	cs := getState(st)
	if cs == nil {
		return
	}
	count := int(n)
	elems := make([]runtime.Value, count)
	for i := count - 1; i >= 0; i-- {
		elems[i] = cs.pop()
	}
	cs.push(runtime.ListOf(elems))
}

//export DomscriptPush_tagged
func DomscriptPush_tagged(st *C.DomscriptState, tag *C.char, n C.int) {
	cs := getState(st)
	if cs == nil {
		return
	}
	count := int(n)
	elems := make([]runtime.Value, count)
	for i := count - 1; i >= 0; i-- {
		elems[i] = cs.pop()
	}
	cs.push(runtime.TaggedOf(C.GoString(tag), elems))
}

//export DomscriptPush_table
func DomscriptPush_table(st *C.DomscriptState) { pushHelper(st, runtime.TableOf(nil)) }

//export DomscriptPush_selector
func DomscriptPush_selector(st *C.DomscriptState, src *C.char) {
	cs := getState(st)
	if cs == nil {
		return
	}
	sel, err := runtime.CompileSelector(C.GoString(src))
	if err != nil {
		cs.push(runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}))
		return
	}
	cs.push(runtime.SelectorOf(sel))
}

//export DomscriptPush_node
func DomscriptPush_node(st *C.DomscriptState, hostNode unsafe.Pointer, kind C.DomscriptNodeKind) {
	cs := getState(st)
	if cs == nil {
		return
	}
	v := cs.st.WrapExternal(uintptr(hostNode), runtime.NodeKind(kind))
	cs.push(v)
}

// Accessors

//export DomscriptAs_int
func DomscriptAs_int(st *C.DomscriptState, ref C.DomscriptRef) C.int64_t {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	v, ok := cs.at(ref)
	if !ok || v.Kind() != runtime.KindInt {
		return 0
	}
	return C.int64_t(v.IntValue())
}

//export DomscriptAs_float
func DomscriptAs_float(st *C.DomscriptState, ref C.DomscriptRef) C.double {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	v, ok := cs.at(ref)
	if !ok {
		return 0
	}
	return C.double(v.Numeric())
}

//export DomscriptAs_bool
func DomscriptAs_bool(st *C.DomscriptState, ref C.DomscriptRef) C.int {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	v, ok := cs.at(ref)
	if !ok || v.Kind() != runtime.KindBool {
		return 0
	}
	if v.BoolValue() {
		return 1
	}
	return 0
}

//export DomscriptAs_string
func DomscriptAs_string(st *C.DomscriptState, ref C.DomscriptRef) *C.char {
	cs := getState(st)
	if cs == nil {
		return nil
	}
	v, ok := cs.at(ref)
	if !ok || v.Kind() != runtime.KindString {
		return nil
	}
	return C.CString(v.StrValue())
}

//export DomscriptLen
func DomscriptLen(st *C.DomscriptState, ref C.DomscriptRef) C.int {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	v, ok := cs.at(ref)
	if !ok {
		return 0
	}
	return C.int(v.Len())
}

//export DomscriptList_get
func DomscriptList_get(st *C.DomscriptState, ref C.DomscriptRef, idx C.int) C.DomscriptRef {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	v, ok := cs.at(ref)
	if !ok || v.Kind() != runtime.KindList {
		return 0
	}
	l := v.List().Elems
	i := int(idx)
	if i < 0 || i >= len(l) {
		cs.push(runtime.TaggedOf("None", nil))
	} else {
		cs.push(runtime.TaggedOf("Some", []runtime.Value{l[i]}))
	}
	return C.DomscriptRef(cs.sp - 1)
}

//export DomscriptTable_get
func DomscriptTable_get(st *C.DomscriptState, ref C.DomscriptRef, keyRef C.DomscriptRef) C.DomscriptRef {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	v, ok := cs.at(ref)
	if !ok || v.Kind() != runtime.KindTable {
		return 0
	}
	key, ok := cs.at(keyRef)
	if !ok {
		return 0
	}
	val, found := v.Table().Get(key)
	if !found {
		cs.push(runtime.TaggedOf("None", nil))
	} else {
		cs.push(runtime.TaggedOf("Some", []runtime.Value{val}))
	}
	return C.DomscriptRef(cs.sp - 1)
}

//export DomscriptTable_set
func DomscriptTable_set(st *C.DomscriptState, ref C.DomscriptRef, keyRef C.DomscriptRef, valRef C.DomscriptRef) {
	cs := getState(st)
	if cs == nil {
		return
	}
	v, ok := cs.at(ref)
	if !ok || v.Kind() != runtime.KindTable {
		return
	}
	key, ok := cs.at(keyRef)
	if !ok {
		return
	}
	val, ok := cs.at(valRef)
	if !ok {
		return
	}
	v.Table().Set(key, val)
}

//export DomscriptTag_of
func DomscriptTag_of(st *C.DomscriptState, ref C.DomscriptRef) C.DomscriptRef {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	v, ok := cs.at(ref)
	if !ok {
		return 0
	}
	tag := v.ListTag()
	if tag != "" {
		cs.push(runtime.TaggedOf("Some", []runtime.Value{runtime.Tag(tag)}))
	} else {
		cs.push(runtime.TaggedOf("None", nil))
	}
	return C.DomscriptRef(cs.sp - 1)
}

//export DomscriptPayload_of
func DomscriptPayload_of(st *C.DomscriptState, ref C.DomscriptRef) C.DomscriptRef {
	cs := getState(st)
	if cs == nil {
		return 0
	}
	v, ok := cs.at(ref)
	if !ok {
		return 0
	}
	if v.Kind() == runtime.KindList {
		cs.push(runtime.ListOf(v.List().Elems))
	} else {
		cs.push(runtime.EmptyList())
	}
	return C.DomscriptRef(cs.sp - 1)
}

// Native functions and events

//export DomscriptRegister
func DomscriptRegister(st *C.DomscriptState, name *C.char, fn C.DomscriptCFn) {
	cs := getState(st)
	if cs == nil {
		return
	}
	goName := C.GoString(name)
	domapi.RegisterNative(cs.st, goName, func(s *runtime.State, args []domapi.Value) (domapi.Value, error) {
		base := cs.sp
		for _, a := range args {
			cs.push(runtime.Value(a))
		}
		status := C.domscript_call_cfn(fn, (*C.DomscriptState)(unsafe.Pointer(cs)))
		if status != C.DOMSCRIPT_OK {
			cs.sp = base
			return domapi.Unit(), fmt.Errorf("C native %s returned an error", goName)
		}
		if cs.sp > base {
			result := cs.pop()
			cs.sp = base
			return domapi.Value(result), nil
		}
		cs.sp = base
		return domapi.Unit(), nil
	})
}

//export DomscriptRegisterStdlib
func DomscriptRegisterStdlib(st *C.DomscriptState) {
	cs := getState(st)
	if cs == nil {
		return
	}
	stdlib.RegisterAll(cs.st)
}

//export DomscriptAddModuleRoot
func DomscriptAddModuleRoot(path *C.char) {
	stdlib.AddModuleRoot(C.GoString(path))
}

//export DomscriptRegisterAsync
func DomscriptRegisterAsync(st *C.DomscriptState, name *C.char, start C.DomscriptCFn) {
	cs := getState(st)
	if cs == nil {
		return
	}
	_ = cs
	_ = start
	_ = name
}

//export DomscriptResolveAsync
func DomscriptResolveAsync(st *C.DomscriptState, token C.uint64_t) {
	cs := getState(st)
	if cs == nil {
		return
	}
	result := cs.pop()
	domapi.ResolveAsync(cs.st, uint64(token), domapi.Value(result), nil)
}

//export DomscriptCall
func DomscriptCall(st *C.DomscriptState, fnRef C.DomscriptRef, nargs C.int) C.DomscriptStatus {
	cs := getState(st)
	if cs == nil {
		return C.DOMSCRIPT_ERR
	}
	fnV, ok := cs.at(fnRef)
	if !ok {
		return C.DOMSCRIPT_ERR
	}
	args := make([]domapi.Value, int(nargs))
	for i := int(nargs) - 1; i >= 0; i-- {
		args[i] = domapi.Value(cs.pop())
	}
	v, err := domapi.Call(cs.st, domapi.Value(fnV), args...)
	if err != nil {
		cs.push(runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}))
		return C.DOMSCRIPT_ERR
	}
	cs.push(runtime.Value(v))
	return C.DOMSCRIPT_OK
}

//export DomscriptDispatchEvent
func DomscriptDispatchEvent(st *C.DomscriptState, nodeRef C.DomscriptRef, event *C.char, eventTableRef C.DomscriptRef) C.DomscriptStatus {
	cs := getState(st)
	if cs == nil {
		return C.DOMSCRIPT_ERR
	}
	nodeV, ok := cs.at(nodeRef)
	if !ok {
		return C.DOMSCRIPT_ERR
	}
	etV, ok := cs.at(eventTableRef)
	if !ok {
		return C.DOMSCRIPT_ERR
	}
	if err := cs.st.DispatchEvent(nodeV, C.GoString(event), etV); err != nil {
		return C.DOMSCRIPT_ERR
	}
	return C.DOMSCRIPT_OK
}

//export DomscriptError
func DomscriptError(st *C.DomscriptState, msg *C.char) {
	cs := getState(st)
	if cs == nil {
		return
	}
	cs.push(runtime.TaggedOf("Error", []runtime.Value{runtime.Str(C.GoString(msg))}))
}
