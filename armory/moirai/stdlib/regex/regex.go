package regex

import (
	"fmt"
	"regexp"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__regex_"+name, fn)
	}

	reg("compile", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("compile expects a :String pattern")
		}
		re, err := regexp.Compile(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		t := runtime.NewTable()
		t.Set(runtime.Str("__re__"), runtime.Str(re.String()))
		t.Set(runtime.Str("__flags__"), runtime.Str(""))
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("match", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("match expects a compiled regex :Table and a :String")
		}
		reV, ok := args[0].Table().Get(runtime.Str("__re__"))
		if !ok || reV.Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("malformed regex")
		}
		re, err := regexp.Compile(reV.StrValue())
		if err != nil {
			return runtime.Unit(), err
		}
		if re.MatchString(args[1].StrValue()) {
			return runtime.TaggedOf("Some", []runtime.Value{runtime.Str(args[1].StrValue())}), nil
		}
		return runtime.TaggedOf("None", nil), nil
	})

	reg("search", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("search expects a compiled regex :Table and a :String")
		}
		reV, ok := args[0].Table().Get(runtime.Str("__re__"))
		if !ok || reV.Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("malformed regex")
		}
		re, err := regexp.Compile(reV.StrValue())
		if err != nil {
			return runtime.Unit(), err
		}
		m := re.FindStringSubmatch(args[1].StrValue())
		if m == nil {
			return runtime.TaggedOf("None", nil), nil
		}
		elems := make([]runtime.Value, len(m))
		for i, s := range m {
			elems[i] = runtime.Str(s)
		}
		return runtime.TaggedOf("Some", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("find_all", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("find_all expects a compiled regex :Table and a :String")
		}
		reV, ok := args[0].Table().Get(runtime.Str("__re__"))
		if !ok || reV.Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("malformed regex")
		}
		re, err := regexp.Compile(reV.StrValue())
		if err != nil {
			return runtime.Unit(), err
		}
		matches := re.FindAllString(args[1].StrValue(), -1)
		elems := make([]runtime.Value, len(matches))
		for i, m := range matches {
			elems[i] = runtime.Str(m)
		}
		return runtime.ListOf(elems), nil
	})

	reg("replace", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 4 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindString || args[2].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("replace expects a compiled regex :Table, :String s, and :String replacement")
		}
		reV, ok := args[0].Table().Get(runtime.Str("__re__"))
		if !ok || reV.Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("malformed regex")
		}
		re, err := regexp.Compile(reV.StrValue())
		if err != nil {
			return runtime.Unit(), err
		}
		return runtime.Str(re.ReplaceAllString(args[1].StrValue(), args[2].StrValue())), nil
	})

	reg("split", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("split expects a compiled regex :Table and a :String")
		}
		reV, ok := args[0].Table().Get(runtime.Str("__re__"))
		if !ok || reV.Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("malformed regex")
		}
		re, err := regexp.Compile(reV.StrValue())
		if err != nil {
			return runtime.Unit(), err
		}
		parts := re.Split(args[1].StrValue(), -1)
		elems := make([]runtime.Value, len(parts))
		for i, p := range parts {
			elems[i] = runtime.Str(p)
		}
		return runtime.ListOf(elems), nil
	})

	reg("escape", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("escape expects a :String")
		}
		return runtime.Str(regexp.QuoteMeta(args[0].StrValue())), nil
	})
}
