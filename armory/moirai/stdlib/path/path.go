package path

import (
	"fmt"
	"path/filepath"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__path_"+name, fn)
	}

	reg("join", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		parts := make([]string, len(args))
		for i, a := range args {
			if a.Kind() != runtime.KindString {
				return runtime.Unit(), fmt.Errorf("join expects :String arguments, got %s", runtime.TypeName(a))
			}
			parts[i] = a.StrValue()
		}
		return runtime.Str(filepath.Join(parts...)), nil
	})

	reg("basename", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("basename expects a :String path")
		}
		return runtime.Str(filepath.Base(args[0].StrValue())), nil
	})

	reg("dirname", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("dirname expects a :String path")
		}
		return runtime.Str(filepath.Dir(args[0].StrValue())), nil
	})

	reg("ext", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("ext expects a :String path")
		}
		return runtime.Str(filepath.Ext(args[0].StrValue())), nil
	})

	reg("split", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("split expects a :String path")
		}
		dir, file := filepath.Split(args[0].StrValue())
		return runtime.ListOf([]runtime.Value{runtime.Str(dir), runtime.Str(file)}), nil
	})

	reg("is_abs", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_abs expects a :String path")
		}
		return runtime.Bool(filepath.IsAbs(args[0].StrValue())), nil
	})

	reg("abs", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("abs expects a :String path")
		}
		abs, err := filepath.Abs(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(abs)}), nil
	})

	reg("normalize", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("normalize expects a :String path")
		}
		return runtime.Str(filepath.Clean(args[0].StrValue())), nil
	})

	reg("relative", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("relative expects :String base and :String target")
		}
		rel, err := filepath.Rel(args[0].StrValue(), args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(rel)}), nil
	})

	reg("match", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("match expects :String pattern and :String name")
		}
		m, err := filepath.Match(args[0].StrValue(), args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.Bool(m), nil
	})

	reg("glob", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("glob expects a :String pattern")
		}
		matches, err := filepath.Glob(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(matches))
		for i, m := range matches {
			elems[i] = runtime.Str(m)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("volume", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("volume expects a :String path")
		}
		return runtime.Str(filepath.VolumeName(args[0].StrValue())), nil
	})
}
