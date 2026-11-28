package meekamcp

import (
	"encoding/json"
	"fmt"
	"reflect"
	"strconv"
	"strings"
)

// SchemaFor derives a JSON Schema (draft-07 compatible subset) from a Go
// struct type using `json`, `doc`/`description`, `jsonschema`, `default`,
// and `enum` struct tags. See MEEKAMCP.md section 5.7.
func SchemaFor(t reflect.Type) (map[string]any, error) {
	for t.Kind() == reflect.Pointer {
		t = t.Elem()
	}
	if t.Kind() != reflect.Struct {
		return nil, fmt.Errorf("meekamcp: schema input must be a struct, got %s", t.Kind())
	}
	return structSchema(t)
}

func structSchema(t reflect.Type) (map[string]any, error) {
	properties := map[string]any{}
	required := []string{}
	for i := 0; i < t.NumField(); i++ {
		field := t.Field(i)
		if !field.IsExported() {
			continue
		}
		name, _, skip := jsonName(field)
		if skip {
			continue
		}
		prop, isRequired, err := fieldSchema(field)
		if err != nil {
			return nil, fmt.Errorf("field %s: %w", field.Name, err)
		}
		properties[name] = prop
		if isRequired {
			required = append(required, name)
		}
	}
	schema := map[string]any{
		"type":       "object",
		"properties": properties,
	}
	if len(required) > 0 {
		schema["required"] = required
	}
	return schema, nil
}

func jsonName(field reflect.StructField) (name string, omitempty, skip bool) {
	tag := field.Tag.Get("json")
	if tag == "-" {
		return "", false, true
	}
	parts := strings.Split(tag, ",")
	name = parts[0]
	if name == "" {
		name = field.Name
	}
	for _, p := range parts[1:] {
		if p == "omitempty" {
			omitempty = true
		}
	}
	return name, omitempty, false
}

func fieldSchema(field reflect.StructField) (map[string]any, bool, error) {
	prop, err := typeSchema(field.Type)
	if err != nil {
		return nil, false, err
	}
	if doc := field.Tag.Get("doc"); doc != "" {
		prop["description"] = doc
	} else if desc := field.Tag.Get("description"); desc != "" {
		prop["description"] = desc
	}
	required := false
	if js := field.Tag.Get("jsonschema"); js != "" {
		for _, entry := range strings.Split(js, ",") {
			entry = strings.TrimSpace(entry)
			if entry == "required" {
				required = true
				continue
			}
			if key, val, ok := strings.Cut(entry, "="); ok {
				prop[key] = coerceScalar(val)
			}
		}
	}
	if def := field.Tag.Get("default"); def != "" {
		prop["default"] = coerceTyped(def, field.Type)
	}
	if enum := field.Tag.Get("enum"); enum != "" {
		values := strings.Split(enum, ",")
		anyVals := make([]any, len(values))
		for i, v := range values {
			anyVals[i] = v
		}
		// For arrays the enum constrains the item type.
		if items, ok := prop["items"].(map[string]any); ok {
			items["enum"] = anyVals
		} else {
			prop["enum"] = anyVals
		}
	}
	return prop, required, nil
}

func typeSchema(t reflect.Type) (map[string]any, error) {
	for t.Kind() == reflect.Pointer {
		t = t.Elem()
	}
	switch t.Kind() {
	case reflect.String:
		return map[string]any{"type": "string"}, nil
	case reflect.Bool:
		return map[string]any{"type": "boolean"}, nil
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64,
		reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		return map[string]any{"type": "integer"}, nil
	case reflect.Float32, reflect.Float64:
		return map[string]any{"type": "number"}, nil
	case reflect.Slice, reflect.Array:
		if t.Elem().Kind() == reflect.Uint8 {
			// []byte serializes to a base64 string.
			return map[string]any{"type": "string", "contentEncoding": "base64"}, nil
		}
		items, err := typeSchema(t.Elem())
		if err != nil {
			return nil, err
		}
		return map[string]any{"type": "array", "items": items}, nil
	case reflect.Map:
		return map[string]any{"type": "object"}, nil
	case reflect.Struct:
		return structSchema(t)
	case reflect.Interface:
		return map[string]any{}, nil
	default:
		return nil, fmt.Errorf("unsupported type %s", t)
	}
}

// coerceScalar converts a tag value into a JSON-friendly scalar.
func coerceScalar(v string) any {
	if n, err := strconv.ParseInt(v, 10, 64); err == nil {
		return n
	}
	if f, err := strconv.ParseFloat(v, 64); err == nil {
		return f
	}
	if b, err := strconv.ParseBool(v); err == nil {
		return b
	}
	return v
}

// coerceTyped converts a default tag according to the field's Go type.
func coerceTyped(v string, t reflect.Type) any {
	for t.Kind() == reflect.Pointer {
		t = t.Elem()
	}
	switch t.Kind() {
	case reflect.String:
		return v
	case reflect.Bool:
		b, _ := strconv.ParseBool(v)
		return b
	case reflect.Float32, reflect.Float64:
		f, _ := strconv.ParseFloat(v, 64)
		return f
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64,
		reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		n, _ := strconv.ParseInt(v, 10, 64)
		return n
	default:
		var out any
		if err := json.Unmarshal([]byte(v), &out); err == nil {
			return out
		}
		return v
	}
}
