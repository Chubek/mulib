package exec

import (
	"testing"

	"domscript/internal/diag"
	"domscript/internal/parser"
	"domscript/internal/runtime"
	"domscript/internal/sema"
)

func TestCompileFunctionParameters(t *testing.T) {
	const src = `
fn add(x, y) { x + y }
add(20, 22)
`

	diags := diag.NewSet("compile-test.doml", src)
	prog := parser.Parse(src, "compile-test.doml", diags)
	if diags.HasErrors() {
		t.Fatalf("parse failed: %s", diags.RenderAll())
	}
	info := sema.Check(prog, nil, nil, diags)
	if diags.HasErrors() {
		t.Fatalf("sema failed: %s", diags.RenderAll())
	}

	st := runtime.NewState()
	chunk, err := Compile(prog, info, st, false)
	if err != nil {
		t.Fatalf("compile failed: %v", err)
	}

	got, err := RunChunk(st, chunk)
	if err != nil {
		t.Fatalf("run failed: %v", err)
	}
	if got.IntValue() != 42 {
		t.Fatalf("got %v, want 42", got)
	}
}
