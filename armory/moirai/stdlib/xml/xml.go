package xml

import (
	"encoding/xml"
	"fmt"
	"os"
	"strings"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__xml_"+name, fn)
	}

	reg("encode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("encode expects a value")
		}
		goVal := xmlToGo(args[0])
		data, err := xml.Marshal(goVal)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("decode", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("decode expects a :String")
		}
		var goVal xmlNode
		err := xml.Unmarshal([]byte(args[0].StrValue()), &goVal)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{goFromXML(goVal)}), nil
	})

	reg("pretty", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("pretty expects a value")
		}
		goVal := xmlToGo(args[0])
		data, err := xml.MarshalIndent(goVal, "", "  ")
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(data))}), nil
	})

	reg("validate", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("validate expects a :String")
		}
		var goVal xmlNode
		err := xml.Unmarshal([]byte(args[0].StrValue()), &goVal)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Bool(true)}), nil
	})

	reg("encode_to_file", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("encode_to_file expects a value and :String path")
		}
		goVal := xmlToGo(args[0])
		data, err := xml.MarshalIndent(goVal, "", "  ")
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
		var goVal xmlNode
		if err := xml.Unmarshal(data, &goVal); err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{goFromXML(goVal)}), nil
	})
}

type xmlNode struct {
	XMLName xml.Name
	Attrs   []xmlAttr  `xml:",any,attr"`
	Content string     `xml:",chardata"`
	Nodes   []xmlNode  `xml:",any"`
}

type xmlAttr struct {
	XMLName xml.Name
	Value   string `xml:",attr"`
}

func xmlToGo(v runtime.Value) any {
	if v.Kind() != runtime.KindTable {
		return runtime.Display(v)
	}
	t := v.Table()
	nameV, _ := t.Get(runtime.Str("name"))
	attrsV, _ := t.Get(runtime.Str("attrs"))
	childrenV, _ := t.Get(runtime.Str("children"))
	textV, _ := t.Get(runtime.Str("text"))

	node := xmlNode{}
	if nameV.Kind() == runtime.KindString {
		node.XMLName = xml.Name{Local: nameV.StrValue()}
	}
	if attrsV.Kind() == runtime.KindTable {
		attrsV.Table().Entries(func(k, val runtime.Value) {
			node.Attrs = append(node.Attrs, xmlAttr{
				XMLName: xml.Name{Local: runtime.Display(k)},
				Value:   runtime.Display(val),
			})
		})
	}
	if textV.Kind() == runtime.KindString {
		node.Content = textV.StrValue()
	}
	if childrenV.Kind() == runtime.KindList {
		for _, c := range childrenV.List().Elems {
			if n, ok := xmlToGo(c).(xmlNode); ok {
				node.Nodes = append(node.Nodes, n)
			}
		}
	}
	return node
}

func goFromXML(node xmlNode) runtime.Value {
	t := runtime.NewTable()
	t.Set(runtime.Str("name"), runtime.Str(node.XMLName.Local))
	t.Set(runtime.Str("text"), runtime.Str(strings.TrimSpace(node.Content)))
	attrT := runtime.NewTable()
	for _, a := range node.Attrs {
		attrT.Set(runtime.Str(a.XMLName.Local), runtime.Str(a.Value))
	}
	t.Set(runtime.Str("attrs"), runtime.TableValue(attrT))
	children := make([]runtime.Value, len(node.Nodes))
	for i, n := range node.Nodes {
		children[i] = goFromXML(n)
	}
	t.Set(runtime.Str("children"), runtime.ListOf(children))
	return runtime.TableValue(t)
}
