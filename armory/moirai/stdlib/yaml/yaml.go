package yaml

import (
	"fmt"
	"os"
	"strconv"
	"strings"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__yaml_"+name, fn)
	}

	reg("encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("encode expects a value")
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(yamlEncode(args[0], 0))}), nil
	})

	reg("decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("decode expects a :String")
		}
		result, err := yamlDecode(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{result}), nil
	})

	reg("pretty", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("pretty expects a value")
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(yamlEncode(args[0], 0))}), nil
	})

	reg("validate", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("validate expects a :String")
		}
		_, err := yamlDecode(args[0].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Bool(true)}), nil
	})

	reg("encode_to_file", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("encode_to_file expects a value and :String path")
		}
		data := yamlEncode(args[0], 0)
		if err := os.WriteFile(args[1].StrValue(), []byte(data), 0644); err != nil {
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
		result, err := yamlDecode(string(data))
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{result}), nil
	})
}

func yamlEncode(v runtime.Value, indent int) string {
	prefix := strings.Repeat("  ", indent)
	switch v.Kind() {
	case runtime.KindUnit:
		return "null"
	case runtime.KindBool:
		if v.BoolValue() {
			return "true"
		}
		return "false"
	case runtime.KindInt:
		return strconv.FormatInt(v.IntValue(), 10)
	case runtime.KindFloat:
		return strconv.FormatFloat(v.FloatValue(), 'g', -1, 64)
	case runtime.KindString:
		s := v.StrValue()
		if strings.ContainsAny(s, ":\n\"'{}[],&*#?|-<>=!%@`") || s == "" {
			return strconv.Quote(s)
		}
		return s
	case runtime.KindList:
		elems := v.List().Elems
		if len(elems) == 0 {
			return "[]"
		}
		var lines []string
		for _, e := range elems {
			encoded := yamlEncode(e, indent+1)
			if strings.Contains(encoded, "\n") {
				lines = append(lines, prefix+"-")
				for _, subline := range strings.Split(encoded, "\n") {
					lines = append(lines, prefix+"  "+subline)
				}
			} else {
				lines = append(lines, prefix+"- "+encoded)
			}
		}
		return strings.Join(lines, "\n")
	case runtime.KindTable:
		t := v.Table()
		if t.Len() == 0 {
			return "{}"
		}
		var lines []string
		t.Entries(func(k, val runtime.Value) {
			key := runtime.Display(k)
			valStr := yamlEncode(val, indent+1)
			if strings.Contains(valStr, "\n") || val.Kind() == runtime.KindTable || val.Kind() == runtime.KindList {
				lines = append(lines, prefix+key+":")
				for _, subline := range strings.Split(valStr, "\n") {
					lines = append(lines, "  "+subline)
				}
			} else {
				lines = append(lines, prefix+key+": "+valStr)
			}
		})
		return strings.Join(lines, "\n")
	default:
		return strconv.Quote(runtime.Display(v))
	}
}

func yamlDecode(src string) (runtime.Value, error) {
	// Simple YAML subset: flat key-value pairs and simple lists
	lines := strings.Split(src, "\n")
	// Filter empty lines and comments
	var clean []string
	for _, l := range lines {
		l = strings.TrimSpace(l)
		if l == "" || strings.HasPrefix(l, "#") {
			continue
		}
		clean = append(clean, l)
	}
	if len(clean) == 0 {
		return runtime.Unit(), fmt.Errorf("empty YAML document")
	}

	// Check if it's a list
	if strings.HasPrefix(clean[0], "- ") {
		elems := make([]runtime.Value, 0, len(clean))
		for _, l := range clean {
			if strings.HasPrefix(l, "- ") {
				v := l[2:]
				elems = append(elems, parseScalar(v))
			}
		}
		return runtime.ListOf(elems), nil
	}

	// Otherwise treat as a map
	t := runtime.NewTable()
	for _, l := range clean {
		parts := strings.SplitN(l, ":", 2)
		if len(parts) != 2 {
			continue
		}
		key := strings.TrimSpace(parts[0])
		val := strings.TrimSpace(parts[1])
		t.Set(runtime.Str(key), parseScalar(val))
	}
	return runtime.TableValue(t), nil
}

func parseScalar(s string) runtime.Value {
	s = strings.TrimSpace(s)
	if s == "null" || s == "~" {
		return runtime.Unit()
	}
	if s == "true" || s == "yes" || s == "on" {
		return runtime.Bool(true)
	}
	if s == "false" || s == "no" || s == "off" {
		return runtime.Bool(false)
	}
	if strings.HasPrefix(s, `"`) && strings.HasSuffix(s, `"`) {
		unq, _ := strconv.Unquote(s)
		return runtime.Str(unq)
	}
	if strings.HasPrefix(s, `'`) && strings.HasSuffix(s, `'`) {
		return runtime.Str(s[1 : len(s)-1])
	}
	if i, err := strconv.ParseInt(s, 10, 64); err == nil {
		return runtime.Int(i)
	}
	if f, err := strconv.ParseFloat(s, 64); err == nil {
		return runtime.Float(f)
	}
	return runtime.Str(s)
}
