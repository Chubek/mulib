package net

import (
	"fmt"
	"net"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__net_"+name, fn)
	}

	reg("dial", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("dial expects :String network and :String address")
		}
		conn, err := net.Dial(args[0].StrValue(), args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		t := runtime.NewTable()
		t.Set(runtime.Str("__network__"), runtime.Str(args[0].StrValue()))
		t.Set(runtime.Str("__addr__"), runtime.Str(args[1].StrValue()))
		t.Set(runtime.Str("__connected__"), runtime.Bool(true))
		_ = conn.Close()
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("listen", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("listen expects :String network and :String address")
		}
		ln, err := net.Listen(args[0].StrValue(), args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		t := runtime.NewTable()
		t.Set(runtime.Str("__network__"), runtime.Str(args[0].StrValue()))
		t.Set(runtime.Str("__addr__"), runtime.Str(args[1].StrValue()))
		t.Set(runtime.Str("__listening__"), runtime.Bool(true))
		_ = ln.Close()
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("resolve", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("resolve expects :String network and :String address")
		}
		addrs, err := net.LookupHost(args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(addrs))
		for i, a := range addrs {
			elems[i] = runtime.Str(a)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("local_addr", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		addrs, err := net.InterfaceAddrs()
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(addrs))
		for i, a := range addrs {
			elems[i] = runtime.Str(a.String())
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})
}
