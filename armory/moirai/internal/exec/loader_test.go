package exec

import (
	"os"
	"path/filepath"
	"testing"

	"domscript/internal/runtime"
	"domscript/stdlib"
)

func TestLoadNamespacedModuleUsesLeafExportBinding(t *testing.T) {
	root := t.TempDir()
	dir := filepath.Join(root, "stringmod")
	if err := os.MkdirAll(dir, 0755); err != nil {
		t.Fatal(err)
	}
	src := "module std.stringmod\n\nlet stringmod = %{ value => 42, }\n"
	if err := os.WriteFile(filepath.Join(dir, "stringmod.doml"), []byte(src), 0644); err != nil {
		t.Fatal(err)
	}

	mod, err := LoadModule(runtime.NewState(), "std.stringmod", root)
	if err != nil {
		t.Fatalf("LoadModule: %v", err)
	}
	if mod.Name != "std.stringmod" {
		t.Fatalf("module name = %q", mod.Name)
	}
	value, ok := mod.Exports.Table().Get(runtime.Str("value"))
	if !ok || value.IntValue() != 42 {
		t.Fatalf("exports.value = %v, want 42", value)
	}
}

func TestModuleExportName(t *testing.T) {
	for _, tc := range []struct{ module, want string }{
		{"std.stringmod", "stringmod"},
		{"foo", "foo"},
	} {
		if got := moduleExportName(tc.module); got != tc.want {
			t.Errorf("moduleExportName(%q) = %q, want %q", tc.module, got, tc.want)
		}
	}
}

func TestLoadModuleUsesDeclaredExportBinding(t *testing.T) {
	root := t.TempDir()
	dir := filepath.Join(root, "mapmod")
	if err := os.MkdirAll(dir, 0755); err != nil {
		t.Fatal(err)
	}
	src := "module std.mapmod\n\nlet map = %{ value => 42, }\n"
	if err := os.WriteFile(filepath.Join(dir, "mapmod.doml"), []byte(src), 0644); err != nil {
		t.Fatal(err)
	}

	mod, err := LoadModule(runtime.NewState(), "std.mapmod", root)
	if err != nil {
		t.Fatalf("LoadModule: %v", err)
	}
	value, ok := mod.Exports.Table().Get(runtime.Str("value"))
	if !ok || value.IntValue() != 42 {
		t.Fatalf("exports.value = %v, want 42", value)
	}
}

func TestStandardLibraryStringModuleImports(t *testing.T) {
	st := runtime.NewState()
	stdlib.RegisterAll(st)
	if _, err := EvalChunk(st, "import std.stringmod", "<test>"); err != nil {
		t.Fatalf("import std.stringmod: %v", err)
	}
	if _, ok := st.LookupGlobal("stringmod"); !ok {
		t.Fatal("stringmod import alias was not bound")
	}
}
