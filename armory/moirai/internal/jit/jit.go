// Package jit: jit.go — the tracing JIT compiler entry point.
//
// The JIT operates in two phases:
//  1. Profile: the interpreter records hot loop headers and call sites.
//  2. Compile: when a threshold is reached, the JIT builds a trace, allocates
//     registers, and emits native code.
//
// Compiled traces live in a code cache. On a guard failure or side exit, the
// runtime trace tier falls back to the interpreter. Native code remains an
// artifact until an executable-code linker and runtime ABI are added.
package jit

import (
	"bytes"
	"embed"
	"fmt"
	"os"
	stdruntime "runtime"
	"sync"

	"domscript/internal/ir"
)

// hostISAs keeps runtime JIT setup independent of the process working
// directory and makes the compiler usable from installed binaries.
//
//go:embed x/*.isa
var hostISAs embed.FS

// Compiler is the top-level JIT compiler. Each VM has one Compiler.
type Compiler struct {
	mu sync.Mutex

	// ISA for the target architecture.
	ISA ISA

	// Code cache: map from function + entry IP to compiled trace.
	cache map[cacheKey]*CompiledTrace

	// Profile counters: map from function + IP to hit count.
	profile map[profileKey]int

	// Hot threshold: number of hits before compiling a trace.
	HotThreshold int

	// Config for trace building.
	TraceCfg TraceConfig

	// Total bytes of native code emitted.
	CodeSize int

	// Maximum code cache size in bytes.
	MaxCodeSize int

	// Statistics.
	Stats CompilerStats
}

// cacheKey uniquely identifies a compiled trace.
type cacheKey struct {
	Func    *ir.Func
	EntryIP int
}

// profileKey uniquely identifies a profiling point.
type profileKey struct {
	Func *ir.Func
	IP   int
}

// CompiledTrace is a native-code-compiled trace.
type CompiledTrace struct {
	Trace    Trace
	Code     []byte       // native machine code
	Relocs   []Relocation // unresolved relocations
	Deopt    *DeoptMetadata
	Entry    uintptr // entry point address (set after linking)
	exec     []byte
	CodeSize int
}

// CompilerStats tracks JIT compilation statistics.
type CompilerStats struct {
	TracesCompiled    int
	TracesInvalidated int
	BytesCompiled     int
	ProfileHits       int
	NativeAttempts    int
	NativeCompiled    int
	NativeRunAttempts int
	NativeOverflows   int
	NativeExecs       int
	Deopts            int
}

// NewHostCompiler creates a compiler using the ISA description for the
// current Go target. The runtime uses this to keep JIT setup independent of
// the source tree layout.
func NewHostCompiler() (*Compiler, error) {
	arch := stdruntime.GOARCH
	if arch == "riscv64" {
		arch = "rv64"
	}
	data, err := hostISAs.ReadFile("x/" + arch + ".isa")
	if err != nil {
		return nil, fmt.Errorf("jit: host ISA %s: %w", arch, err)
	}
	isa, err := ParseISA(string(data))
	if err != nil {
		return nil, fmt.Errorf("jit: host ISA %s: %w", arch, err)
	}
	return NewCompiler(isa), nil
}

// NewCompiler creates a new JIT compiler for the given ISA.
func NewCompiler(isa ISA) *Compiler {
	return &Compiler{
		ISA:          isa,
		cache:        make(map[cacheKey]*CompiledTrace),
		profile:      make(map[profileKey]int),
		HotThreshold: 50,      // compile after 50 hits
		MaxCodeSize:  1 << 20, // 1 MB
		TraceCfg:     DefaultTraceConfig(),
	}
}

// NewCompilerFromFile creates a compiler by parsing an ISA file.
func NewCompilerFromFile(path string) (*Compiler, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("jit: read ISA file %s: %w", path, err)
	}
	isa, err := ParseISA(string(data))
	if err != nil {
		return nil, fmt.Errorf("jit: parse ISA: %w", err)
	}
	return NewCompiler(isa), nil
}

// Notify records a profiling hit at the given function and IP.
// Returns true if the threshold was reached and compilation should occur.
func (c *Compiler) Notify(f *ir.Func, ip int) bool {
	c.mu.Lock()
	defer c.mu.Unlock()

	key := profileKey{Func: f, IP: ip}
	c.profile[key]++
	c.Stats.ProfileHits++

	return c.profile[key] == c.HotThreshold
}

// Compile builds and caches a compiled trace for a function at the given entry.
func (c *Compiler) Compile(f *ir.Func, entry int) (*CompiledTrace, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	// Check cache.
	ck := cacheKey{Func: f, EntryIP: entry}
	if ct, ok := c.cache[ck]; ok {
		return ct, nil
	}

	// Check code size budget.
	if c.CodeSize >= c.MaxCodeSize {
		return nil, fmt.Errorf("jit: code cache full (%d bytes)", c.CodeSize)
	}

	// Build trace.
	trace := BuildTraceWithConfig(f, entry, c.TraceCfg)
	if len(trace.Code) == 0 {
		return nil, fmt.Errorf("jit: empty trace for %s @%d", f.Name, entry)
	}

	// Emit native code.
	code, relocs, err := EmitNative(trace, c.ISA)
	if err != nil {
		return nil, fmt.Errorf("jit: emit %s @%d: %w", f.Name, entry, err)
	}

	// Build deopt metadata.
	deopt := BuildDeoptMetadata(trace, c.ISA)

	ct := &CompiledTrace{
		Trace:    trace,
		Code:     code,
		Relocs:   relocs,
		Deopt:    deopt,
		CodeSize: len(code),
	}

	if trace.CanNativeExecute() {
		c.Stats.NativeAttempts++
		if entryPtr, execBuf, ok, err := buildNativeTrace(trace, c.ISA); err != nil {
			return nil, fmt.Errorf("jit: native emit %s @%d: %w", f.Name, entry, err)
		} else if ok {
			ct.Entry = entryPtr
			ct.exec = execBuf
			c.Stats.NativeCompiled++
		}
	}

	// Cache.
	c.cache[ck] = ct
	c.CodeSize += len(code)
	c.Stats.TracesCompiled++
	c.Stats.BytesCompiled += len(code)

	return ct, nil
}

