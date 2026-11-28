// Package domscript is the public Go embedding API. It exposes the State
// lifecycle, value construction and inspection, native-function
// registration, and the Eval/Pump/DispatchEvent surface.
package domscript

import (
	"domscript/internal/exec"
	"domscript/internal/ffi"
	"domscript/internal/runtime"
	"domscript/stdlib"
)

// State is a single logical interpreter bound to one host-owned document
// tree. It is not safe for concurrent use, but distinct states are fully
// independent (Section 18.1).
type State = runtime.State

// Open creates a new state with the prelude bootstrap loaded.
func Open() *State {
	st := runtime.NewState()
	exec.InstallCallHook(st)
	// Embedders get the same standard-library surface as dsc.  Hosts may
	// still register additional natives after Open returns.
	stdlib.RegisterAll(st)
	return st
}

// Close releases all resources.
func Close(st *State) { st.Close() }

// EnableXST toggles extensible-syntax parsing on a state.
func EnableXST(st *runtime.State, enabled bool) {
	st.XST = enabled
}

// BindDocument installs the host root Node as the `document` global.
func BindDocument(st *runtime.State, root Value) {
	st.BindGlobal("document", runtime.Value(root))
}

// Eval compiles and evaluates src, returning the result or an error.
func Eval(st *runtime.State, src, chunkName string) (Value, error) {
	v, err := exec.EvalChunk(st, src, chunkName)
	return Value(v), err
}

// Pump resumes suspended coroutines.
func Pump(st *runtime.State) error {
	return exec.Pump(st)
}

// ------------------------------------------------------------------ types

// Kind mirrors the twelve core types.
type Kind = runtime.Kind

const (
	KindUnit     = runtime.KindUnit
	KindBool     = runtime.KindBool
	KindInt      = runtime.KindInt
	KindFloat    = runtime.KindFloat
	KindString   = runtime.KindString
	KindTag      = runtime.KindTag
	KindList     = runtime.KindList
	KindTable    = runtime.KindTable
	KindFn       = runtime.KindFn
	KindIota     = runtime.KindIota
	KindNode     = runtime.KindNode
	KindSelector = runtime.KindSelector
	KindChar     = runtime.KindChar
	KindRegex    = runtime.KindRegex
	KindGrammar  = runtime.KindGrammar
	KindAbsyn    = runtime.KindAbsyn
	KindParser   = runtime.KindParser
	KindReader   = runtime.KindReader
)

// NodeKind is the concrete DOM node kind.
type NodeKind = runtime.NodeKind

const (
	KindElement  = runtime.KindElement
	KindTextNode = runtime.KindText
	KindComment  = runtime.KindComment
	KindDocument = runtime.KindDocument
	KindFragment = runtime.KindFragment
)

// Value wraps a runtime Value.
type Value runtime.Value

func (v Value) Kind() Kind     { return runtime.Value(v).Kind() }
func (v Value) Int() int64     { return runtime.Value(v).IntValue() }
func (v Value) Float() float64 { return runtime.Value(v).FloatValue() }
func (v Value) Bool() bool     { return runtime.Value(v).BoolValue() }
func (v Value) Str() string    { return runtime.Value(v).StrValue() }
func (v Value) Len() int       { return runtime.Value(v).Len() }
func (v Value) Index(i int) (Value, bool) {
	rv := runtime.Value(v)
	if rv.Kind() != runtime.KindList {
		return Value{}, false
	}
	l := rv.List().Elems
	if i < 0 || i >= len(l) {
		return Value{}, false
	}
	return Value(l[i]), true
}
func (v Value) Get(key Value) Value {
	rv := runtime.Value(v)
	if rv.Kind() != runtime.KindTable {
		return Value(runtime.TaggedOf("None", nil))
	}
	val, ok := rv.Table().Get(runtime.Value(key))
	if !ok {
		return Value(runtime.TaggedOf("None", nil))
	}
	return Value(val)
}
func (v Value) TagOf() (string, bool) {
	rv := runtime.Value(v)
	tag := rv.ListTag()
	return tag, tag != ""
}
func (v Value) Payload() Value {
	rv := runtime.Value(v)
	if rv.Kind() == runtime.KindList && rv.ListTag() != "" {
		return Value(runtime.ListOf(rv.List().Elems))
	}
	return Value(runtime.EmptyList())
}

