package benchmark

import (
	"testing"

	"domscript/internal/diag"
	"domscript/internal/exec"
	"domscript/internal/ir"
	"domscript/internal/lexer"
	"domscript/internal/parser"
	"domscript/internal/runtime"
	"domscript/internal/sema"
)

const loopProgram = `
let total = 0
let i = 0
while i < 1000 {
    total = total + i
    i = i + 1
}
total
`

func BenchmarkLoop(b *testing.B) {
	for _, tc := range []struct {
		name      string
		enabled   bool
		threshold int
		maxInstrs int
		guards    bool
	}{
		{name: "Interpreter", enabled: false},
		{name: "JITThreshold1", enabled: true, threshold: 1, maxInstrs: 64, guards: true},
		{name: "JITThreshold10", enabled: true, threshold: 10, maxInstrs: 128, guards: true},
		{name: "JITThreshold50", enabled: true, threshold: 50, maxInstrs: 256, guards: true},
		{name: "JITNoGuards", enabled: true, threshold: 10, maxInstrs: 128, guards: false},
	} {
		b.Run(tc.name, func(b *testing.B) {
			chunk := benchmarkChunk(b)
			st := runtime.NewState()
			options := exec.DefaultJITOptions()
			options.Enabled = tc.enabled
			options.HotThreshold = tc.threshold
			if tc.maxInstrs > 0 {
				options.TraceConfig.MaxInstrs = tc.maxInstrs
				options.TraceConfig.RecordGuards = tc.guards
			}
			if err := exec.ConfigureJIT(st, options); err != nil {
				b.Fatal(err)
			}

			if _, err := exec.RunChunk(st, chunk); err != nil {
				b.Fatal(err)
			}

			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				value, err := exec.RunChunk(st, chunk)
				if err != nil {
					b.Fatal(err)
				}
				if value.IntValue() != 499500 {
					b.Fatalf("result = %d, want 499500", value.IntValue())
				}
			}
			if stats, ok := exec.JITStats(st); ok {
				b.ReportMetric(float64(stats.TracesCompiled), "traces")
				b.ReportMetric(float64(stats.NativeAttempts), "native-attempts")
				b.ReportMetric(float64(stats.NativeCompiled), "native-compiled")
				b.ReportMetric(float64(stats.NativeRunAttempts), "native-run-attempts")
				b.ReportMetric(float64(stats.NativeOverflows), "native-overflows")
				b.ReportMetric(float64(stats.NativeExecs), "native-execs")
				b.ReportMetric(float64(stats.BytesCompiled), "jit-bytes")
			}
		})
	}
}

func benchmarkChunk(b *testing.B) *ir.Func {
	b.Helper()
	diags := diag.NewSet("benchmark.doml", loopProgram)
	_ = lexer.Scan(loopProgram, "benchmark.doml", diags)
	program := parser.Parse(loopProgram, "benchmark.doml", diags)
	if diags.HasErrors() {
		b.Fatalf("parse failed: %s", diags.RenderAll())
	}
	state := runtime.NewState()
	info := sema.Check(program, state.RegisteredNames(), nil, diags)
	if diags.HasErrors() {
		b.Fatalf("sema failed: %s", diags.RenderAll())
	}
	chunk, err := exec.Compile(program, info, state, true)
	if err != nil {
		b.Fatalf("compile failed: %v", err)
	}
	return chunk
}
