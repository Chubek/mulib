package sync

import (
	"fmt"
	stdsync "sync"
	"sync/atomic"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

var counter int64
var (
	mutexMu      stdsync.Mutex
	mutexStates  = make(map[*runtime.Table]*stdsync.Mutex)
	waitgroupMap = make(map[*runtime.Table]*stdsync.WaitGroup)
)

func mutexState(t *runtime.Table) *stdsync.Mutex {
	mutexMu.Lock()
	defer mutexMu.Unlock()
	if m, ok := mutexStates[t]; ok {
		return m
	}
	m := &stdsync.Mutex{}
	mutexStates[t] = m
	return m
}

func waitgroupState(t *runtime.Table) *stdsync.WaitGroup {
	mutexMu.Lock()
	defer mutexMu.Unlock()
	if wg, ok := waitgroupMap[t]; ok {
		return wg
	}
	wg := &stdsync.WaitGroup{}
	waitgroupMap[t] = wg
	return wg
}

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__sync_"+name, fn)
	}

	reg("mutex_new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		t := runtime.NewTable()
		t.Set(runtime.Str("__locked__"), runtime.Bool(false))
		return runtime.TableValue(t), nil
	})

	reg("mutex_lock", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("mutex_lock expects a mutex :Table")
		}
		mutexState(args[0].Table()).Lock()
		args[0].Table().Set(runtime.Str("__locked__"), runtime.Bool(true))
		return runtime.Unit(), nil
	})

	reg("mutex_unlock", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("mutex_unlock expects a mutex :Table")
		}
		mutexState(args[0].Table()).Unlock()
		args[0].Table().Set(runtime.Str("__locked__"), runtime.Bool(false))
		return runtime.Unit(), nil
	})

	reg("mutex_try_lock", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("mutex_try_lock expects a mutex :Table")
		}
		m := mutexState(args[0].Table())
		if !m.TryLock() {
			return runtime.Bool(false), nil
		}
		args[0].Table().Set(runtime.Str("__locked__"), runtime.Bool(true))
		return runtime.Bool(true), nil
	})

	reg("atomic_load", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(atomic.LoadInt64(&counter)), nil
	})

	reg("atomic_add", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("atomic_add expects an :Int")
		}
		return runtime.Int(atomic.AddInt64(&counter, args[0].IntValue())), nil
	})

	reg("atomic_store", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("atomic_store expects an :Int")
		}
		atomic.StoreInt64(&counter, args[0].IntValue())
		return runtime.Unit(), nil
	})

	reg("atomic_swap", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("atomic_swap expects an :Int")
		}
		return runtime.Int(atomic.SwapInt64(&counter, args[0].IntValue())), nil
	})

	reg("atomic_cas", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindInt || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("atomic_cas expects old and new :Ints")
		}
		swapped := atomic.CompareAndSwapInt64(&counter, args[0].IntValue(), args[1].IntValue())
		return runtime.Bool(swapped), nil
	})

	reg("waitgroup_new", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		t := runtime.NewTable()
		t.Set(runtime.Str("__count__"), runtime.Int(0))
		waitgroupState(t)
		return runtime.TableValue(t), nil
	})

	reg("waitgroup_add", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindTable || args[1].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("waitgroup_add expects a waitgroup :Table and an :Int delta")
		}
		t := args[0].Table()
		wg := waitgroupState(t)
		delta := int(args[1].IntValue())
		defer func() {
			_ = recover()
		}()
		wg.Add(delta)
		cV, _ := t.Get(runtime.Str("__count__"))
		t.Set(runtime.Str("__count__"), runtime.Int(cV.IntValue()+args[1].IntValue()))
		return runtime.Unit(), nil
	})

	reg("waitgroup_done", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("waitgroup_done expects a waitgroup :Table")
		}
		t := args[0].Table()
		wg := waitgroupState(t)
		wg.Done()
		cV, _ := t.Get(runtime.Str("__count__"))
		t.Set(runtime.Str("__count__"), runtime.Int(cV.IntValue()-1))
		return runtime.Unit(), nil
	})

	reg("waitgroup_wait", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindTable {
			return runtime.Unit(), fmt.Errorf("waitgroup_wait expects a waitgroup :Table")
		}
		waitgroupState(args[0].Table()).Wait()
		return runtime.Unit(), nil
	})
}
