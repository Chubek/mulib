package async

import (
	"fmt"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__async_"+name, fn)
	}

	reg("channel_new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		size := 0
		if len(args) > 0 && args[0].Kind() == runtime.KindInt {
			size = int(args[0].IntValue())
		}
		t := runtime.NewTable()
		t.Set(runtime.Str("__cap__"), runtime.Int(int64(size)))
		t.Set(runtime.Str("__closed__"), runtime.Bool(false))
		t.Set(runtime.Str("__buf__"), runtime.ListOf(nil))
		return runtime.TableValue(t), nil
	})

	reg("channel_send", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("channel_send expects a channel :Table and a value")
		}
		t := args[0].Table()
		closedV, _ := t.Get(runtime.Str("__closed__"))
		if closedV.Kind() == runtime.KindBool && closedV.BoolValue() {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str("channel is closed")}), nil
		}
		bufV, _ := t.Get(runtime.Str("__buf__"))
		if bufV.Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("malformed channel")
		}
		capV, _ := t.Get(runtime.Str("__cap__"))
		if capV.Kind() == runtime.KindInt && capV.IntValue() > 0 && len(bufV.List().Elems) >= int(capV.IntValue()) {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str("channel buffer full")}), nil
		}
		l := bufV.List()
		l.Elems = append(l.Elems, args[1])
		t.Set(runtime.Str("__buf__"), runtime.ListOf(l.Elems))
		return runtime.Unit(), nil
	})

	reg("channel_recv", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("channel_recv expects a channel :Table")
		}
		t := args[0].Table()
		closedV, _ := t.Get(runtime.Str("__closed__"))
		bufV, _ := t.Get(runtime.Str("__buf__"))
		if bufV.Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("malformed channel")
		}
		l := bufV.List()
		if len(l.Elems) == 0 {
			if closedV.Kind() == runtime.KindBool && closedV.BoolValue() {
				return runtime.TaggedOf("None", nil), nil
			}
			return runtime.TaggedOf("None", nil), nil
		}
		v := l.Elems[0]
		l.Elems = l.Elems[1:]
		t.Set(runtime.Str("__buf__"), runtime.ListOf(l.Elems))
		return runtime.TaggedOf("Some", []runtime.Value{v}), nil
	})

	reg("channel_close", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("channel_close expects a channel :Table")
		}
		t := args[0].Table()
		t.Set(runtime.Str("__closed__"), runtime.Bool(true))
		return runtime.Unit(), nil
	})

	reg("channel_len", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("channel_len expects a channel :Table")
		}
		t := args[0].Table()
		bufV, _ := t.Get(runtime.Str("__buf__"))
		if bufV.Kind() != runtime.KindList {
			return runtime.Int(0), nil
		}
		return runtime.Int(int64(len(bufV.List().Elems))), nil
	})

	reg("wait", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("wait expects a channel :Table")
		}
		t := args[0].Table()
		bufV, _ := t.Get(runtime.Str("__buf__"))
		if bufV.Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("malformed channel")
		}
		l := bufV.List()
		if len(l.Elems) == 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		v := l.Elems[0]
		l.Elems = l.Elems[1:]
		t.Set(runtime.Str("__buf__"), runtime.ListOf(l.Elems))
		return runtime.TaggedOf("Some", []runtime.Value{v}), nil
	})

	reg("cancel", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("cancel expects a channel :Table")
		}
		t := args[0].Table()
		t.Set(runtime.Str("__closed__"), runtime.Bool(true))
		t.Set(runtime.Str("__buf__"), runtime.ListOf(nil))
		return runtime.Unit(), nil
	})
}
