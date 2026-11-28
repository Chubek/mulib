package heap

import (
	"fmt"
	"sort"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

type minHeap struct {
	elems []runtime.Value
	less  func(a, b runtime.Value) bool
}

func (h *minHeap) Len() int           { return len(h.elems) }
func (h *minHeap) Less(i, j int) bool { return h.less(h.elems[i], h.elems[j]) }
func (h *minHeap) Swap(i, j int)      { h.elems[i], h.elems[j] = h.elems[j], h.elems[i] }
func (h *minHeap) Push(x any)         { h.elems = append(h.elems, x.(runtime.Value)) }
func (h *minHeap) Pop() any {
	n := len(h.elems)
	v := h.elems[n-1]
	h.elems = h.elems[:n-1]
	return v
}

func defaultLess(a, b runtime.Value) bool {
	if (a.Kind() == runtime.KindInt || a.Kind() == runtime.KindFloat) &&
		(b.Kind() == runtime.KindInt || b.Kind() == runtime.KindFloat) {
		return a.Numeric() < b.Numeric()
	}
	return runtime.Display(a) < runtime.Display(b)
}

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__heap_"+name, fn)
	}

	reg("new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		t := runtime.NewTable()
		h := &minHeap{less: defaultLess}
		t.Set(runtime.Str("__heap__"), runtime.ListOf(nil))
		t.Set(runtime.Str("__less__"), runtime.Unit())
		_ = h
		return runtime.TableValue(t), nil
	})

	reg("push", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("push expects a :Table heap and a value")
		}
		t := args[0].Table()
		dataV, ok := t.Get(runtime.Str("__heap__"))
		if !ok || dataV.Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("malformed heap")
		}
		l := dataV.List()
		l.Elems = append(l.Elems, args[1])
		t.Set(runtime.Str("__heap__"), runtime.ListOf(l.Elems))
		return args[0], nil
	})

	reg("pop", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("pop expects a :Table heap")
		}
		t := args[0].Table()
		dataV, ok := t.Get(runtime.Str("__heap__"))
		if !ok || dataV.Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("malformed heap")
		}
		l := dataV.List()
		if len(l.Elems) == 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		// Find the minimum element
		minIdx := 0
		for i := 1; i < len(l.Elems); i++ {
			if defaultLess(l.Elems[i], l.Elems[minIdx]) {
				minIdx = i
			}
		}
		v := l.Elems[minIdx]
		l.Elems = append(l.Elems[:minIdx], l.Elems[minIdx+1:]...)
		t.Set(runtime.Str("__heap__"), runtime.ListOf(l.Elems))
		return runtime.TaggedOf("Some", []runtime.Value{v}), nil
	})

	reg("peek", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("peek expects a :Table heap")
		}
		t := args[0].Table()
		dataV, ok := t.Get(runtime.Str("__heap__"))
		if !ok || dataV.Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("malformed heap")
		}
		l := dataV.List()
		if len(l.Elems) == 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		minIdx := 0
		for i := 1; i < len(l.Elems); i++ {
			if defaultLess(l.Elems[i], l.Elems[minIdx]) {
				minIdx = i
			}
		}
		return runtime.TaggedOf("Some", []runtime.Value{l.Elems[minIdx]}), nil
	})

	reg("len", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("len expects a :Table heap")
		}
		t := args[0].Table()
		dataV, ok := t.Get(runtime.Str("__heap__"))
		if !ok || dataV.Kind() != runtime.KindList {
			return runtime.Int(0), nil
		}
		return runtime.Int(int64(len(dataV.List().Elems))), nil
	})

	reg("heapify", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("heapify expects a :List")
		}
		elems := args[0].List().Elems
		sort.Slice(elems, func(i, j int) bool {
			return defaultLess(elems[i], elems[j])
		})
		return runtime.ListOf(elems), nil
	})

	reg("replace", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("replace expects a :Table heap and a value")
		}
		t := args[0].Table()
		dataV, ok := t.Get(runtime.Str("__heap__"))
		if !ok || dataV.Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("malformed heap")
		}
		l := dataV.List()
		if len(l.Elems) == 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		minIdx := 0
		for i := 1; i < len(l.Elems); i++ {
			if defaultLess(l.Elems[i], l.Elems[minIdx]) {
				minIdx = i
			}
		}
		old := l.Elems[minIdx]
		l.Elems[minIdx] = args[1]
		t.Set(runtime.Str("__heap__"), runtime.ListOf(l.Elems))
		return runtime.TaggedOf("Some", []runtime.Value{old}), nil
	})
}
