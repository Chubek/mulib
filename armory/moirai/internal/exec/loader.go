// Package exec: loader.go — module loading.
//
// A module has a name, a .doml source half, and an optional native half
// registered through internal/ffi. The loader evaluates the .doml half
// and caches its result per state. Circular imports are detected and
// reported through internal/diag, not deadlocked.
package exec

import (
	"fmt"
	"os"
	"strings"

	"domscript/internal/ast"
	"domscript/internal/diag"
	"domscript/internal/parser"
	"domscript/internal/runtime"
)

// LoadModule evaluates a module's .doml source and caches its exports.
// The native half must be registered before calling LoadModule.
// baseDir is the directory containing module roots.
func LoadModule(st *runtime.State, name, baseDir string) (*runtime.Module, error) {
	if m, ok := st.LookupModule(name); ok {
		return m, nil
	}

	// Track in-progress loads for circular-import detection.
	if st.Modules == nil {
		st.Modules = make(map[string]any)
	}
	loadingKey := "_loading_" + name
	if _, ok := st.Modules[loadingKey]; ok {
		return nil, fmt.Errorf("circular import of module %q", name)
	}
	st.Modules[loadingKey] = true
	defer delete(st.Modules, loadingKey)

	srcPath, ok := moduleFilePath(baseDir, name)
	if !ok {
		return nil, fmt.Errorf("module %q not found in %q", name, baseDir)
	}
	src, err := os.ReadFile(srcPath)
	if err != nil {
		return nil, fmt.Errorf("module %q: %v", name, err)
	}

	// Parse first so we reject program files before evaluating side effects.
	parseDiags := diag.NewSet(srcPath, string(src))
	prog := parser.Parse(string(src), srcPath, parseDiags)
	if parseDiags.HasErrors() {
		return nil, fmt.Errorf("module %q: %v", name, parseDiags.RenderAll())
	}
	if err := ensureModuleDeclaration(name, prog); err != nil {
		return nil, fmt.Errorf("module %q: %v", name, err)
	}

	// Evaluate the module source.
	_, err = EvalChunk(st, string(src), srcPath)
	if err != nil {
		return nil, fmt.Errorf("module %q: %v", name, err)
	}

	// A module's namespace is its qualified declaration (for example,
	// std.mapmod). Its export table is a normal Domscript binding, so it need
	// not have the same spelling as the final namespace segment: std.mapmod
	// deliberately exports `map`. Read that binding from the parsed module
	// instead of guessing it from the module name.
	exportName, err := moduleExportBinding(prog)
	if err != nil {
		return nil, fmt.Errorf("module %q: %v", name, err)
	}
	exports, ok := st.LookupGlobal(exportName)
	if !ok {
		return nil, fmt.Errorf("module %q did not define export table", name)
	}
	if exports.Kind() != runtime.KindTable {
		return nil, fmt.Errorf("module %q export %q is not a table", name, exportName)
	}

	m := &runtime.Module{Name: name, Exports: exports}
	st.RegisterModule(m)
	return m, nil
}

func moduleExportName(moduleName string) string {
	parts := strings.Split(moduleName, ".")
	if len(parts) == 0 || parts[len(parts)-1] == "" {
		return moduleName
	}
	return parts[len(parts)-1]
}

// moduleExportBinding returns the single top-level table declaration that
// defines a module's public namespace. It deliberately uses the parsed AST so
// comments and native function names cannot affect module loading.
func moduleExportBinding(prog *ast.Program) (string, error) {
	var name string
	for _, stmt := range prog.Stmts {
		let, ok := stmt.(*ast.Let)
		if !ok {
			continue
		}
		if _, ok := let.Value.(*ast.TableLit); !ok {
			continue
		}
		if name != "" {
			return "", fmt.Errorf("multiple export tables (%q and %q)", name, let.Name)
		}
		name = let.Name
	}
	if name == "" {
		return "", fmt.Errorf("did not define an export table")
	}
	return name, nil
}

func ensureModuleDeclaration(moduleName string, prog *ast.Program) error {
	var mod *ast.Module
	for _, st := range prog.Stmts {
		switch s := st.(type) {
		case *ast.ProgramDecl:
			return fmt.Errorf("cannot import a program")
		case *ast.Module:
			if mod != nil {
				return fmt.Errorf("multiple module declarations")
			}
			mod = s
			if s.Name != moduleName {
				return fmt.Errorf("module file declares %q, expected %q", s.Name, moduleName)
			}
		}
	}
	if mod == nil {
		return fmt.Errorf("file has no module declaration")
	}
	return nil
}
