package os

import (
	"fmt"
	goos "os"
	"runtime"

	"domscript/internal/ffi"
	druntime "domscript/internal/runtime"
)

func Register(st *druntime.State) {
	reg := func(name string, fn druntime.GoNative) {
		ffi.Register(st, "__os_"+name, fn)
	}

	reg("getenv", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		if len(args) < 1 || args[0].Kind() != druntime.KindString {
			return druntime.Unit(), fmt.Errorf("getenv expects a :String key")
		}
		v, ok := goos.LookupEnv(args[0].StrValue())
		if !ok {
			return druntime.TaggedOf("None", nil), nil
		}
		return druntime.TaggedOf("Some", []druntime.Value{druntime.Str(v)}), nil
	})

	reg("setenv", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		if len(args) < 2 || args[0].Kind() != druntime.KindString || args[1].Kind() != druntime.KindString {
			return druntime.Unit(), fmt.Errorf("setenv expects :String key and :String value")
		}
		if err := goos.Setenv(args[0].StrValue(), args[1].StrValue()); err != nil {
			return druntime.TaggedOf("Error", []druntime.Value{druntime.Str(err.Error())}), nil
		}
		return druntime.Unit(), nil
	})

	reg("environ", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		env := goos.Environ()
		elems := make([]druntime.Value, len(env))
		for i, e := range env {
			elems[i] = druntime.Str(e)
		}
		return druntime.ListOf(elems), nil
	})

	reg("args", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		a := goos.Args
		elems := make([]druntime.Value, len(a))
		for i, arg := range a {
			elems[i] = druntime.Str(arg)
		}
		return druntime.ListOf(elems), nil
	})

	reg("exit", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		code := 0
		if len(args) > 0 && args[0].Kind() == druntime.KindInt {
			code = int(args[0].IntValue())
		}
		goos.Exit(code)
		return druntime.Unit(), nil
	})

	reg("cwd", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		dir, err := goos.Getwd()
		if err != nil {
			return druntime.TaggedOf("Error", []druntime.Value{druntime.Str(err.Error())}), nil
		}
		return druntime.TaggedOf("Success", []druntime.Value{druntime.Str(dir)}), nil
	})

	reg("chdir", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		if len(args) < 1 || args[0].Kind() != druntime.KindString {
			return druntime.Unit(), fmt.Errorf("chdir expects a :String path")
		}
		err := goos.Chdir(args[0].StrValue())
		if err != nil {
			return druntime.TaggedOf("Error", []druntime.Value{druntime.Str(err.Error())}), nil
		}
		return druntime.TaggedOf("Success", []druntime.Value{druntime.Unit()}), nil
	})

	reg("platform", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Str(runtime.GOOS), nil
	})

	reg("arch", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Str(runtime.GOARCH), nil
	})

	reg("hostname", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		h, err := goos.Hostname()
		if err != nil {
			return druntime.TaggedOf("Error", []druntime.Value{druntime.Str(err.Error())}), nil
		}
		return druntime.TaggedOf("Success", []druntime.Value{druntime.Str(h)}), nil
	})

	reg("pid", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Int(int64(goos.Getpid())), nil
	})

	reg("uid", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Int(int64(goos.Getuid())), nil
	})

	reg("tmpdir", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Str(goos.TempDir()), nil
	})

	reg("homedir", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		h, err := goos.UserHomeDir()
		if err != nil {
			return druntime.TaggedOf("Error", []druntime.Value{druntime.Str(err.Error())}), nil
		}
		return druntime.TaggedOf("Success", []druntime.Value{druntime.Str(h)}), nil
	})

	reg("separator", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Str(string(goos.PathSeparator)), nil
	})

	reg("list_separator", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Str(string(goos.PathListSeparator)), nil
	})

	reg("shell", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		shell := goos.Getenv("SHELL")
		if shell == "" {
			if runtime.GOOS == "windows" {
				shell = "cmd.exe"
			} else {
				shell = "/bin/sh"
			}
		}
		return druntime.Str(shell), nil
	})

	reg("executable", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		exe, err := goos.Executable()
		if err != nil {
			return druntime.TaggedOf("Error", []druntime.Value{druntime.Str(err.Error())}), nil
		}
		return druntime.TaggedOf("Success", []druntime.Value{druntime.Str(exe)}), nil
	})
}
