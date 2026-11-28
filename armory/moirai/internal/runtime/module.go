// Package runtime: module.go — the module registry.
//
// Domscript 1.0.0 has no module keyword (Section 17): namespacing is done
// with Tables of functions, and a host may evaluate any number of source
// chunks against one State, with later chunks seeing earlier bindings.
// The registry here serves the two places that still need a name keyed
// beyond the global environment:
//
//   - dsc / api.Load keep a per-state cache so evaluating the same module
//     source twice yields one namespace and one set of side effects;
//   - stdlib modules register their exported Table under their name, so
//     the prelude can bind `math`, `string`, ... without polluting the
//     global namespace with every native.
//
// A Module's exports are an ordinary Table value; nothing about it is
// privileged.
package runtime

// Module is one loaded namespace.
type Module struct {
	Name    string
	Exports Value // a :Table of bindings
}

// RegisterModule records a loaded module under its name. Registering the
// same name again replaces the previous entry, matching the "later chunk
// wins" chunk semantics.
func (st *State) RegisterModule(m *Module) {
	if st.Modules == nil {
		st.Modules = make(map[string]any)
	}
	st.Modules[m.Name] = m
}

// LookupModule fetches a loaded module by name.
func (st *State) LookupModule(name string) (*Module, bool) {
	m, ok := st.Modules[name]
	if !ok {
		return nil, false
	}
	mod, ok := m.(*Module)
	return mod, ok
}

// LoadedModules lists registered module names (diagnostics and dsc).
func (st *State) LoadedModules() []string {
	names := make([]string, 0, len(st.Modules))
	for n := range st.Modules {
		names = append(names, n)
	}
	return names
}