// CompileHeads compiles traces for all hot trace heads in a function.
func (c *Compiler) CompileHeads(f *ir.Func) ([]*CompiledTrace, error) {
	heads := TraceHeads(f)
	var compiled []*CompiledTrace
	for _, h := range heads {
		if c.Notify(f, h) {
			ct, err := c.Compile(f, h)
			if err != nil {
				continue // skip failed compilations
			}
			compiled = append(compiled, ct)
		}
	}
	return compiled, nil
}

// Lookup finds a cached compiled trace.
func (c *Compiler) Lookup(f *ir.Func, entry int) *CompiledTrace {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.cache[cacheKey{Func: f, EntryIP: entry}]
}

// Invalidate removes a compiled trace from the cache (e.g., after a guard failure).
func (c *Compiler) Invalidate(f *ir.Func, entry int) {
	c.mu.Lock()
	defer c.mu.Unlock()
	key := cacheKey{Func: f, EntryIP: entry}
	if ct, ok := c.cache[key]; ok {
		ct.release()
		c.CodeSize -= ct.CodeSize
		delete(c.cache, key)
		c.Stats.TracesInvalidated++
	}
}

// InvalidateAll clears all compiled traces.
func (c *Compiler) InvalidateAll() {
	c.mu.Lock()
	defer c.mu.Unlock()
	for _, ct := range c.cache {
		ct.release()
	}
	c.cache = make(map[cacheKey]*CompiledTrace)
	c.CodeSize = 0
}

// RecordDeopt records a deoptimization event.
func (c *Compiler) RecordDeopt(f *ir.Func, ip int, reason DeoptReason) {
	c.mu.Lock()
	c.Stats.Deopts++
	c.mu.Unlock()

	// Invalidate the trace that caused the deopt.
	c.Invalidate(f, ip)
}

// RecordNativeExec records one native trace execution.
func (c *Compiler) RecordNativeExec() {
	c.mu.Lock()
	c.Stats.NativeExecs++
	c.mu.Unlock()
}

// RecordNativeRunAttempt records one attempt to execute a native trace.
func (c *Compiler) RecordNativeRunAttempt() {
	c.mu.Lock()
	c.Stats.NativeRunAttempts++
	c.mu.Unlock()
}

// RecordNativeOverflow records a native trace stack-size mismatch.
func (c *Compiler) RecordNativeOverflow() {
	c.mu.Lock()
	c.Stats.NativeOverflows++
	c.mu.Unlock()
}

// Disassemble returns a human-readable disassembly of a compiled trace.
func (ct *CompiledTrace) Disassemble() string {
	var b bytes.Buffer

	fmt.Fprintf(&b, "; Compiled trace for %s @%d\n", ct.Trace.Func.Name, ct.Trace.Entry)
	fmt.Fprintf(&b, "; %d instructions, %d bytes of code\n", len(ct.Trace.Code), len(ct.Code))
	fmt.Fprintf(&b, "; %d guards, %d side exits, %d children\n",
		len(ct.Trace.Guards), len(ct.Trace.SideExits), len(ct.Trace.Children))

	// Hex dump of native code (first 256 bytes).
	limit := len(ct.Code)
	if limit > 256 {
		limit = 256
	}
	fmt.Fprintf(&b, "; Native code (%d bytes, showing first %d):\n", len(ct.Code), limit)
	for i := 0; i < limit; i += 16 {
		end := i + 16
		if end > limit {
			end = limit
		}
		fmt.Fprintf(&b, "; %04x: ", i)
		for j := i; j < end; j++ {
			fmt.Fprintf(&b, "%02x ", ct.Code[j])
		}
		fmt.Fprintf(&b, "\n")
	}

	// Relocations.
	if len(ct.Relocs) > 0 {
		fmt.Fprintf(&b, "; Relocations:\n")
		for _, r := range ct.Relocs {
			fmt.Fprintf(&b, ";   @%04x %s target=%d\n", r.Offset, r.Kind, r.Target)
		}
	}

	// Deopt metadata.
	if ct.Deopt != nil {
		b.WriteString(FormatDeopt(ct.Deopt))
	}

	return b.String()
}

// StatsString returns a human-readable summary of compiler statistics.
func (c *Compiler) StatsString() string {
	c.mu.Lock()
	defer c.mu.Unlock()
	return fmt.Sprintf(
		"JIT: %d traces (%d bytes), %d deopts, %d profile hits, cache %d/%d bytes",
		c.Stats.TracesCompiled, c.Stats.BytesCompiled,
		c.Stats.Deopts, c.Stats.ProfileHits,
		c.CodeSize, c.MaxCodeSize,
	)
}

// StatsSnapshot returns a race-free copy of compiler statistics.
func (c *Compiler) StatsSnapshot() CompilerStats {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.Stats
}

func (ct *CompiledTrace) release() {
	if len(ct.exec) == 0 {
		return
	}
	_ = releaseExecutable(ct.exec)
	ct.exec = nil
	ct.Entry = 0
}
