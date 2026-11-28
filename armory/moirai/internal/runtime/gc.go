// Package runtime: gc.go documents the rooting and release discipline for
// a State (Section 18.1). Domscript values are reclaimed by the Go
// collector; the runtime's job is keeping every live Domscript value
// reachable from a Go root and never letting a host pointer masquerade as
// one.
//
// Root set. Everything a script can reach hangs off exactly these State
// fields, each of which is itself reachable from the *State the host holds:
//
//	Global        top-level bindings (let, fn, host-registered natives)
//	Builtins      the prelude environment
//	Nodes         the node-handle table (internal NodeData payloads;
//	              external entries hold uintptr only — see below)
//	subs          live `on` subscriptions (their Fn values)
//	pending       parked asynchronous coroutines (their continuation
//	              closures pin the whole activation: envs, stacks, code)
//	Modules       loaded module namespaces
//
// Anything not reachable from those is garbage by definition: temporaries
// on an interpreter stack, closed-over Envs with no surviving Fn, and
// node handles whose entries were dropped (below).
//
// Host pointers. External node entries store the host's pointer as a
// uintptr. A uintptr is invisible to the Go collector BY DESIGN: the
// runtime never dereferences it (all access goes through the Host
// interface), and the host owns the pointed-to node's lifetime entirely.
// The byHost map therefore neither keeps host memory alive nor keeps Go
// memory alive; a host that frees a node without notifying the state
// leaves a stale entry that only ever fails a Host call. Hosts that need
// reclamation call ReleaseNode (below), normally from their own
// node-removal path.
//
// Suspension. A parked coroutine's continuation is a Go closure chain
// held by pending[token].Runner; the collector keeps it (and its captured
// Envs) alive until the entry is dropped at resume or Close. No
// explicit pinning is needed and none exists.
//
// Release. Close() drops every root; after Close the State retains its
// shape but all tables are emptied, and any later use is a host bug the
// API layer rejects. ReleaseNode removes one handle-table entry (and its
// byHost mapping) when the host destroys that node; :Node values already
// in flight compare dead (display "<detached>") and their ops fail with
// a catchable error, which is the observable contract for "the host
// removed this node."
package runtime

import "fmt"

// ReleaseNode drops the handle-table entry for id, for hosts that destroy
// a node and want its identity reclaimed. Ids are never reused
// (NodeTable.next only increments), so a stale :Node value never aliases
// a new node.
func (st *State) ReleaseNode(id int64) error {
	e, ok := st.Nodes.table[id]
	if !ok {
		return fmt.Errorf("no node handle %d", id)
	}
	// Detach from an internal parent first so the parent's child list
	// never references a dead id.
	if !e.External && e.Data != nil && e.Data.Parent >= 0 {
		if p := st.entry(e.Data.Parent); p != nil {
			_ = st.detach(p, id)
		}
	}
	delete(st.Nodes.table, id)
	if e.External {
		delete(st.Nodes.byHost, e.Host)
	}
	return nil
}

// HandleCount reports the number of live node handles (diagnostics).
func (st *State) HandleCount() int { return len(st.Nodes.table) }
