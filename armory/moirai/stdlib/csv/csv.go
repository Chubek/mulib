package csv

import (
	"encoding/csv"
	"fmt"
	"os"
	"strings"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__csv_"+name, fn)
	}

	reg("read", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("read expects a :String path")
		}
		f, err := os.Open(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		defer f.Close()
		reader := csv.NewReader(f)
		records, err := reader.ReadAll()
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		elems := make([]runtime.Value, len(records))
		for i, rec := range records {
			fields := make([]runtime.Value, len(rec))
			for j, f := range rec {
				fields[j] = runtime.Str(f)
			}
			elems[i] = runtime.ListOf(fields)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(elems)}), nil
	})

	reg("write", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("write expects a :String path and :List of rows")
		}
		f, err := os.Create(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		defer f.Close()
		writer := csv.NewWriter(f)
		for _, row := range args[1].List().Elems {
			if row.Kind() == runtime.KindList {
				fields := row.List().Elems
				rec := make([]string, len(fields))
				for j, f := range fields {
					rec[j] = runtime.Display(f)
				}
				writer.Write(rec)
			}
		}
		writer.Flush()
		if err := writer.Error(); err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})

	reg("parse_line", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("parse_line expects a :String")
		}
		reader := csv.NewReader(strings.NewReader(args[0].StrValue()))
		records, err := reader.ReadAll()
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		if len(records) == 0 {
			return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(nil)}), nil
		}
		fields := make([]runtime.Value, len(records[0]))
		for i, f := range records[0] {
			fields[i] = runtime.Str(f)
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.ListOf(fields)}), nil
	})

	reg("format_line", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindList {
			return runtime.Unit(), fmt.Errorf("format_line expects a :List of fields")
		}
		var buf strings.Builder
		w := csv.NewWriter(&buf)
		fields := args[0].List().Elems
		rec := make([]string, len(fields))
		for i, f := range fields {
			rec[i] = runtime.Display(f)
		}
		w.Write(rec)
		w.Flush()
		s := buf.String()
		if len(s) > 0 {
			s = strings.TrimSuffix(s, "\n")
		}
		return runtime.Str(s), nil
	})
}
