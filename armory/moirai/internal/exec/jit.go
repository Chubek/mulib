package exec

import (
	"sync"

	"domscript/internal/jit"
	"domscript/internal/runtime"
)

// JITOptions controls the runtime trace tier for one interpreter state.
type JITOptions struct {
	Enabled      bool
	HotThreshold int
	TraceConfig  jit.TraceConfig
	MaxCodeSize  int
}

// DefaultJITOptions returns the default runtime JIT settings.
func DefaultJITOptions() JITOptions {
	return JITOptions{
		Enabled:      true,
		HotThreshold: 50,
		TraceConfig:  jit.DefaultTraceConfig(),
		MaxCodeSize:  1 << 20,
	}
}

type jitState struct {
	compiler *jit.Compiler
}

var stateJIT sync.Map

// ConfigureJIT installs JIT settings for st. A disabled configuration makes
// all subsequent VMs for the state use the baseline interpreter.
func ConfigureJIT(st *runtime.State, options JITOptions) error {
	if !options.Enabled {
		stateJIT.Store(st, &jitState{})
		return nil
	}
	compiler, err := jit.NewHostCompiler()
	if err != nil {
		return err
	}
	if options.HotThreshold > 0 {
		compiler.HotThreshold = options.HotThreshold
	}
	if options.TraceConfig.MaxInstrs > 0 {
		compiler.TraceCfg = options.TraceConfig
	}
	if options.MaxCodeSize > 0 {
		compiler.MaxCodeSize = options.MaxCodeSize
	}
	stateJIT.Store(st, &jitState{compiler: compiler})
	return nil
}

// DisableJIT disables the trace tier for st.
func DisableJIT(st *runtime.State) {
	stateJIT.Store(st, &jitState{})
}

// JITStats returns a snapshot of the compiler statistics for st.
func JITStats(st *runtime.State) (jit.CompilerStats, bool) {
	value, ok := stateJIT.Load(st)
	if !ok {
		return jit.CompilerStats{}, false
	}
	config := value.(*jitState)
	if config.compiler == nil {
		return jit.CompilerStats{}, false
	}
	return config.compiler.StatsSnapshot(), true
}

func compilerFor(st *runtime.State) *jit.Compiler {
	if value, ok := stateJIT.Load(st); ok {
		return value.(*jitState).compiler
	}
	compiler, err := jit.NewHostCompiler()
	if err != nil {
		return nil
	}
	actual := &jitState{compiler: compiler}
	value, _ := stateJIT.LoadOrStore(st, actual)
	return value.(*jitState).compiler
}
