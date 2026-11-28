package unicode

import (
	"fmt"
	"unicode"
	"unicode/utf8"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__unicode_"+name, fn)
	}

	reg("is_letter", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("is_letter expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Bool(unicode.IsLetter(r)), nil
	})

	reg("is_digit", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("is_digit expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Bool(unicode.IsDigit(r)), nil
	})

	reg("is_space", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("is_space expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Bool(unicode.IsSpace(r)), nil
	})

	reg("is_punct", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("is_punct expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Bool(unicode.IsPunct(r)), nil
	})

	reg("is_upper", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("is_upper expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Bool(unicode.IsUpper(r)), nil
	})

	reg("is_lower", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("is_lower expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Bool(unicode.IsLower(r)), nil
	})

	reg("to_upper", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("to_upper expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Str(string(unicode.ToUpper(r))), nil
	})

	reg("to_lower", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("to_lower expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Str(string(unicode.ToLower(r))), nil
	})

	reg("to_title", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("to_title expects a :String or :Int codepoint")
		}
		r := toRune(args[0])
		return runtime.Str(string(unicode.ToTitle(r))), nil
	})

	reg("codepoint", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("codepoint expects a :String")
		}
		r, _ := utf8.DecodeRuneInString(args[0].StrValue())
		return runtime.Int(int64(r)), nil
	})

	reg("from_codepoint", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("from_codepoint expects an :Int codepoint")
		}
		return runtime.Str(string(rune(args[0].IntValue()))), nil
	})

	reg("len", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("len expects a :String")
		}
		return runtime.Int(int64(utf8.RuneCountInString(args[0].StrValue()))), nil
	})

	reg("normalize_nfc", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return args[0], nil
	})

	reg("normalize_nfd", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return args[0], nil
	})
}

func toRune(v runtime.Value) rune {
	if v.Kind() == runtime.KindInt {
		return rune(v.IntValue())
	}
	if v.Kind() == runtime.KindString {
		r, _ := utf8.DecodeRuneInString(v.StrValue())
		return r
	}
	return utf8.RuneError
}
