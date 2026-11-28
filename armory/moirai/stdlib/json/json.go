package json

import (
	"encoding/json"
	"fmt"
	"os"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__json_"+name, fn)
	}

	reg("encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("encode expects a value")
		}
		goVal := toGo(args[0])
		data, err := json.Marshal(goVal)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("decode expects a :String")
		}
		var goVal any
		err := json.Unmarshal([]byte(args[0].StrValue()), &goVal)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{fromGo(goVal)}), nil
	})

	reg("pretty", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("pretty expects a value")
		}
		goVal := toGo(args[0])
		data, err := json.MarshalIndent(goVal, "", "  ")
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("validate", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("validate expects a :String")
		}
		var goVal any
		err := json.Unmarshal([]byte(args[0].StrValue()), &goVal)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Bool(true)}), nil
	})

	reg("encode_to_file", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("encode_to_file expects a value and :String path")
		}
		goVal := toGo(args[0])
		data, err := json.MarshalIndent(goVal, "", "  ")
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		if err := os.WriteFile(args[1].StrValue(), data, 0644); err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})

	reg("decode_from_file", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("decode_from_file expects a :String path")
		}
		data, err := os.ReadFile(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		var goVal any
		if err := json.Unmarshal(data, &goVal); err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{fromGo(goVal)}), nil
	})
}

func toGo(v runtime.Value) any {
	switch v.Kind() {
	case runtime.KindUnit:
		return nil
	case runtime.KindBool:
		return v.BoolValue()
	case runtime.KindInt:
		return v.IntValue()
	case runtime.KindFloat:
		return v.FloatValue()
	case runtime.KindString:
		return v.StrValue()
	case runtime.KindList:
		elems := v.List().Elems
		out := make([]any, len(elems))
		for i, e := range elems {
			out[i] = toGo(e)
		}
		return out
	case runtime.KindTable:
		out := make(map[string]any)
		v.Table().Entries(func(k, val runtime.Value) {
			if k.Kind() == runtime.KindString {
				out[k.StrValue()] = toGo(val)
				return
			}
			out[runtime.Display(k)] = toGo(val)
		})
		return out
	default:
		return runtime.Display(v)
	}
}

func fromGo(v any) runtime.Value {
	switch val := v.(type) {
	case nil:
		return runtime.Unit()
	case bool:
		return runtime.Bool(val)
	case float64:
		if val == float64(int64(val)) {
			return runtime.Int(int64(val))
		}
		return runtime.Float(val)
	case string:
		return runtime.Str(val)
	case []any:
		elems := make([]runtime.Value, len(val))
		for i, e := range val {
			elems[i] = fromGo(e)
		}
		return runtime.ListOf(elems)
	case map[string]any:
		t := runtime.NewTable()
		for k, v := range val {
			t.Set(runtime.Str(k), fromGo(v))
		}
		return runtime.TableValue(t)
	case []byte:
		return runtime.Str(string(val))
	default:
		return runtime.Str(fmt.Sprintf("%v", val))
	}
}
