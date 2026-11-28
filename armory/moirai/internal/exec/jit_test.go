package exec

import (
	"testing"

	"domscript/internal/jit"
	"domscript/internal/runtime"
)

func TestJITTraceExecutionMatchesInterpreter(t *testing.T) {
	const source = `
let i = 0
while i < 100 {
    i = i + 1
}
i
`

	jitOptions := DefaultJITOptions()
	jitOptions.HotThreshold = 1
	jitOptions.TraceConfig = jit.DefaultTraceConfig()

	jitState := runtime.NewState()
	if err := ConfigureJIT(jitState, jitOptions); err != nil {
		t.Fatal(err)
	}
	jitValue, err := EvalChunk(jitState, source, "jit-test.doml")
	if err != nil {
		t.Fatalf("JIT run failed: %v", err)
	}
	if got := jitValue.IntValue(); got != 100 {
		t.Fatalf("JIT result = %d, want 100", got)
	}
	stats, ok := JITStats(jitState)
	if !ok || stats.TracesCompiled == 0 {
		t.Fatalf("JIT did not compile a trace: %+v", stats)
	}

	interpreterState := runtime.NewState()
	DisableJIT(interpreterState)
	interpreterValue, err := EvalChunk(interpreterState, source, "interpreter-test.doml")
	if err != nil {
		t.Fatalf("interpreter run failed: %v", err)
	}
	if !runtime.Equals(jitValue, interpreterValue) {
		t.Fatalf("JIT result %v differs from interpreter result %v", jitValue, interpreterValue)
	}
}

func TestJITCanBeDisabled(t *testing.T) {
	st := runtime.NewState()
	DisableJIT(st)
	if _, err := EvalChunk(st, "1 + 2", "jit-disabled.doml"); err != nil {
		t.Fatal(err)
	}
	if _, ok := JITStats(st); ok {
		t.Fatal("disabled JIT should not expose compiler stats")
	}
}
