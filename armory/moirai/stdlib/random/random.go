package random

import (
	"fmt"
	"math/rand"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

var rng = rand.New(rand.NewSource(rand.Int63()))

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__random_"+name, fn)
	}

	reg("seed", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("seed expects an :Int")
		}
		rng = rand.New(rand.NewSource(args[0].IntValue()))
		return runtime.Unit(), nil
	})

	reg("random", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Float(rng.Float64()), nil
	})

	reg("randint", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("randint expects an :Int max (exclusive)")
		}
		return runtime.Int(int64(rng.Intn(int(args[0].IntValue())))), nil
	})

	reg("uniform", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 {
			return runtime.Unit(), fmt.Errorf("uniform expects lo and hi")
		}
		lo := args[0].Numeric()
		hi := args[1].Numeric()
		return runtime.Float(lo + rng.Float64()*(hi-lo)), nil
	})

	reg("choice", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("choice expects a :List")
		}
		elems := args[0].List().Elems
		if len(elems) == 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{elems[rng.Intn(len(elems))]}), nil
	})

	reg("choices", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("choices expects a :List")
		}
		elems := args[0].List().Elems
		k := len(elems)
		if len(args) > 1 && args[1].Kind() == runtime.KindInt {
			k = int(args[1].IntValue())
		}
		out := make([]runtime.Value, k)
		for i := 0; i < k; i++ {
			out[i] = elems[rng.Intn(len(elems))]
		}
		return runtime.ListOf(out), nil
	})

	reg("shuffle", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("shuffle expects a :List")
		}
		elems := args[0].List().Elems
		out := make([]runtime.Value, len(elems))
		copy(out, elems)
		rng.Shuffle(len(out), func(i, j int) {
			out[i], out[j] = out[j], out[i]
		})
		return runtime.ListOf(out), nil
	})

	reg("sample", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("sample expects a :List and an :Int k")
		}
		elems := args[0].List().Elems
		k := int(args[1].IntValue())
		if k > len(elems) {
			k = len(elems)
		}
		perm := rng.Perm(len(elems))
		out := make([]runtime.Value, k)
		for i := 0; i < k; i++ {
			out[i] = elems[perm[i]]
		}
		return runtime.ListOf(out), nil
	})

	reg("gauss", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		mu := 0.0
		sigma := 1.0
		if len(args) > 0 {
			mu = args[0].Numeric()
		}
		if len(args) > 1 {
			sigma = args[1].Numeric()
		}
		return runtime.Float(rng.NormFloat64()*sigma + mu), nil
	})

	reg("bytes", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("bytes expects an :Int count")
		}
		n := int(args[0].IntValue())
		buf := make([]byte, n)
		rng.Read(buf)
		return runtime.Str(string(buf)), nil
	})
}
