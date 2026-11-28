package crypto

import (
	"crypto/hmac"
	"crypto/md5"
	"crypto/rand"
	"crypto/sha256"
	"crypto/sha512"
	"encoding/hex"
	"fmt"
	"hash"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__crypto_"+name, fn)
	}

	reg("sha256", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("sha256 expects a :String")
		}
		h := sha256.Sum256([]byte(args[0].StrValue()))
		return runtime.Str(hex.EncodeToString(h[:])), nil
	})

	reg("sha512", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("sha512 expects a :String")
		}
		h := sha512.Sum512([]byte(args[0].StrValue()))
		return runtime.Str(hex.EncodeToString(h[:])), nil
	})

	reg("md5", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("md5 expects a :String")
		}
		h := md5.Sum([]byte(args[0].StrValue()))
		return runtime.Str(hex.EncodeToString(h[:])), nil
	})

	reg("hmac_sha256", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("hmac_sha256 expects :String key and :String message")
		}
		mac := hmac.New(sha256.New, []byte(args[0].StrValue()))
		mac.Write([]byte(args[1].StrValue()))
		return runtime.Str(hex.EncodeToString(mac.Sum(nil))), nil
	})

	reg("hmac_sha512", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("hmac_sha512 expects :String key and :String message")
		}
		mac := hmac.New(sha512.New, []byte(args[0].StrValue()))
		mac.Write([]byte(args[1].StrValue()))
		return runtime.Str(hex.EncodeToString(mac.Sum(nil))), nil
	})

	reg("random_bytes", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("random_bytes expects an :Int count")
		}
		n := int(args[0].IntValue())
		buf := make([]byte, n)
		if _, err := rand.Read(buf); err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(hex.EncodeToString(buf))}), nil
	})

	reg("constant_time_compare", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("constant_time_compare expects two :Strings")
		}
		a := []byte(args[0].StrValue())
		b := []byte(args[1].StrValue())
		if len(a) != len(b) {
			return runtime.Bool(false), nil
		}
		result := byte(0)
		for i := range a {
			result |= a[i] ^ b[i]
		}
		return runtime.Bool(result == 0), nil
	})

	_ = hash.Hash(nil)
}
