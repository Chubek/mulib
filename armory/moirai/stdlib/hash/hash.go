package hash

import (
	"crypto/md5"
	"crypto/sha1"
	"crypto/sha256"
	"crypto/sha512"
	"encoding/hex"
	"fmt"
	"hash/crc32"
	"hash/fnv"
	"os"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__hash_"+name, fn)
	}

	reg("fnv1a", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("fnv1a expects a :String")
		}
		h := fnv.New64a()
		h.Write([]byte(args[0].StrValue()))
		return runtime.Str(hex.EncodeToString(h.Sum(nil))), nil
	})

	reg("crc32", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("crc32 expects a :String")
		}
		h := crc32.ChecksumIEEE([]byte(args[0].StrValue()))
		return runtime.Str(fmt.Sprintf("%08x", h)), nil
	})

	reg("sha1", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("sha1 expects a :String")
		}
		h := sha1.Sum([]byte(args[0].StrValue()))
		return runtime.Str(hex.EncodeToString(h[:])), nil
	})

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

	reg("hash_file", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("hash_file expects :String algorithm and :String path")
		}
		data, err := os.ReadFile(args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		algo := args[0].StrValue()
		switch algo {
		case "sha256":
			h := sha256.Sum256(data)
			return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(hex.EncodeToString(h[:]))}), nil
		case "sha512":
			h := sha512.Sum512(data)
			return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(hex.EncodeToString(h[:]))}), nil
		case "md5":
			h := md5.Sum(data)
			return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(hex.EncodeToString(h[:]))}), nil
		default:
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str("unknown algorithm: " + algo)}), nil
		}
	})
}