// ------------------------------------------------------------------ constructors

func Unit() Value           { return Value(runtime.Unit()) }
func Bool(b bool) Value     { return Value(runtime.Bool(b)) }
func Int(n int64) Value     { return Value(runtime.Int(n)) }
func Float(f float64) Value { return Value(runtime.Float(f)) }
func Str(s string) Value    { return Value(runtime.Str(s)) }
func Tag(name string) Value { return Value(runtime.Tag(name)) }

func List(elems ...Value) Value {
	rv := make([]runtime.Value, len(elems))
	for i, e := range elems {
		rv[i] = runtime.Value(e)
	}
	return Value(runtime.ListOf(rv))
}

func Table(entries ...[2]Value) Value {
	pairs := make([][2]runtime.Value, len(entries))
	for i, e := range entries {
		pairs[i] = [2]runtime.Value{runtime.Value(e[0]), runtime.Value(e[1])}
	}
	return Value(runtime.TableOf(pairs))
}

func Tagged(tag string, elems ...Value) Value {
	rv := make([]runtime.Value, len(elems))
	for i, e := range elems {
		rv[i] = runtime.Value(e)
	}
	return Value(runtime.TaggedOf(tag, rv))
}

func CompileSelector(src string) (Value, error) {
	sel, err := runtime.CompileSelector(src)
	if err != nil {
		return Value{}, err
	}
	return Value(runtime.SelectorOf(sel)), nil
}

// ------------------------------------------------------------------ nodes

func WrapNode(st *runtime.State, hostPtr uintptr, kind NodeKind) Value {
	return Value(st.WrapExternal(hostPtr, kind))
}

func CreateElement(st *runtime.State, tag string) Value {
	return Value(st.CreateElement(tag))
}

func CreateText(st *runtime.State, text string) Value {
	return Value(st.CreateText(text))
}

// ------------------------------------------------------------------ natives

type GoFn func(st *State, args []Value) (Value, error)

func RegisterNative(st *runtime.State, name string, fn GoFn) {
	ffi.Register(st, name, func(s *runtime.State, args []runtime.Value) (runtime.Value, error) {
		apiArgs := make([]Value, len(args))
		for i, a := range args {
			apiArgs[i] = Value(a)
		}
		v, err := fn(s, apiArgs)
		return runtime.Value(v), err
	})
}

func RegisterAsync(st *runtime.State, name string, start func(s *runtime.State, args []Value) (token uint64, err error)) {
	ffi.RegisterAsync(st, name, func(s *runtime.State, args []runtime.Value) (uint64, error) {
		apiArgs := make([]Value, len(args))
		for i, a := range args {
			apiArgs[i] = Value(a)
		}
		return start(s, apiArgs)
	})
}

func ResolveAsync(st *runtime.State, token uint64, result Value, err error) {
	ffi.ResolveAsync(st, token, runtime.Value(result), err)
}

func Call(st *runtime.State, fn Value, args ...Value) (Value, error) {
	rv := make([]runtime.Value, len(args))
	for i, a := range args {
		rv[i] = runtime.Value(a)
	}
	fnV := runtime.Value(fn)
	if fnV.Kind() != runtime.KindFn {
		return Value{}, &runtime.ErrorValue{Msg: "Call requires a function value"}
	}
	v, err := exec.CallFn(st, fnV.Fn(), rv)
	return Value(v), err
}

func DispatchEvent(st *runtime.State, node Value, event string, eventTable Value) error {
	return st.DispatchEvent(runtime.Value(node), event, runtime.Value(eventTable))
}
