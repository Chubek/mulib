package inspect

import (
	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__inspect_"+name, fn)
	}

	reg("type_of", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), nil
		}
		return runtime.Tag(runtime.TypeName(args[0])), nil
	})

	reg("kind", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), nil
		}
		return runtime.Str(args[0].Kind().String()), nil
	})

	reg("is_bool", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindBool), nil
	})

	reg("is_int", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindInt), nil
	})

	reg("is_float", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindFloat), nil
	})

	reg("is_string", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindString), nil
	})

	reg("is_list", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindList), nil
	})

	reg("is_table", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindTable), nil
	})

	reg("is_fn", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindFn), nil
	})

	reg("is_tag", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindTag), nil
	})

	reg("is_node", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindNode), nil
	})

	reg("is_unit", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		return runtime.Bool(args[0].Kind() == runtime.KindUnit), nil
	})

	reg("is_none", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		if args[0].Kind() == runtime.KindList {
			return runtime.Bool(args[0].ListTag() == "None"), nil
		}
		return runtime.Bool(false), nil
	})

	reg("is_some", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Bool(false), nil
		}
		if args[0].Kind() == runtime.KindList {
			return runtime.Bool(args[0].ListTag() == "Some"), nil
		}
		return runtime.Bool(false), nil
	})

	reg("repr", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Str("()"), nil
		}
		return runtime.Str(runtime.Repr(args[0])), nil
	})
}
