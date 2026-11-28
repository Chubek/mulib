package collections

import (
	"fmt"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__collections_"+name, fn)
	}

	reg("deque_new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.ListOf(nil), nil
	})

	reg("deque_push_left", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("deque_push_left expects a :List deque and a value")
		}
		l := args[0].List()
		l.Elems = append([]runtime.Value{args[1]}, l.Elems...)
		return args[0], nil
	})

	reg("deque_push_right", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("deque_push_right expects a :List deque and a value")
		}
		l := args[0].List()
		l.Elems = append(l.Elems, args[1])
		return args[0], nil
	})

	reg("deque_pop_left", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("deque_pop_left expects a :List deque")
		}
		l := args[0].List()
		if len(l.Elems) == 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		v := l.Elems[0]
		l.Elems = l.Elems[1:]
		return runtime.TaggedOf("Some", []runtime.Value{v}), nil
	})

	reg("deque_pop_right", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("deque_pop_right expects a :List deque")
		}
		l := args[0].List()
		n := len(l.Elems)
		if n == 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		v := l.Elems[n-1]
		l.Elems = l.Elems[:n-1]
		return runtime.TaggedOf("Some", []runtime.Value{v}), nil
	})

	reg("deque_len", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("deque_len expects a :List deque")
		}
		return runtime.Int(int64(len(args[0].List().Elems))), nil
	})

	reg("counter_new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.TableOf(nil), nil
	})

	reg("counter_add", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("counter_add expects a :Table counter and a key")
		}
		t := args[0].Table()
		cur, ok := t.Get(args[1])
		n := int64(1)
		if ok && cur.Kind() == runtime.KindInt {
			n = cur.IntValue() + 1
		}
		t.Set(args[1], runtime.Int(n))
		return args[0], nil
	})

	reg("counter_get", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("counter_get expects a :Table counter and a key")
		}
		t := args[0].Table()
		cur, ok := t.Get(args[1])
		if !ok || cur.Kind() != runtime.KindInt {
			return runtime.Int(0), nil
		}
		return cur, nil
	})

	reg("counter_most_common", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("counter_most_common expects a :Table counter")
		}
		t := args[0].Table()
		type pair struct {
			k runtime.Value
			v int64
		}
		var pairs []pair
		t.Entries(func(k, v runtime.Value) {
			if v.Kind() == runtime.KindInt {
				pairs = append(pairs, pair{k, v.IntValue()})
			}
		})
		for i := 0; i < len(pairs); i++ {
			for j := i + 1; j < len(pairs); j++ {
				if pairs[j].v > pairs[i].v {
					pairs[i], pairs[j] = pairs[j], pairs[i]
				}
			}
		}
		n := len(pairs)
		if len(args) > 1 && args[1].Kind() == runtime.KindInt {
			if lim := int(args[1].IntValue()); lim < n {
				n = lim
			}
		}
		elems := make([]runtime.Value, n)
		for i := 0; i < n; i++ {
			elems[i] = runtime.ListOf([]runtime.Value{pairs[i].k, runtime.Int(pairs[i].v)})
		}
		return runtime.ListOf(elems), nil
	})

	reg("default_dict_new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("default_dict_new expects a default value")
		}
		t := runtime.NewTable()
		t.Set(runtime.Str("__default__"), args[0])
		t.Set(runtime.Str("__data__"), runtime.TableOf(nil))
		return runtime.TableValue(t), nil
	})

	reg("default_dict_get", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("default_dict_get expects a :Table dd and a key")
		}
		t := args[0].Table()
		dataV, _ := t.Get(runtime.Str("__data__"))
		if dataV.Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("malformed default_dict")
		}
		data := dataV.Table()
		v, ok := data.Get(args[1])
		if ok {
			return v, nil
		}
		def, _ := t.Get(runtime.Str("__default__"))
		return def, nil
	})

	reg("default_dict_set", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 3 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("default_dict_set expects a :Table dd, key, and value")
		}
		t := args[0].Table()
		dataV, _ := t.Get(runtime.Str("__data__"))
		if dataV.Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("malformed default_dict")
		}
		data := dataV.Table()
		data.Set(args[1], args[2])
		return runtime.Unit(), nil
	})
}
