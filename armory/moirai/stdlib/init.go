// Package stdlib is the Domscript standard library. It provides a
// RegisterAll function that registers every stdlib module's native
// functions into a state, after which the corresponding .doml sources
// can be evaluated to create the module export tables.
package stdlib

import (
	stdenv "os"
	"path/filepath"
	goruntime "runtime"
	"strings"
	gosync "sync"

	domruntime "domscript/internal/runtime"
	"domscript/stdlib/array"
	"domscript/stdlib/asdl"
	"domscript/stdlib/async"
	"domscript/stdlib/collections"
	"domscript/stdlib/crypto"
	"domscript/stdlib/csv"
	"domscript/stdlib/debug"
	"domscript/stdlib/dns"
	"domscript/stdlib/encoding"
	"domscript/stdlib/ffi"
	"domscript/stdlib/fs"
	"domscript/stdlib/hash"
	"domscript/stdlib/heap"
	"domscript/stdlib/http"
	"domscript/stdlib/inspect"
	"domscript/stdlib/io"
	"domscript/stdlib/json"
	"domscript/stdlib/mapmod"
	"domscript/stdlib/math"
	"domscript/stdlib/net"
	stdos "domscript/stdlib/os"
	"domscript/stdlib/path"
	"domscript/stdlib/process"
	"domscript/stdlib/random"
	"domscript/stdlib/regex"
	"domscript/stdlib/set"
	"domscript/stdlib/sexpr"
	"domscript/stdlib/stringmod"
	"domscript/stdlib/sync"
	"domscript/stdlib/thread"
	"domscript/stdlib/time"
	"domscript/stdlib/traceback"
	"domscript/stdlib/unicode"
	"domscript/stdlib/url"
	"domscript/stdlib/xml"
	"domscript/stdlib/yaml"
)

// RegisterAll registers every stdlib module's native functions into st.
// Call this before evaluating any .doml module sources.
func RegisterAll(st *domruntime.State) {
	io.Register(st)
	fs.Register(st)
	stdos.Register(st)
	path.Register(st)
	process.Register(st)
	collections.Register(st)
	array.Register(st)
	asdl.Register(st)
	mapmod.Register(st)
	set.Register(st)
	sexpr.Register(st)
	heap.Register(st)
	stringmod.Register(st)
	regex.Register(st)
	unicode.Register(st)
	encoding.Register(st)
	math.Register(st)
	random.Register(st)
	time.Register(st)
	crypto.Register(st)
	hash.Register(st)
	json.Register(st)
	xml.Register(st)
	csv.Register(st)
	yaml.Register(st)
	http.Register(st)
	net.Register(st)
	url.Register(st)
	dns.Register(st)
	ffi.Register(st)
	thread.Register(st)
	async.Register(st)
	sync.Register(st)
	debug.Register(st)
	inspect.Register(st)
	traceback.Register(st)
}

// ModuleRoots returns directories that may contain stdlib module sources.
//
// The first entries are taken from DOMSCRIPT_STDLIB if set. It accepts
// multiple entries separated by the platform path list separator.
// Entries after that come from AddModuleRoot (embedded hosts such as
// DOMTeX registering their own library trees at runtime), and finally
// the directory holding the stdlib sources themselves.
func ModuleRoots() []string {
	roots := []string{}
	seen := map[string]struct{}{}

	add := func(raw string) {
		if root, ok := normalizeRoot(raw); ok {
			if _, dup := seen[root]; !dup {
				seen[root] = struct{}{}
				roots = append(roots, root)
			}
		}
	}

	if env := stdenv.Getenv("DOMSCRIPT_STDLIB"); env != "" {
		for _, p := range strings.Split(env, string(stdenv.PathListSeparator)) {
			add(p)
		}
	}

	extraRootsMu.Lock()
	for _, root := range extraRoots {
		add(root)
	}
	extraRootsMu.Unlock()

	if _, file, _, ok := goruntime.Caller(0); ok {
		add(filepath.Dir(file))
	}
	return roots
}

var (
	extraRootsMu gosync.Mutex
	extraRoots   []string
)

func normalizeRoot(raw string) (string, bool) {
	root := strings.TrimSpace(raw)
	if root == "" {
		return "", false
	}
	if abs, err := filepath.Abs(root); err == nil {
		root = abs
	}
	return filepath.Clean(root), true
}

// AddModuleRoot registers an additional module search root for the
// lifetime of the process. It exists for embedded hosts (DOMTeX's
// domtex.* libraries, for example): unlike DOMSCRIPT_STDLIB, which the
// runtime only observes from the startup environment, this takes effect
// immediately, including for already-open states.
func AddModuleRoot(path string) {
	root, ok := normalizeRoot(path)
	if !ok {
		return
	}
	extraRootsMu.Lock()
	defer extraRootsMu.Unlock()
	for _, known := range extraRoots {
		if known == root {
			return
		}
	}
	extraRoots = append(extraRoots, root)
}
