package traceback

import (
	"runtime"
	"strings"

	"domscript/internal/ffi"
	druntime "domscript/internal/runtime"
)

func Register(st *druntime.State) {
	reg := func(name string, fn druntime.GoNative) {
		ffi.Register(st, "__traceback_"+name, fn)
	}

	reg("current", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		buf := make([]byte, 8192)
		n := runtime.Stack(buf, false)
		return druntime.Str(string(buf[:n])), nil
	})

	reg("format", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		buf := make([]byte, 8192)
		n := runtime.Stack(buf, true)
		return druntime.Str(string(buf[:n])), nil
	})

	reg("extract", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		buf := make([]byte, 8192)
		n := runtime.Stack(buf, false)
		lines := strings.Split(string(buf[:n]), "\n")
		var elems []druntime.Value
		for _, l := range lines {
			l = strings.TrimSpace(l)
			if l != "" {
				elems = append(elems, druntime.Str(l))
			}
		}
		return druntime.ListOf(elems), nil
	})

	reg("last_error", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		if st.LastError != nil {
			return druntime.TaggedOf("Some", []druntime.Value{druntime.Str(st.LastError.Error())}), nil
		}
		return druntime.TaggedOf("None", nil), nil
	})
}
