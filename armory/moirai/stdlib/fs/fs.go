package fs

import (
	"fmt"
	"io/fs"
	"os"
	"path/filepath"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__fs_"+name, fn)
	}

	reg("exists", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("exists expects a :String path")
		}
		_, err := os.Stat(args[0].StrValue())
		if err == nil {
			return runtime.Bool(true), nil
		}
		if os.IsNotExist(err) {
			return runtime.Bool(false), nil
		}
		return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
	})

	reg("is_dir", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_dir expects a :String path")
		}
		info, err := os.Stat(args[0].StrValue())
		if err != nil {
			if os.IsNotExist(err) {
				return runtime.Bool(false), nil
			}
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.Bool(info.IsDir()), nil
	})

	reg("is_file", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("is_file expects a :String path")
		}
		info, err := os.Stat(args[0].StrValue())
		if err != nil {
			if os.IsNotExist(err) {
				return runtime.Bool(false), nil
			}
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.Bool(info.Mode().IsRegular()), nil
	})

	reg("read", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("read expects a :String path")
		}
		data, err := os.ReadFile(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("write", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("write expects :String path and :String content")
		}
		err := os.WriteFile(args[0].StrValue(), []byte(args[1].StrValue()), 0644)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})

	reg("remove", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("remove expects a :String path")
		}
		err := os.RemoveAll(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})

	reg("mkdir", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("mkdir expects a :String path")
		}
		err := os.MkdirAll(args[0].StrValue(), 0755)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})

	reg("list_dir", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("list_dir expects a :String path")
		}
		entries, err := os.ReadDir(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(entries))
		for i, e := range entries {
			kind := "file"
			if e.IsDir() {
				kind = "dir"
			}
			elems[i] = runtime.TableOf([][2]runtime.Value{
				{runtime.Str("name"), runtime.Str(e.Name())},
				{runtime.Str("kind"), runtime.Str(kind)},
			})
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("stat", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("stat expects a :String path")
		}
		info, err := os.Stat(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		t := runtime.NewTable()
		t.Set(runtime.Str("name"), runtime.Str(info.Name()))
		t.Set(runtime.Str("size"), runtime.Int(info.Size()))
		t.Set(runtime.Str("is_dir"), runtime.Bool(info.IsDir()))
		t.Set(runtime.Str("mode"), runtime.Str(info.Mode().String()))
		t.Set(runtime.Str("mod_time"), runtime.Int(info.ModTime().Unix()))
		return runtime.TaggedOf("Success", []runtime.Value{runtime.TableValue(t)}), nil
	})

	reg("rename", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("rename expects :String old and :String new")
		}
		err := os.Rename(args[0].StrValue(), args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})

	reg("copy", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("copy expects :String src and :String dst")
		}
		data, err := os.ReadFile(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		err = os.WriteFile(args[1].StrValue(), data, 0644)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})

	reg("walk", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("walk expects a :String path")
		}
		var paths []runtime.Value
		var walkErr error
		filepath.Walk(args[0].StrValue(), func(path string, info fs.FileInfo, err error) error {
			if err != nil {
				walkErr = err
				return err
			}
			paths = append(paths, runtime.Str(path))
			return nil
		})
		if walkErr != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(walkErr.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(paths)}), nil
	})

	reg("ext", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("ext expects a :String path")
		}
		return runtime.Str(filepath.Ext(args[0].StrValue())), nil
	})

	reg("base", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("base expects a :String path")
		}
		return runtime.Str(filepath.Base(args[0].StrValue())), nil
	})

	reg("dir", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("dir expects a :String path")
		}
		return runtime.Str(filepath.Dir(args[0].StrValue())), nil
	})
}
