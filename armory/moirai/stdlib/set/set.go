package set

import (
	"fmt"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__set_"+name, fn)
	}

	reg("new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.TableOf(nil), nil
	})

	reg("len", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("len expects a :Table set")
		}
		return runtime.Int(int64(args[0].Table().Len())), nil
	})

	reg("add", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("add expects a :Table set and a value")
		}
		args[0].Table().Set(args[1], runtime.Bool(true))
		return args[0], nil
	})

	reg("has", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("has expects a :Table set and a value")
		}
		_, ok := args[0].Table().Get(args[1])
		return runtime.Bool(ok), nil
	})

	reg("delete", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("delete expects a :Table set and a value")
		}
		args[0].Table().Delete(args[1])
		return args[0], nil
	})

	reg("clear", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("clear expects a :Table set")
		}
		args[0].Table().Clear()
		return args[0], nil
	})

	reg("values", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("values expects a :Table set")
		}
		var vals []runtime.Value
		args[0].Table().Entries(func(k, _ runtime.Value) {
			vals = append(vals, k)
		})
		return runtime.ListOf(vals), nil
	})

	reg("union", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("union expects :Table set arguments")
		}
		out := runtime.NewTable()
		args[0].Table().Entries(func(k, _ runtime.Value) { out.Set(k, runtime.Bool(true)) })
		for _, a := range args[1:] {
			if a.Kind() != runtime.KindTable {
				continue
			}
			a.Table().Entries(func(k, _ runtime.Value) { out.Set(k, runtime.Bool(true)) })
		}
		return runtime.TableValue(out), nil
	})

	reg("intersection", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("intersection expects two :Table sets")
		}
		a, b := args[0].Table(), args[1].Table()
		out := runtime.NewTable()
		a.Entries(func(k, _ runtime.Value) {
			if _, ok := b.Get(k); ok {
				out.Set(k, runtime.Bool(true))
			}
		})
		return runtime.TableValue(out), nil
	})

	reg("difference", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("difference expects two :Table sets")
		}
		a, b := args[0].Table(), args[1].Table()
		out := runtime.NewTable()
		a.Entries(func(k, _ runtime.Value) {
			if _, ok := b.Get(k); !ok {
				out.Set(k, runtime.Bool(true))
			}
		})
		return runtime.TableValue(out), nil
	})

	reg("subset", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("subset expects two :Table sets")
		}
		a, b := args[0].Table(), args[1].Table()
		allIn := true
		a.Entries(func(k, _ runtime.Value) {
			if _, ok := b.Get(k); !ok {
				allIn = false
			}
		})
		return runtime.Bool(allIn), nil
	})

	reg("superset", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("superset expects two :Table sets")
		}
		a, b := args[0].Table(), args[1].Table()
		allIn := true
		b.Entries(func(k, _ runtime.Value) {
			if _, ok := a.Get(k); !ok {
				allIn = false
			}
		})
		return runtime.Bool(allIn), nil
	})

	reg("from_list", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("from_list expects a :List")
		}
		out := runtime.NewTable()
		for _, v := range args[0].List().Elems {
			out.Set(v, runtime.Bool(true))
		}
		return runtime.TableValue(out), nil
	})
}
