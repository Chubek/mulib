package io

import (
	"bufio"
	"fmt"
	"os"
	"strings"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__io_"+name, fn)
	}

	reg("print", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		for _, a := range args {
			st.Print(a)
		}
		return runtime.Unit(), nil
	})

	reg("println", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		for _, a := range args {
			fmt.Fprint(st.Output, runtime.Display(a))
		}
		fmt.Fprintln(st.Output)
		return runtime.Unit(), nil
	})

	reg("input", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		prompt := ""
		if len(args) > 0 && args[0].Kind() == runtime.KindString {
			prompt = args[0].StrValue()
		}
		if prompt != "" {
			fmt.Fprint(st.Output, prompt)
		}
		reader := bufio.NewReader(os.Stdin)
		line, err := reader.ReadString('\n')
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(strings.TrimRight(line, "\r\n"))}), nil
	})

	reg("read_file", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("read_file expects a :String path")
		}
		data, err := os.ReadFile(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("write_file", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("write_file expects :String path and :String content")
		}
		err := os.WriteFile(args[0].StrValue(), []byte(args[1].StrValue()), 0644)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})

	reg("read_all", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("read_all expects a :String path")
		}
		data, err := os.ReadFile(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("file_exists", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("file_exists expects a :String path")
		}
		_, err := os.Stat(args[0].StrValue())
		return runtime.Bool(err == nil), nil
	})

	reg("stdout_write", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("stdout_write expects a :String")
		}
		fmt.Fprint(os.Stdout, args[0].StrValue())
		return runtime.Unit(), nil
	})

	reg("stderr_write", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("stderr_write expects a :String")
		}
		fmt.Fprint(os.Stderr, args[0].StrValue())
		return runtime.Unit(), nil
	})
}
