package ffi

import (
	"fmt"
	"unsafe"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__ffi_"+name, fn)
	}

	reg("size_of", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("size_of expects a :String type name")
		}
		sizes := map[string]int64{
			"int8": 1, "int16": 2, "int32": 4, "int64": 8,
			"uint8": 1, "uint16": 2, "uint32": 4, "uint64": 8,
			"float32": 4, "float64": 8,
			"ptr": int64(unsafe.Sizeof(uintptr(0))), "pointer": int64(unsafe.Sizeof(uintptr(0))),
		}
		if s, ok := sizes[args[0].StrValue()]; ok {
			return runtime.Int(s), nil
		}
		return runtime.Int(0), nil
	})

	reg("align_of", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("align_of expects a :String type name")
		}
		aligns := map[string]int64{
			"int8": 1, "int16": 2, "int32": 4, "int64": 8,
			"uint8": 1, "uint16": 2, "uint32": 4, "uint64": 8,
			"float32": 4, "float64": 8,
			"ptr": int64(unsafe.Alignof(uintptr(0))), "pointer": int64(unsafe.Alignof(uintptr(0))),
		}
		if a, ok := aligns[args[0].StrValue()]; ok {
			return runtime.Int(a), nil
		}
		return runtime.Int(8), nil
	})

	reg("c_string", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("c_string expects a :String")
		}
		return runtime.Str(args[0].StrValue() + "\x00"), nil
	})

	reg("go_string", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("go_string expects a :String")
		}
		s := args[0].StrValue()
		// Strip trailing null byte
		if len(s) > 0 && s[len(s)-1] == 0 {
			s = s[:len(s)-1]
		}
		return runtime.Str(s), nil
	})
}
