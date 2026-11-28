package mapmod

import (
	"fmt"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__map_"+name, fn)
	}

	reg("new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.TableOf(nil), nil
	})

	reg("len", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("len expects a :Table")
		}
		return runtime.Int(int64(args[0].Table().Len())), nil
	})

	reg("get", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("get expects a :Table and a key")
		}
		v, ok := args[0].Table().Get(args[1])
		if !ok {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{v}), nil
	})

	reg("set", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 3 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("set expects a :Table, key, and value")
		}
		args[0].Table().Set(args[1], args[2])
		return args[0], nil
	})

	reg("has", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("has expects a :Table and a key")
		}
		_, ok := args[0].Table().Get(args[1])
		return runtime.Bool(ok), nil
	})

	reg("delete", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("delete expects a :Table and a key")
		}
		args[0].Table().Delete(args[1])
		return args[0], nil
	})

	reg("keys", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("keys expects a :Table")
		}
		var keys []runtime.Value
		args[0].Table().Entries(func(k, _ runtime.Value) {
			keys = append(keys, k)
		})
		return runtime.ListOf(keys), nil
	})

	reg("values", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("values expects a :Table")
		}
		var vals []runtime.Value
		args[0].Table().Entries(func(_, v runtime.Value) {
			vals = append(vals, v)
		})
		return runtime.ListOf(vals), nil
	})

	reg("entries", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("entries expects a :Table")
		}
		var entries []runtime.Value
		args[0].Table().Entries(func(k, v runtime.Value) {
			entries = append(entries, runtime.ListOf([]runtime.Value{k, v}))
		})
		return runtime.ListOf(entries), nil
	})

	reg("clear", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("clear expects a :Table")
		}
		args[0].Table().Clear()
		return args[0], nil
	})

	reg("for_each", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindFn {
			return runtime.Unit(), fmt.Errorf("for_each expects a :Table and a :Fn")
		}
		fn := args[1].Fn()
		var errVal error
		args[0].Table().Entries(func(k, v runtime.Value) {
			if errVal != nil {
				return
			}
			_, errVal = st.CallDomscript(fn, []runtime.Value{k, v})
		})
		if errVal != nil {
			return runtime.Unit(), errVal
		}
		return runtime.Unit(), nil
	})

	reg("merge", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("merge expects :Table arguments")
		}
		out := runtime.NewTable()
		args[0].Table().Entries(func(k, v runtime.Value) { out.Set(k, v) })
		for _, a := range args[1:] {
			if a.Kind() != runtime.KindTable {
				continue
			}
			a.Table().Entries(func(k, v runtime.Value) { out.Set(k, v) })
		}
		return runtime.TableValue(out), nil
	})

	reg("from_list", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("from_list expects a :List of [key, value] pairs")
		}
		out := runtime.NewTable()
		for _, p := range args[0].List().Elems {
			if p.Kind() == runtime.KindList {
				elems := p.List().Elems
				if len(elems) >= 2 {
					out.Set(elems[0], elems[1])
				}
			}
		}
		return runtime.TableValue(out), nil
	})
}
