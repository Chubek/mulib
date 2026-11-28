package array

import (
	"fmt"
	"sort"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__array_"+name, fn)
	}

	reg("new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		size := 0
		if len(args) > 0 && args[0].Kind() == runtime.KindInt {
			size = int(args[0].IntValue())
		}
		fill := runtime.Unit()
		if len(args) > 1 {
			fill = args[1]
		}
		elems := make([]runtime.Value, size)
		for i := range elems {
			elems[i] = fill
		}
		return runtime.ListOf(elems), nil
	})

	reg("len", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("len expects a :List")
		}
		return runtime.Int(int64(len(args[0].List().Elems))), nil
	})

	reg("get", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("get expects a :List and an :Int index")
		}
		l := args[0].List().Elems
		i := int(args[1].IntValue())
		if i < 0 || i >= len(l) {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{l[i]}), nil
	})

	reg("set", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 3 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("set expects a :List, an :Int index, and a value")
		}
		l := args[0].List()
		i := int(args[1].IntValue())
		if i < 0 || i >= len(l.Elems) {
			return runtime.Unit(), fmt.Errorf("index out of bounds: %d", i)
		}
		l.Elems[i] = args[2]
		return args[0], nil
	})

	reg("push", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("push expects a :List and a value")
		}
		l := args[0].List()
		l.Elems = append(l.Elems, args[1])
		return args[0], nil
	})

	reg("pop", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("pop expects a :List")
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

	reg("shift", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("shift expects a :List")
		}
		l := args[0].List()
		if len(l.Elems) == 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		v := l.Elems[0]
		l.Elems = l.Elems[1:]
		return runtime.TaggedOf("Some", []runtime.Value{v}), nil
	})

	reg("unshift", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("unshift expects a :List and a value")
		}
		l := args[0].List()
		l.Elems = append([]runtime.Value{args[1]}, l.Elems...)
		return args[0], nil
	})

	reg("slice", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("slice expects a :List and start index")
		}
		l := args[0].List().Elems
		start := int(args[1].IntValue())
		end := len(l)
		if len(args) > 2 && args[2].Kind() == runtime.KindInt {
			end = int(args[2].IntValue())
		}
		if start < 0 {
			start = 0
		}
		if end > len(l) {
			end = len(l)
		}
		if start > end {
			return runtime.ListOf(nil), nil
		}
		out := make([]runtime.Value, end-start)
		copy(out, l[start:end])
		return runtime.ListOf(out), nil
	})

	reg("concat", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		var elems []runtime.Value
		for _, a := range args {
			if a.Kind() == runtime.KindList {
				elems = append(elems, a.List().Elems...)
			} else {
				elems = append(elems, a)
			}
		}
		return runtime.ListOf(elems), nil
	})

	reg("map", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindFn {
			return runtime.Unit(), fmt.Errorf("map expects a :List and a :Fn")
		}
		l := args[0].List().Elems
		fn := args[1].Fn()
		out := make([]runtime.Value, len(l))
		for i, v := range l {
			res, err := st.CallDomscript(fn, []runtime.Value{v})
			if err != nil {
				return runtime.Unit(), err
			}
			out[i] = res
		}
		return runtime.ListOf(out), nil
	})

	reg("filter", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindFn {
			return runtime.Unit(), fmt.Errorf("filter expects a :List and a :Fn")
		}
		l := args[0].List().Elems
		fn := args[1].Fn()
		var out []runtime.Value
		for _, v := range l {
			res, err := st.CallDomscript(fn, []runtime.Value{v})
			if err != nil {
				return runtime.Unit(), err
			}
			if res.Kind() == runtime.KindBool && res.BoolValue() {
				out = append(out, v)
			}
		}
		return runtime.ListOf(out), nil
	})

	reg("reduce", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 3 || args[0].Kind() != runtime.KindList || args[2].Kind() != runtime.KindFn {
			return runtime.Unit(), fmt.Errorf("reduce expects a :List, an initial value, and a :Fn")
		}
		l := args[0].List().Elems
		acc := args[1]
		fn := args[2].Fn()
		for _, v := range l {
			res, err := st.CallDomscript(fn, []runtime.Value{acc, v})
			if err != nil {
				return runtime.Unit(), err
			}
			acc = res
		}
		return acc, nil
	})

	reg("find", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindFn {
			return runtime.Unit(), fmt.Errorf("find expects a :List and a :Fn")
		}
		l := args[0].List().Elems
		fn := args[1].Fn()
		for _, v := range l {
			res, err := st.CallDomscript(fn, []runtime.Value{v})
			if err != nil {
				return runtime.Unit(), err
			}
			if res.Kind() == runtime.KindBool && res.BoolValue() {
				return runtime.TaggedOf("Some", []runtime.Value{v}), nil
			}
		}
		return runtime.TaggedOf("None", nil), nil
	})

	reg("sort", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("sort expects a :List")
		}
		l := args[0].List()
		elems := make([]runtime.Value, len(l.Elems))
		copy(elems, l.Elems)
		sort.Slice(elems, func(i, j int) bool {
			// Compare by numeric value if both Int/Float, else by display string
			ai, aj := elems[i], elems[j]
			if (ai.Kind() == runtime.KindInt || ai.Kind() == runtime.KindFloat) &&
				(aj.Kind() == runtime.KindInt || aj.Kind() == runtime.KindFloat) {
				return ai.Numeric() < aj.Numeric()
			}
			return runtime.Display(ai) < runtime.Display(aj)
		})
		l.Elems = elems
		return args[0], nil
	})

	reg("reverse", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("reverse expects a :List")
		}
		l := args[0].List()
		for i, j := 0, len(l.Elems)-1; i < j; i, j = i+1, j-1 {
			l.Elems[i], l.Elems[j] = l.Elems[j], l.Elems[i]
		}
		return args[0], nil
	})

	reg("join", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("join expects a :List and a :String separator")
		}
		l := args[0].List().Elems
		sep := args[1].StrValue()
		parts := make([]string, len(l))
		for i, v := range l {
			parts[i] = runtime.Display(v)
		}
		result := ""
		for i, p := range parts {
			if i > 0 {
				result += sep
			}
			result += p
		}
		return runtime.Str(result), nil
	})

	reg("fill", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("fill expects a :List and a value")
		}
		l := args[0].List()
		for i := range l.Elems {
			l.Elems[i] = args[1]
		}
		return args[0], nil
	})

	reg("includes", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("includes expects a :List and a value")
		}
		for _, v := range args[0].List().Elems {
			if runtime.Equals(v, args[1]) {
				return runtime.Bool(true), nil
			}
		}
		return runtime.Bool(false), nil
	})

	reg("index_of", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("index_of expects a :List and a value")
		}
		for i, v := range args[0].List().Elems {
			if runtime.Equals(v, args[1]) {
				return runtime.TaggedOf("Some", []runtime.Value{runtime.Int(int64(i))}), nil
			}
		}
		return runtime.TaggedOf("None", nil), nil
	})

	reg("last_index_of", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("last_index_of expects a :List and a value")
		}
		l := args[0].List().Elems
		for i := len(l) - 1; i >= 0; i-- {
			if runtime.Equals(l[i], args[1]) {
				return runtime.TaggedOf("Some", []runtime.Value{runtime.Int(int64(i))}), nil
			}
		}
		return runtime.TaggedOf("None", nil), nil
	})

	reg("every", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindFn {
			return runtime.Unit(), fmt.Errorf("every expects a :List and a :Fn")
		}
		fn := args[1].Fn()
		for _, v := range args[0].List().Elems {
			res, err := st.CallDomscript(fn, []runtime.Value{v})
			if err != nil {
				return runtime.Unit(), err
			}
			if res.Kind() != runtime.KindBool || !res.BoolValue() {
				return runtime.Bool(false), nil
			}
		}
		return runtime.Bool(true), nil
	})

	reg("some", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindFn {
			return runtime.Unit(), fmt.Errorf("some expects a :List and a :Fn")
		}
		fn := args[1].Fn()
		for _, v := range args[0].List().Elems {
			res, err := st.CallDomscript(fn, []runtime.Value{v})
			if err != nil {
				return runtime.Unit(), err
			}
			if res.Kind() == runtime.KindBool && res.BoolValue() {
				return runtime.Bool(true), nil
			}
		}
		return runtime.Bool(false), nil
	})

	reg("for_each", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindFn {
			return runtime.Unit(), fmt.Errorf("for_each expects a :List and a :Fn")
		}
		fn := args[1].Fn()
		for _, v := range args[0].List().Elems {
			if _, err := st.CallDomscript(fn, []runtime.Value{v}); err != nil {
				return runtime.Unit(), err
			}
		}
		return runtime.Unit(), nil
	})
}
