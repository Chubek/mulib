package debug

import (
	"bytes"
	"fmt"
	"runtime"
	"runtime/debug"
	"strconv"

	"domscript/internal/ffi"
	druntime "domscript/internal/runtime"
)

func Register(st *druntime.State) {
	reg := func(name string, fn druntime.GoNative) {
		ffi.Register(st, "__debug_"+name, fn)
	}

	reg("stack", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		buf := make([]byte, 4096)
		n := runtime.Stack(buf, false)
		return druntime.Str(string(buf[:n])), nil
	})

	reg("mem_stats", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		var m runtime.MemStats
		runtime.ReadMemStats(&m)
		t := druntime.NewTable()
		t.Set(druntime.Str("alloc"), druntime.Int(int64(m.Alloc)))
		t.Set(druntime.Str("total_alloc"), druntime.Int(int64(m.TotalAlloc)))
		t.Set(druntime.Str("sys"), druntime.Int(int64(m.Sys)))
		t.Set(druntime.Str("num_gc"), druntime.Int(int64(m.NumGC)))
		t.Set(druntime.Str("heap_alloc"), druntime.Int(int64(m.HeapAlloc)))
		t.Set(druntime.Str("heap_sys"), druntime.Int(int64(m.HeapSys)))
		t.Set(druntime.Str("heap_objects"), druntime.Int(int64(m.HeapObjects)))
		t.Set(druntime.Str("goroutines"), druntime.Int(int64(runtime.NumGoroutine())))
		return druntime.TableValue(t), nil
	})

	reg("gc", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		runtime.GC()
		return druntime.Unit(), nil
	})

	reg("set_gc_percent", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		if len(args) < 1 || args[0].Kind() != druntime.KindInt {
			return druntime.Unit(), fmt.Errorf("set_gc_percent expects an :Int")
		}
		old := debug.SetGCPercent(int(args[0].IntValue()))
		return druntime.Int(int64(old)), nil
	})

	reg("free_os_memory", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		debug.FreeOSMemory()
		return druntime.Unit(), nil
	})

	reg("build_info", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		info, ok := debug.ReadBuildInfo()
		if !ok {
			return druntime.TaggedOf("None", nil), nil
		}
		t := druntime.NewTable()
		t.Set(druntime.Str("go_version"), druntime.Str(info.GoVersion))
		t.Set(druntime.Str("path"), druntime.Str(info.Path))
		t.Set(druntime.Str("main"), druntime.Str(info.Main.Path))
		return druntime.TaggedOf("Some", []druntime.Value{druntime.TableValue(t)}), nil
	})

	reg("breakpoint", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		runtime.Breakpoint()
		return druntime.Unit(), nil
	})

	reg("goroutine_id", func(st *druntime.State, args []druntime.Value) (druntime.Value, error) {
		var buf [64]byte
		n := runtime.Stack(buf[:], false)
		fields := bytes.Fields(buf[:n])
		if len(fields) < 2 {
			return druntime.Int(0), nil
		}
		id, err := strconv.ParseInt(string(fields[1]), 10, 64)
		if err != nil {
			return druntime.Unit(), fmt.Errorf("goroutine_id parse failed: %w", err)
		}
		return druntime.Int(id), nil
	})
}
