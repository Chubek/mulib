package url

import (
	"fmt"
	nurl "net/url"
	"strings"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__url_"+name, fn)
	}

	reg("parse", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("parse expects a :String URL")
		}
		u, err := nurl.Parse(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{urlToTable(u)}), nil
	})

	reg("encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("encode expects a :String")
		}
		return runtime.Str(nurl.QueryEscape(args[0].StrValue())), nil
	})

	reg("decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("decode expects a :String")
		}
		s, err := nurl.QueryUnescape(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(s)}), nil
	})

	reg("join", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("join expects :String base and :String ref")
		}
		base, err := nurl.Parse(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		ref, err := nurl.Parse(args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(base.ResolveReference(ref).String())}), nil
	})

	reg("query_encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("query_encode expects a :Table")
		}
		vals := nurl.Values{}
		args[0].Table().Entries(func(k, v runtime.Value) {
			vals.Set(runtime.Display(k), runtime.Display(v))
		})
		return runtime.Str(vals.Encode()), nil
	})

	reg("query_decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("query_decode expects a :String")
		}
		vals, err := nurl.ParseQuery(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		t := runtime.NewTable()
		for k, vs := range vals {
			t.Set(runtime.Str(k), runtime.Str(strings.Join(vs, ",")))
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("build", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("build expects a :Table with URL parts")
		}
		t := args[0].Table()
		u := &nurl.URL{}
		if scheme, ok := t.Get(runtime.Str("scheme")); ok && scheme.Kind() == runtime.KindString {
			u.Scheme = scheme.StrValue()
		}
		if host, ok := t.Get(runtime.Str("host")); ok && host.Kind() == runtime.KindString {
			u.Host = host.StrValue()
		}
		if path, ok := t.Get(runtime.Str("path")); ok && path.Kind() == runtime.KindString {
			u.Path = path.StrValue()
		}
		if query, ok := t.Get(runtime.Str("query")); ok && query.Kind() == runtime.KindString {
			u.RawQuery = query.StrValue()
		}
		if fragment, ok := t.Get(runtime.Str("fragment")); ok && fragment.Kind() == runtime.KindString {
			u.Fragment = fragment.StrValue()
		}
		return runtime.Str(u.String()), nil
	})
}

func urlToTable(u *nurl.URL) runtime.Value {
	t := runtime.NewTable()
	t.Set(runtime.Str("scheme"), runtime.Str(u.Scheme))
	t.Set(runtime.Str("host"), runtime.Str(u.Host))
	t.Set(runtime.Str("path"), runtime.Str(u.Path))
	t.Set(runtime.Str("query"), runtime.Str(u.RawQuery))
	t.Set(runtime.Str("fragment"), runtime.Str(u.Fragment))
	if u.User != nil {
		t.Set(runtime.Str("user"), runtime.Str(u.User.Username()))
	}
	return runtime.TableValue(t)
}
