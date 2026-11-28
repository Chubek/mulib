package dns

import (
	"fmt"
	"net"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__dns_"+name, fn)
	}

	reg("resolve", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("resolve expects a :String hostname")
		}
		addrs, err := net.LookupHost(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(addrs))
		for i, a := range addrs {
			elems[i] = runtime.Str(a)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("resolve_a", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("resolve_a expects a :String hostname")
		}
		ips, err := net.LookupIP(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		var v4 []runtime.Value
		for _, ip := range ips {
			if ip4 := ip.To4(); ip4 != nil {
				v4 = append(v4, runtime.Str(ip4.String()))
			}
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(v4)}), nil
	})

	reg("resolve_aaaa", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("resolve_aaaa expects a :String hostname")
		}
		ips, err := net.LookupIP(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		var v6 []runtime.Value
		for _, ip := range ips {
			if ip.To4() == nil {
				v6 = append(v6, runtime.Str(ip.String()))
			}
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(v6)}), nil
	})

	reg("resolve_mx", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("resolve_mx expects a :String hostname")
		}
		mxs, err := net.LookupMX(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(mxs))
		for i, mx := range mxs {
			t := runtime.NewTable()
			t.Set(runtime.Str("host"), runtime.Str(mx.Host))
			t.Set(runtime.Str("pref"), runtime.Int(int64(mx.Pref)))
			elems[i] = runtime.TableValue(t)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("resolve_ns", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("resolve_ns expects a :String hostname")
		}
		nss, err := net.LookupNS(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(nss))
		for i, ns := range nss {
			elems[i] = runtime.Str(ns.Host)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("resolve_cname", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("resolve_cname expects a :String hostname")
		}
		cname, err := net.LookupCNAME(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(cname)}), nil
	})

	reg("resolve_txt", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("resolve_txt expects a :String hostname")
		}
		txts, err := net.LookupTXT(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(txts))
		for i, t := range txts {
			elems[i] = runtime.Str(t)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("reverse_lookup", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("reverse_lookup expects a :String IP address")
		}
		names, err := net.LookupAddr(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(names))
		for i, n := range names {
			elems[i] = runtime.Str(n)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})
}
