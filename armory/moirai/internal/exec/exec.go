// Package exec: exec.go — the orchestration layer a state uses to run
// compiled chunks. It handles chunk evaluation, coroutine scheduling,
// pump processing, and event-handler dispatch.
package exec

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"domscript/internal/ast"
	"domscript/internal/diag"
	"domscript/internal/ir"
	"domscript/internal/lexer"
	"domscript/internal/parser"
	"domscript/internal/runtime"
	"domscript/internal/sema"
	"domscript/stdlib"
)

var vmPool sync.Pool

// EvalChunk compiles and evaluates src in the given state. It returns the
// value of the last expression (or Unit) and any error. If the chunk
// suspends on an async effect, the returned AsyncSuspend carries the
// token the host should resolve.
func EvalChunk(st *runtime.State, src, chunkName string) (runtime.Value, error) {
	return EvalChunkWithOptions(st, src, chunkName)
}

// EvalChunkWithOptions compiles and evaluates src using the state's parse
// configuration.
func EvalChunkWithOptions(st *runtime.State, src, chunkName string) (runtime.Value, error) {
	diags := diag.NewSet(chunkName, src)

	// Lex and parse.
	toks := lexer.Scan(src, chunkName, diags)
	_ = toks
	prog := parser.Parse(src, chunkName, diags, parser.ParseOptions{XST: st.XST})
	if diags.HasErrors() {
		return runtime.Unit(), diag.NewErrorWithSource(diags)
	}
	if err := ResolveImports(st, prog, chunkName); err != nil {
		return runtime.Unit(), err
	}

	// Grammar `Parse` helper (internal/grammar over dependencies/libglr).
	ensureParseNative(st)

	// Sema.
	info := sema.Check(prog, st.RegisteredNames(), st.ChunkGlobalNames(), diags)
	if diags.HasErrors() {
		return runtime.Unit(), diag.NewErrorWithSource(diags)
	}

	// Compile.
	chunk, err := Compile(prog, info, st, true)
	if err != nil {
		return runtime.Unit(), fmt.Errorf("compile: %v", err)
	}

	// Register static grammar/absyn shapes before running so mid-chunk
	// DeriveParser/DeriveTree resolve; custom lexeme closures are patched
	// after (they live in hidden `__grammar_G_L` globals).
	if err := RegisterGrammarsStatic(st, prog); err != nil {
		return runtime.Unit(), err
	}

	// Run.
	v, err := RunChunk(st, chunk)
	if err != nil {
		return runtime.Unit(), err
	}
	if err := PatchGrammarFns(st, prog); err != nil {
		return runtime.Unit(), err
	}
	return v, nil
}

// resolveImports loads each top-level import and binds its namespace in the
// state's ordinary global environment. Native halves are expected to have
// been registered by the host (stdlib.RegisterAll does this for stdlib).
func ResolveImports(st *runtime.State, prog *ast.Program, chunkName string) error {
	for _, stmt := range prog.Stmts {
		im, ok := stmt.(*ast.Import)
		if !ok {
			continue
		}
		base, err := importBase(im.Library, im.Module, chunkName)
		if err != nil {
			return err
		}
		mod, err := LoadModule(st, im.Module, base)
		if err != nil {
			return err
		}
		// Selective `import m : a, b` binds each name from the export
		// table directly (used by `import std.io : read_file, ...`).
		if len(im.Symbols) > 0 {
			if mod.Exports.Kind() != runtime.KindTable {
				return fmt.Errorf("module %q exports are not a table", im.Module)
			}
			for _, sym := range im.Symbols {
				v, ok := mod.Exports.Table().Get(runtime.Tag(sym))
				if !ok {
					v, ok = mod.Exports.Table().Get(runtime.Str(sym))
				}
				if !ok {
					return fmt.Errorf("module %q has no export %q", im.Module, sym)
				}
				st.BindGlobal(sym, v)
			}
			continue
		}
		name := im.Alias
		if name == "" {
			name = importAlias(im.Module)
		}
		st.BindGlobal(name, mod.Exports)
	}
	return nil
}

func importBase(library, module, chunkName string) (string, error) {
	if library != "" {
		candidates := []string{library}
		if library == "stdlib" {
			candidates = stdlib.ModuleRoots()
			if len(candidates) == 0 {
				candidates = []string{"stdlib"}
			}
		}
		for _, base := range candidates {
			if _, ok := moduleSourcePath(base, module); ok {
				return base, nil
			}
		}
		return "", fmt.Errorf("cannot locate module %q in library %q", module, library)
	}
	candidates := []string{}
	if chunkName != "" && chunkName[0] != '<' {
		candidates = append(candidates, filepath.Dir(chunkName))
	}
	candidates = append(candidates, stdlib.ModuleRoots()...)
	candidates = append(candidates, ".")
	for _, base := range candidates {
		if _, ok := moduleSourcePath(base, module); ok {
			return base, nil
		}
	}
	return "", fmt.Errorf("cannot locate module %q", module)
}

func importAlias(moduleName string) string {
	return moduleExportName(moduleName)
}

