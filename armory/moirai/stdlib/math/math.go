package math

import (
	"fmt"
	"math"
	"math/rand"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func toFloat(v runtime.Value) float64 {
	if v.Kind() == runtime.KindInt {
		return float64(v.IntValue())
	}
	return v.FloatValue()
}

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__math_"+name, fn)
	}

	reg("abs", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("abs expects a number")
		}
		if args[0].Kind() == runtime.KindInt {
			n := args[0].IntValue()
			if n < 0 {
				n = -n
			}
			return runtime.Int(n), nil
		}
		return runtime.Float(math.Abs(toFloat(args[0]))), nil
	})

	reg("ceil", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("ceil expects a number")
		}
		return runtime.Float(math.Ceil(toFloat(args[0]))), nil
	})

	reg("floor", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("floor expects a number")
		}
		return runtime.Float(math.Floor(toFloat(args[0]))), nil
	})

	reg("round", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("round expects a number")
		}
		return runtime.Float(math.Round(toFloat(args[0]))), nil
	})

	reg("sqrt", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("sqrt expects a number")
		}
		return runtime.Float(math.Sqrt(toFloat(args[0]))), nil
	})

	reg("cbrt", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("cbrt expects a number")
		}
		return runtime.Float(math.Cbrt(toFloat(args[0]))), nil
	})

	reg("pow", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 {
			return runtime.Unit(), fmt.Errorf("pow expects two numbers")
		}
		return runtime.Float(math.Pow(toFloat(args[0]), toFloat(args[1]))), nil
	})

	reg("exp", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("exp expects a number")
		}
		return runtime.Float(math.Exp(toFloat(args[0]))), nil
	})

	reg("log", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("log expects a number")
		}
		return runtime.Float(math.Log(toFloat(args[0]))), nil
	})

	reg("log2", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("log2 expects a number")
		}
		return runtime.Float(math.Log2(toFloat(args[0]))), nil
	})

	reg("log10", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("log10 expects a number")
		}
		return runtime.Float(math.Log10(toFloat(args[0]))), nil
	})

	reg("sin", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("sin expects a number")
		}
		return runtime.Float(math.Sin(toFloat(args[0]))), nil
	})

	reg("cos", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("cos expects a number")
		}
		return runtime.Float(math.Cos(toFloat(args[0]))), nil
	})

	reg("tan", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("tan expects a number")
		}
		return runtime.Float(math.Tan(toFloat(args[0]))), nil
	})

	reg("asin", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("asin expects a number")
		}
		return runtime.Float(math.Asin(toFloat(args[0]))), nil
	})

	reg("acos", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("acos expects a number")
		}
		return runtime.Float(math.Acos(toFloat(args[0]))), nil
	})

	reg("atan", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("atan expects a number")
		}
		return runtime.Float(math.Atan(toFloat(args[0]))), nil
	})

	reg("atan2", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 {
			return runtime.Unit(), fmt.Errorf("atan2 expects two numbers")
		}
		return runtime.Float(math.Atan2(toFloat(args[0]), toFloat(args[1]))), nil
	})

	reg("sinh", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("sinh expects a number")
		}
		return runtime.Float(math.Sinh(toFloat(args[0]))), nil
	})

	reg("cosh", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("cosh expects a number")
		}
		return runtime.Float(math.Cosh(toFloat(args[0]))), nil
	})

	reg("tanh", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("tanh expects a number")
		}
		return runtime.Float(math.Tanh(toFloat(args[0]))), nil
	})

	reg("hypot", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 {
			return runtime.Unit(), fmt.Errorf("hypot expects two numbers")
		}
		return runtime.Float(math.Hypot(toFloat(args[0]), toFloat(args[1]))), nil
	})

	reg("degrees", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("degrees expects a number")
		}
		return runtime.Float(toFloat(args[0]) * 180.0 / math.Pi), nil
	})

	reg("radians", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("radians expects a number")
		}
		return runtime.Float(toFloat(args[0]) * math.Pi / 180.0), nil
	})

	reg("gcd", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindInt || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("gcd expects two :Ints")
		}
		a, b := args[0].IntValue(), args[1].IntValue()
		for b != 0 {
			a, b = b, a%b
		}
		if a < 0 {
			a = -a
		}
		return runtime.Int(a), nil
	})

	reg("lcm", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindInt || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("lcm expects two :Ints")
		}
		a, b := args[0].IntValue(), args[1].IntValue()
		aa, bb := a, b
		for bb != 0 {
			aa, bb = bb, aa%bb
		}
		if aa < 0 {
			aa = -aa
		}
		return runtime.Int((a / aa) * b), nil
	})

	reg("factorial", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("factorial expects an :Int")
		}
		n := args[0].IntValue()
		if n < 0 {
			return runtime.Unit(), fmt.Errorf("factorial: negative input")
		}
		result := int64(1)
		for i := int64(2); i <= n; i++ {
			result *= i
		}
		return runtime.Int(result), nil
	})

	reg("is_nan", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("is_nan expects a number")
		}
		return runtime.Bool(math.IsNaN(toFloat(args[0]))), nil
	})

	reg("is_inf", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("is_inf expects a number")
		}
		return runtime.Bool(math.IsInf(toFloat(args[0]), 0)), nil
	})

	reg("sign", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("sign expects a number")
		}
		x := toFloat(args[0])
		if x < 0 {
			return runtime.Int(-1), nil
		}
		if x > 0 {
			return runtime.Int(1), nil
		}
		return runtime.Int(0), nil
	})

	reg("min", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 {
			return runtime.Unit(), fmt.Errorf("min expects at least two numbers")
		}
		minVal := toFloat(args[0])
		for _, a := range args[1:] {
			v := toFloat(a)
			if v < minVal {
				minVal = v
			}
		}
		return runtime.Float(minVal), nil
	})

	reg("max", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 {
			return runtime.Unit(), fmt.Errorf("max expects at least two numbers")
		}
		maxVal := toFloat(args[0])
		for _, a := range args[1:] {
			v := toFloat(a)
			if v > maxVal {
				maxVal = v
			}
		}
		return runtime.Float(maxVal), nil
	})

	reg("clamp", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 3 {
			return runtime.Unit(), fmt.Errorf("clamp expects value, min, max")
		}
		x := toFloat(args[0])
		lo := toFloat(args[1])
		hi := toFloat(args[2])
		if x < lo {
			x = lo
		}
		if x > hi {
			x = hi
		}
		return runtime.Float(x), nil
	})

	reg("lerp", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 3 {
			return runtime.Unit(), fmt.Errorf("lerp expects a, b, t")
		}
		a := toFloat(args[0])
		b := toFloat(args[1])
		t := toFloat(args[2])
		return runtime.Float(a + (b-a)*t), nil
	})

	reg("trunc", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("trunc expects a number")
		}
		return runtime.Float(math.Trunc(toFloat(args[0]))), nil
	})

	_ = rand.Int
}
