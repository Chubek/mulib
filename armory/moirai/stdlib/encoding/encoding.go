package encoding

import (
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"net/url"
	"unicode/utf8"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__encoding_"+name, fn)
	}

	reg("base64_encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("base64_encode expects a :String")
		}
		return runtime.Str(base64.StdEncoding.EncodeToString([]byte(args[0].StrValue()))), nil
	})

	reg("base64_decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("base64_decode expects a :String")
		}
		data, err := base64.StdEncoding.DecodeString(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("hex_encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("hex_encode expects a :String")
		}
		return runtime.Str(hex.EncodeToString([]byte(args[0].StrValue()))), nil
	})

	reg("hex_decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("hex_decode expects a :String")
		}
		data, err := hex.DecodeString(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("url_encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("url_encode expects a :String")
		}
		return runtime.Str(url.QueryEscape(args[0].StrValue())), nil
	})

	reg("url_decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("url_decode expects a :String")
		}
		s, err := url.QueryUnescape(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(s)}), nil
	})

	reg("utf8_encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("utf8_encode expects an :Int codepoint")
		}
		buf := make([]byte, utf8.UTFMax)
		n := utf8.EncodeRune(buf, rune(args[0].IntValue()))
		return runtime.Str(string(buf[:n])), nil
	})

	reg("utf8_decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("utf8_decode expects a :String")
		}
		r, _ := utf8.DecodeRuneInString(args[0].StrValue())
		return runtime.Int(int64(r)), nil
	})

	reg("utf8_valid", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("utf8_valid expects a :String")
		}
		return runtime.Bool(utf8.ValidString(args[0].StrValue())), nil
	})
}