// moduleSourcePath returns the .doml path for name under base, if present.
func moduleSourcePath(base, module string) (string, bool) {
	path, ok := moduleFilePath(base, module)
	if !ok {
		return "", false
	}
	st, err := os.Stat(path)
	if err != nil || st.IsDir() {
		return "", false
	}
	return path, true
}

func moduleFilePath(base, module string) (string, bool) {
	parts := strings.Split(module, ".")
	if len(parts) == 0 || parts[0] == "" {
		return "", false
	}
	if len(parts) > 1 && parts[0] == "std" {
		parts = parts[1:]
	}
	if len(parts) == 0 {
		return "", false
	}
	filename := parts[len(parts)-1] + ".doml"
	dir := filepath.Join(base, filepath.Join(parts...))
	return filepath.Join(dir, filename), true
}

// RunChunk executes a compiled chunk against the state's global env.
func RunChunk(st *runtime.State, chunk *ir.Func) (runtime.Value, error) {
	vm := acquireVM(st)
	defer releaseVM(vm)

	// The chunk activation uses the state's global env.
	st.Global.Grow(chunk.NLocals)
	act := &activation{fn: chunk, env: st.Global, ip: 0}
	vm.frames = append(vm.frames, act)

	result, err := vm.Run()
	// Make top-level bindings visible to subsequent chunks and the module
	// loader using the same slots allocated by the compiler.
	st.InstallGlobals(ChunkGlobals(chunk))
	return result, err
}

func acquireVM(st *runtime.State) *VM {
	if v := vmPool.Get(); v != nil {
		vm := v.(*VM)
		vm.st = st
		vm.jit = compilerFor(st)
		if vm.jitHeads == nil {
			vm.jitHeads = make(map[*ir.Func]map[int]struct{})
		}
		vm.frames = vm.frames[:0]
		vm.stack = vm.stack[:0]
		vm.sp = 0
		vm.raiseErr = nil
		vm.iters = vm.iters[:0]
		vm.nodeStack = vm.nodeStack[:0]
		vm.nativeLocals = vm.nativeLocals[:0]
		vm.nativeStack = vm.nativeStack[:0]
		return vm
	}
	return NewVM(st)
}

func releaseVM(vm *VM) {
	if vm == nil {
		return
	}
	vm.st = nil
	vm.jit = nil
	vm.frames = vm.frames[:0]
	vm.stack = vm.stack[:0]
	vm.sp = 0
	vm.raiseErr = nil
	vm.iters = vm.iters[:0]
	vm.nodeStack = vm.nodeStack[:0]
	vm.nativeLocals = vm.nativeLocals[:0]
	vm.nativeStack = vm.nativeStack[:0]
	vmPool.Put(vm)
}

// CallFn invokes a Domscript function value with arguments, under effect
// semantics. This is the hook installed into the runtime so event
// dispatch and api.Call can invoke script functions.
func CallFn(st *runtime.State, fn *runtime.Fn, args []runtime.Value) (runtime.Value, error) {
	vm := NewVM(st)

	// Push the function and args, then call.
	vm.push(runtime.FnOf(fn))
	for _, a := range args {
		vm.push(a)
	}
	// Create a synthetic activation for the call.
	// We need an activation to execute the call instruction.
	// Let's just simulate the call directly using the VM's call method.
	if err := vm.call(runtime.FnOf(fn), args); err != nil {
		if as, ok := err.(*AsyncSuspend); ok {
			return runtime.Unit(), as
		}
		return runtime.Unit(), err
	}
	// Run the VM until the callee returns.
	result, err := vm.Run()
	return result, err
}

// Pump resumes all coroutines whose async effects have been resolved.
func Pump(st *runtime.State) error {
	tokens := st.TakeResolved()
	for _, tok := range tokens {
		p := st.PopPending(tok)
		if p == nil {
			continue
		}
		si, ok := p.Runner.(*SuspendInfo)
		if !ok {
			continue
		}
		vm := NewVM(st)
		vm.Resume(si)

		// Push the resolved result.
		if p.Err != nil {
			vm.push(runtime.TaggedOf("Error", []runtime.Value{runtime.Str(p.Err.Error())}))
		} else {
			vm.push(runtime.TaggedOf("Success", []runtime.Value{p.Result}))
		}

		result, err := vm.Run()
		if err != nil {
			if as, ok := err.(*AsyncSuspend); ok {
				// Re-suspension: park again.
				st.ParkAsync(as.Token, vm.Suspend())
				continue
			}
			st.LastError = err
			continue
		}
		_ = result
	}
	return nil
}

// InstallCallHook sets up the runtime's CallDomscript hook so event
// dispatch and api.Call work.
func InstallCallHook(st *runtime.State) {
	st.CallDomscript = func(fn *runtime.Fn, args []runtime.Value) (runtime.Value, error) {
		return CallFn(st, fn, args)
	}
}

// Ensure imports.
var _ = ast.Program{}
var _ = ir.Func{}
var _ = sema.Info{}
var _ = parser.Parse
var _ = lexer.Scan
var _ = diag.NewSet
