package stringmod

import (
	"fmt"
	"strconv"
	"strings"
	"unicode"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__string_"+name, fn)
	}

	reg("len", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("len expects a :String")
		}
		return runtime.Int(int64(len(args[0].StrValue()))), nil
	})

	reg("upper", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("upper expects a :String")
		}
		return runtime.Str(strings.ToUpper(args[0].StrValue())), nil
	})

	reg("lower", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("lower expects a :String")
		}
		return runtime.Str(strings.ToLower(args[0].StrValue())), nil
	})

	reg("trim", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("trim expects a :String")
		}
		return runtime.Str(strings.TrimSpace(args[0].StrValue())), nil
	})

	reg("trim_left", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("trim_left expects a :String")
		}
		cutset := " \t\n\r"
		if len(args) > 1 && args[1].Kind() == runtime.KindString {
			cutset = args[1].StrValue()
		}
		return runtime.Str(strings.TrimLeft(args[0].StrValue(), cutset)), nil
	})

	reg("trim_right", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("trim_right expects a :String")
		}
		cutset := " \t\n\r"
		if len(args) > 1 && args[1].Kind() == runtime.KindString {
			cutset = args[1].StrValue()
		}
		return runtime.Str(strings.TrimRight(args[0].StrValue(), cutset)), nil
	})

	reg("starts_with", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("starts_with expects two :Strings")
		}
		return runtime.Bool(strings.HasPrefix(args[0].StrValue(), args[1].StrValue())), nil
	})

	reg("ends_with", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("ends_with expects two :Strings")
		}
		return runtime.Bool(strings.HasSuffix(args[0].StrValue(), args[1].StrValue())), nil
	})

	reg("contains", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("contains expects two :Strings")
		}
		return runtime.Bool(strings.Contains(args[0].StrValue(), args[1].StrValue())), nil
	})

	reg("index_of", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("index_of expects two :Strings")
		}
		i := strings.Index(args[0].StrValue(), args[1].StrValue())
		if i < 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{runtime.Int(int64(i))}), nil
	})

	reg("last_index_of", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("last_index_of expects two :Strings")
		}
		i := strings.LastIndex(args[0].StrValue(), args[1].StrValue())
		if i < 0 {
			return runtime.TaggedOf("None", nil), nil
		}
		return runtime.TaggedOf("Some", []runtime.Value{runtime.Int(int64(i))}), nil
	})

	reg("replace", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 3 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString || args[2].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("replace expects three :Strings (s, old, new)")
		}
		n := -1
		if len(args) > 3 && args[3].Kind() == runtime.KindInt {
			n = int(args[3].IntValue())
		}
		return runtime.Str(strings.Replace(args[0].StrValue(), args[1].StrValue(), args[2].StrValue(), n)), nil
	})

	reg("split", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("split expects two :Strings")
		}
		parts := strings.Split(args[0].StrValue(), args[1].StrValue())
		elems := make([]runtime.Value, len(parts))
		for i, p := range parts {
			elems[i] = runtime.Str(p)
		}
		return runtime.ListOf(elems), nil
	})

	reg("join", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindList || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("join expects a :List of :Strings and a :String separator")
		}
		parts := make([]string, len(args[0].List().Elems))
		for i, v := range args[0].List().Elems {
			parts[i] = runtime.Display(v)
		}
		return runtime.Str(strings.Join(parts, args[1].StrValue())), nil
	})

	reg("repeat", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("repeat expects a :String and an :Int count")
		}
		return runtime.Str(strings.Repeat(args[0].StrValue(), int(args[1].IntValue()))), nil
	})

	reg("slice", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("slice expects a :String and start index")
		}
		s := args[0].StrValue()
		start := int(args[1].IntValue())
		end := len(s)
		if len(args) > 2 && args[2].Kind() == runtime.KindInt {
			end = int(args[2].IntValue())
		}
		if start < 0 {
			start = 0
		}
		if end > len(s) {
			end = len(s)
		}
		if start > end {
			return runtime.Str(""), nil
		}
		return runtime.Str(s[start:end]), nil
	})

	reg("pad_left", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("pad_left expects a :String and an :Int width")
		}
		pad := " "
		if len(args) > 2 && args[2].Kind() == runtime.KindString {
			pad = args[2].StrValue()
		}
		s := args[0].StrValue()
		w := int(args[1].IntValue())
		for len(s) < w {
			s = pad + s
		}
		return runtime.Str(s), nil
	})

	reg("pad_right", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("pad_right expects a :String and an :Int width")
		}
		pad := " "
		if len(args) > 2 && args[2].Kind() == runtime.KindString {
			pad = args[2].StrValue()
		}
		s := args[0].StrValue()
		w := int(args[1].IntValue())
		for len(s) < w {
			s = s + pad
		}
		return runtime.Str(s), nil
	})

	reg("reverse", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("reverse expects a :String")
		}
		runes := []rune(args[0].StrValue())
		for i, j := 0, len(runes)-1; i < j; i, j = i+1, j-1 {
			runes[i], runes[j] = runes[j], runes[i]
		}
		return runtime.Str(string(runes)), nil
	})

	reg("capitalize", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("capitalize expects a :String")
		}
		s := args[0].StrValue()
		if len(s) == 0 {
			return runtime.Str(""), nil
		}
		runes := []rune(s)
		runes[0] = unicode.ToUpper(runes[0])
		for i := 1; i < len(runes); i++ {
			runes[i] = unicode.ToLower(runes[i])
		}
		return runtime.Str(string(runes)), nil
	})

	reg("title", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("title expects a :String")
		}
		return runtime.Str(strings.ToTitle(args[0].StrValue())), nil
	})

	reg("is_alpha", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_alpha expects a :String")
		}
		for _, r := range args[0].StrValue() {
			if !unicode.IsLetter(r) {
				return runtime.Bool(false), nil
			}
		}
		return runtime.Bool(len(args[0].StrValue()) > 0), nil
	})

	reg("is_digit", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_digit expects a :String")
		}
		for _, r := range args[0].StrValue() {
			if !unicode.IsDigit(r) {
				return runtime.Bool(false), nil
			}
		}
		return runtime.Bool(len(args[0].StrValue()) > 0), nil
	})

	reg("is_alnum", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_alnum expects a :String")
		}
		for _, r := range args[0].StrValue() {
			if !unicode.IsLetter(r) && !unicode.IsDigit(r) {
				return runtime.Bool(false), nil
			}
		}
		return runtime.Bool(len(args[0].StrValue()) > 0), nil
	})

	reg("is_space", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_space expects a :String")
		}
		for _, r := range args[0].StrValue() {
			if !unicode.IsSpace(r) {
				return runtime.Bool(false), nil
			}
		}
		return runtime.Bool(len(args[0].StrValue()) > 0), nil
	})

	reg("is_upper", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_upper expects a :String")
		}
		for _, r := range args[0].StrValue() {
			if !unicode.IsUpper(r) {
				return runtime.Bool(false), nil
			}
		}
		return runtime.Bool(len(args[0].StrValue()) > 0), nil
	})

	reg("is_lower", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_lower expects a :String")
		}
		for _, r := range args[0].StrValue() {
			if !unicode.IsLower(r) {
				return runtime.Bool(false), nil
			}
		}
		return runtime.Bool(len(args[0].StrValue()) > 0), nil
	})

	reg("count", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("count expects two :Strings")
		}
		return runtime.Int(int64(strings.Count(args[0].StrValue(), args[1].StrValue()))), nil
	})

	reg("format", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("format expects a :String template")
		}
		s := args[0].StrValue()
		for i := 1; i < len(args); i++ {
			placeholder := "{" + strconv.Itoa(i-1) + "}"
			s = strings.ReplaceAll(s, placeholder, runtime.Display(args[i]))
		}
		return runtime.Str(s), nil
	})

	_ = strconv.Itoa
}
