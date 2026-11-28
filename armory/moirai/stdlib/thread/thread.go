package thread

import (
	"fmt"
	"runtime"
	"time"

	"domscript/internal/ffi"
	druntime "domscript/internal/runtime"
)

func Register(st *druntime.State) {
	reg := func(name string, fn druntime.GoNative) {
		ffi.Register(st, "__thread_"+name, fn)
	}

	reg("spawn", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		if len(args) < 1 || args[0].Kind() != druntime.KindFn {
			return druntime.Unit(), fmt.Errorf("spawn expects a :Fn")
		}
		fn := args[0].Fn()
		go func() {
			_, _ = st.CallDomscript(fn, nil)
		}()
		return druntime.Unit(), nil
	})

	reg("sleep", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		if len(args) < 1 {
			return druntime.Unit(), fmt.Errorf("sleep expects duration in seconds")
		}
		d := time.Duration(args[0].Numeric() * float64(time.Second))
		time.Sleep(d)
		return druntime.Unit(), nil
	})

	reg("num_cpus", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Int(int64(runtime.NumCPU())), nil
	})

	reg("goroutines", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Int(int64(runtime.NumGoroutine())), nil
	})

	reg("version", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		return druntime.Str(runtime.Version()), nil
	})
}
