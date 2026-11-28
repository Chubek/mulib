// Package runtime: object.go implements the per-state node-handle table.
//
// Nodes are identity-based handles into a host-owned live tree, never
// structural values (Sections 4.3 and 6.3). A :Node value is an index into
// the state's table; each entry holds either an opaque host pointer
// (stored as an integer, never dereferenced by Go) or runtime-owned node
// data for the built-in stub document. Node equality is handle identity.
// All tree operations on external nodes go through the Host interface;
// the runtime never walks host memory itself.
package runtime

import (
	"fmt"
	"strings"
)

// NodeKind is the concrete DOM node kind (Section 6.3). Ordinals are ABI
// (the C DomscriptNodeKind enum order).
type NodeKind uint8

const (
	KindElement NodeKind = iota
	KindText
	KindComment
	KindDocument
	KindFragment
)

func (k NodeKind) TagName() string {
	switch k {
	case KindElement:
		return "Element"
	case KindText:
		return "Text"
	case KindComment:
		return "Comment"
	case KindDocument:
		return "Document"
	case KindFragment:
		return "Fragment"
	}
	return "?"
}

// Node is a :Node value: an index into the state's node table, with a
// back-pointer so display can find its entry without global state.
type Node struct {
	id  int64
	tbl *NodeTable
}

// NodeID returns the handle identity.
func (n *Node) NodeID() int64 { return n.id }

func (n *Node) display() string {
	if n.tbl == nil {
		return "<node>"
	}
	e, ok := n.tbl.table[n.id]
	if !ok {
		return "<detached>"
	}
	switch e.Kind {
	case KindElement:
		if e.Data != nil {
			if cls, ok := e.Data.Attrs.Get(Str("class")); ok && cls.kind == KindString && cls.s != "" {
				return "<" + e.Data.Tag + " class=" + fmt.Sprintf("%q", cls.s) + ">"
			}
			return "<" + e.Data.Tag + ">"
		}
		return "<element>"
	case KindText:
		if e.Data != nil {
			return fmt.Sprintf("<text %q>", clip(e.Data.Text, 32))
		}
		return "<text>"
	case KindComment:
		return "<comment>"
	case KindDocument:
		return "<document>"
	case KindFragment:
		return "<fragment>"
	}
	return "<node>"
}

func clip(s string, n int) string {
	if len(s) <= n {
		return s
	}
	return s[:n] + "…"
}

// NodeData is the runtime-owned tree payload for internal nodes.
type NodeData struct {
	Tag      string // element name (KindElement)
	Text     string // text/comment content
	Attrs    *Table // element attributes: name String -> String|Bool
	Children []int64
	Parent   int64 // -1 at a root
}

// NodeEntry is one handle-table entry. ID is the handle identity.
type NodeEntry struct {
	ID       int64
	External bool // true: Host pointer, ops routed to st.Host
	Host     uintptr
	Kind     NodeKind
	Data     *NodeData // internal nodes
}

// NodeTable manages handle identity and internal tree data.
type NodeTable struct {
	table  map[int64]*NodeEntry
	byHost map[uintptr]int64
	next   int64
}

func NewNodeTable() *NodeTable {
	return &NodeTable{table: make(map[int64]*NodeEntry), byHost: make(map[uintptr]int64)}
}

func (t *NodeTable) newEntry(e *NodeEntry) *NodeEntry {
	t.next++
	e.ID = t.next
	t.table[e.ID] = e
	return e
}

// NodeOf wraps a handle id as a :Node value.
func (t *NodeTable) NodeOf(id int64) Value {
	return Value{kind: KindNode, ref: &Node{id: id, tbl: t}}
}

// Entry fetches an entry by handle id.
func (t *NodeTable) Entry(id int64) (*NodeEntry, bool) {
	e, ok := t.table[id]
	return e, ok
}

// Host is the set of tree operations the runtime performs on external
// (host-owned) nodes. Internal nodes never call these.
type Host interface {
	CreateExternal(kind NodeKind, tag, text string) uintptr
	AppendExternal(parent, child uintptr) error
	RemoveExternal(parent, child uintptr) error
	ReplaceExternal(parent, oldN, newN uintptr) error
	ChildrenExternal(n uintptr) []uintptr
	ParentExternal(n uintptr) (uintptr, bool)
	AttrGetExternal(n uintptr, name string) (Value, bool)
	AttrSetExternal(n uintptr, name string, v Value)
	AttrRemoveExternal(n uintptr, name string)
	TagNameExternal(n uintptr) (string, bool)
	TextExternal(n uintptr) (string, bool)
	KindExternal(n uintptr) NodeKind
}

// ------------------------------------------------- state node operations

func (st *State) CreateElement(tag string) Value {
	e := st.Nodes.newEntry(&NodeEntry{Kind: KindElement,
		Data: &NodeData{Tag: tag, Attrs: NewTable(), Parent: -1}})
	return st.Nodes.NodeOf(e.ID)
}

func (st *State) CreateText(text string) Value {
	e := st.Nodes.newEntry(&NodeEntry{Kind: KindText,
		Data: &NodeData{Text: text, Parent: -1}})
	return st.Nodes.NodeOf(e.ID)
}

func (st *State) CreateComment(text string) Value {
	e := st.Nodes.newEntry(&NodeEntry{Kind: KindComment,
		Data: &NodeData{Text: text, Parent: -1}})
	return st.Nodes.NodeOf(e.ID)
}

func (st *State) CreateFragment() Value {
	e := st.Nodes.newEntry(&NodeEntry{Kind: KindFragment,
		Data: &NodeData{Parent: -1}})
	return st.Nodes.NodeOf(e.ID)
}

func (st *State) CreateDocument() Value {
	e := st.Nodes.newEntry(&NodeEntry{Kind: KindDocument,
		Data: &NodeData{Parent: -1}})
	return st.Nodes.NodeOf(e.ID)
}

// WrapExternal registers a host node pointer as a handle.
func (st *State) WrapExternal(p uintptr, kind NodeKind) Value {
	if id, ok := st.Nodes.byHost[p]; ok {
		return st.Nodes.NodeOf(id)
	}
	e := st.Nodes.newEntry(&NodeEntry{External: true, Host: p, Kind: kind})
	st.Nodes.byHost[p] = e.ID
	return st.Nodes.NodeOf(e.ID)
}

// extID maps a host pointer to a (possibly fresh) handle id.
func (st *State) extID(p uintptr) int64 {
	if id, ok := st.Nodes.byHost[p]; ok {
		return id
	}
	k := KindElement
	if st.Host != nil {
		k = st.Host.KindExternal(p)
	}
	e := st.Nodes.newEntry(&NodeEntry{External: true, Host: p, Kind: k})
	st.Nodes.byHost[p] = e.ID
	return e.ID
}

func (st *State) nodeEntry(v Value) (*NodeEntry, error) {
	if v.kind != KindNode {
		return nil, fmt.Errorf("expected a :Node, got %s", TypeName(v))
	}
	id := v.ref.(*Node).id
	e, ok := st.Nodes.table[id]
	if !ok {
		return nil, fmt.Errorf("node handle is no longer live")
	}
	return e, nil
}

func (st *State) NodeKindOf(v Value) (NodeKind, error) {
	e, err := st.nodeEntry(v)
	if err != nil {
		return 0, err
	}
	if e.External && st.Host != nil {
		return st.Host.KindExternal(e.Host), nil
	}
	return e.Kind, nil
}

func (st *State) TagNameOf(v Value) (Value, error) {
	e, err := st.nodeEntry(v)
	if err != nil {
		return Unit(), err
	}
	if e.External {
		if st.Host == nil || !e.External {
			return TaggedOf("None", nil), nil
		}
		name, ok := st.Host.TagNameExternal(e.Host)
		if !ok {
			return TaggedOf("None", nil), nil
		}
		return TaggedOf("Some", []Value{Str(name)}), nil
	}
	if e.Kind != KindElement {
		return TaggedOf("None", nil), nil
	}
	return TaggedOf("Some", []Value{Str(e.Data.Tag)}), nil
}

func (st *State) TextOfNode(v Value) (Value, error) {
	e, err := st.nodeEntry(v)
	if err != nil {
		return Unit(), err
	}
	if e.External && st.Host != nil {
		text, ok := st.Host.TextExternal(e.Host)
		if !ok {
			return TaggedOf("None", nil), nil
		}
		return TaggedOf("Some", []Value{Str(text)}), nil
	}
	if e.Kind != KindText {
		return TaggedOf("None", nil), nil
	}
	return TaggedOf("Some", []Value{Str(e.Data.Text)}), nil
}

func someTable(t *Table) Value {
	return TaggedOf("Some", []Value{Value{kind: KindTable, ref: t}})
}

func (st *State) AttrsOf(v Value) (Value, error) {
	e, err := st.nodeEntry(v)
	if err != nil {
		return Unit(), err
	}
	if e.External {
		// The Host interface reads attributes one by one and offers no
		// enumeration; Attrs on an external node yields the empty Table
		// (documented in NOTES.md).
		return someTable(NewTable()), nil
	}
	if e.Kind != KindElement {
		return TaggedOf("None", nil), nil
	}
	return someTable(e.Data.Attrs), nil
}

func (st *State) GetAttr(v Value, name string) (Value, error) {
	e, err := st.nodeEntry(v)
	if err != nil {
		return Unit(), err
	}
	if e.External && st.Host != nil {
		if val, ok := st.Host.AttrGetExternal(e.Host, name); ok {
			return TaggedOf("Some", []Value{val}), nil
		}
		return TaggedOf("None", nil), nil
	}
	if e.Kind != KindElement {
		return TaggedOf("None", nil), nil
	}
	if val, ok := e.Data.Attrs.Get(Str(name)); ok {
		return TaggedOf("Some", []Value{val}), nil
	}
	return TaggedOf("None", nil), nil
}

func (st *State) SetAttr(v Value, name string, val Value) error {
	e, err := st.nodeEntry(v)
	if err != nil {
		return err
	}
	if e.External {
		if st.Host != nil {
			st.Host.AttrSetExternal(e.Host, name, val)
		}
		return nil
	}
	if e.Kind != KindElement {
		return fmt.Errorf("SetAttr on a non-element node")
	}
	return e.Data.Attrs.Set(Str(name), val)
}

func (st *State) RemoveAttr(v Value, name string) error {
	e, err := st.nodeEntry(v)
	if err != nil {
		return err
	}
	if e.External {
		if st.Host != nil {
			st.Host.AttrRemoveExternal(e.Host, name)
		}
		return nil
	}
	if e.Kind != KindElement {
		return fmt.Errorf("RemoveAttr on a non-element node")
	}
	e.Data.Attrs.Delete(Str(name))
	return nil
}

func (st *State) ChildrenOf(v Value) (Value, error) {
	e, err := st.nodeEntry(v)
	if err != nil {
		return Unit(), err
	}
	var ids []int64
	if e.External {
		if st.Host == nil {
			return EmptyList(), nil
		}
		for _, h := range st.Host.ChildrenExternal(e.Host) {
			ids = append(ids, st.extID(h))
		}
	} else {
		ids = append(ids, e.Data.Children...)
	}
	elems := make([]Value, len(ids))
	for i, id := range ids {
		elems[i] = st.Nodes.NodeOf(id)
	}
	return ListOf(elems), nil
}

func (st *State) ParentOf(v Value) (Value, error) {
	e, err := st.nodeEntry(v)
	if err != nil {
		return Unit(), err
	}
	if e.External {
		if st.Host == nil {
			return TaggedOf("None", nil), nil
		}
		p, ok := st.Host.ParentExternal(e.Host)
		if !ok || p == 0 {
			return TaggedOf("None", nil), nil
		}
		return TaggedOf("Some", []Value{st.Nodes.NodeOf(st.extID(p))}), nil
	}
	if e.Data.Parent < 0 {
		return TaggedOf("None", nil), nil
	}
	return TaggedOf("Some", []Value{st.Nodes.NodeOf(e.Data.Parent)}), nil
}

func nodeID(v Value) int64 { return v.ref.(*Node).id }

func (st *State) AppendChild(parent, child Value) error {
	pe, err := st.nodeEntry(parent)
	if err != nil {
		return err
	}
	ce, err := st.nodeEntry(child)
	if err != nil {
		return err
	}
	if pe.External || ce.External {
		if st.Host == nil {
			return fmt.Errorf("cannot mutate an external node without a host")
		}
		if pe.External && ce.External {
			return st.Host.AppendExternal(pe.Host, ce.Host)
		}
		if pe.External {
			// Internal child adopted by an external parent.
			h := st.Host.CreateExternal(ce.Kind, ce.Data.Tag, ce.Data.Text)
			ce.External = true
			ce.Host = h
			delete(st.Nodes.table, ce.ID)
			st.Nodes.byHost[h] = ce.ID
			st.Nodes.table[ce.ID] = ce
			return st.Host.AppendExternal(pe.Host, h)
		}
		return fmt.Errorf("cannot append an external node to an internal parent")
	}
	// Appending a node to one of its own descendants is a tree error
	// (Section 6.3).
	if st.subtreeContains(ce.ID, pe.ID) {
		return fmt.Errorf("cannot append a node to one of its own descendants")
	}
	if ce.Data.Parent >= 0 {
		if perr := st.detach(st.Nodes.table[ce.Data.Parent], ce.ID); perr != nil {
			return perr
		}
	}
	pe.Data.Children = append(pe.Data.Children, ce.ID)
	ce.Data.Parent = pe.ID
	return nil
}

// subtreeContains reports whether root's subtree contains target.
func (st *State) subtreeContains(root, target int64) bool {
	if root == target {
		return true
	}
	e, ok := st.Nodes.table[root]
	if !ok || e.External || e.Data == nil {
		return false
	}
	for _, c := range e.Data.Children {
		if c == target || st.subtreeContains(c, target) {
			return true
		}
	}
	return false
}

func (st *State) detach(parent *NodeEntry, childID int64) error {
	if parent == nil || parent.External || parent.Data == nil {
		return fmt.Errorf("cannot detach from an external parent")
	}
	for i, c := range parent.Data.Children {
		if c == childID {
			parent.Data.Children = append(parent.Data.Children[:i], parent.Data.Children[i+1:]...)
			if e, ok := st.Nodes.table[childID]; ok && e.Data != nil {
				e.Data.Parent = -1
			}
			return nil
		}
	}
	return fmt.Errorf("node is not a child of the given parent")
}

func (st *State) RemoveChild(parent, child Value) error {
	pe, err := st.nodeEntry(parent)
	if err != nil {
		return err
	}
	ce, err := st.nodeEntry(child)
	if err != nil {
		return err
	}
	if pe.External || ce.External {
		if st.Host == nil {
			return fmt.Errorf("cannot mutate an external node without a host")
		}
		if pe.External && ce.External {
			return st.Host.RemoveExternal(pe.Host, ce.Host)
		}
		return fmt.Errorf("mixed internal/external RemoveChild is not supported")
	}
	if !containsChild(pe.Data.Children, ce.ID) {
		return fmt.Errorf("node is not a child of the given parent")
	}
	return st.detach(pe, ce.ID)
}

func containsChild(children []int64, id int64) bool {
	for _, c := range children {
		if c == id {
			return true
		}
	}
	return false
}

func (st *State) ReplaceChild(parent, oldChild, newChild Value) error {
	pe, err := st.nodeEntry(parent)
	if err != nil {
		return err
	}
	oe, err := st.nodeEntry(oldChild)
	if err != nil {
		return err
	}
	ne, err := st.nodeEntry(newChild)
	if err != nil {
		return err
	}
	if pe.External || oe.External || ne.External {
		if st.Host == nil {
			return fmt.Errorf("cannot mutate an external node without a host")
		}
		if pe.External && oe.External && ne.External {
			return st.Host.ReplaceExternal(pe.Host, oe.Host, ne.Host)
		}
		return fmt.Errorf("mixed internal/external ReplaceChild is not supported")
	}
	for i, c := range pe.Data.Children {
		if c == oe.ID {
			pe.Data.Children[i] = ne.ID
			ne.Data.Parent = pe.ID
			oe.Data.Parent = -1
			return nil
		}
	}
	return fmt.Errorf("node is not a child of the given parent")
}

// ------------------------------------------------------------- selectors
//
// Selector syntax is implementation-defined (Section 8); the subset here
// is a strict subset of standard CSS and is documented in NOTES.md:
//
//	groups:      selector [ "," selector ]*
//	compound:    [ tag | "*" ] { "#"id | "."class | "[" attr test val "]" | ":" pseudo }
//	attr test:   = | ~= | ^= | $= | *= | (presence)
//	pseudo:      first-child | last-child | only-child | empty |
//	             nth-child(an+b | odd | even) | not(compound)
//	combinator:  descendant (whitespace) | ">" | "+" | "~"
//
// Only elements match; Text, Comment, Document, and Fragment nodes never
// do. Matching walks the per-state handle table for internal nodes and the
// Host interface for external ones, so the same code serves both trees.

// AttrTest selects how an attribute condition compares.
type AttrTest uint8

const (
	AttrPresent AttrTest = iota
	AttrEquals
	AttrIncludes // [a~=v]: whitespace-separated word equal to v
	AttrPrefix   // [a^=v]
	AttrSuffix   // [a$=v]
	AttrContains // [a*=v]
)

// SelCond is one attribute condition.
type SelCond struct {
	Attr string
	Test AttrTest
	Val  string
}

// PseudoKind enumerates the supported pseudo-classes.
type PseudoKind uint8

const (
	PseudoFirstChild PseudoKind = iota
	PseudoLastChild
	PseudoOnlyChild
	PseudoEmpty
	PseudoNthChild
	PseudoNot
)

// SelPseudo is one pseudo-class condition. A/B hold the nth-child
// coefficients (index == A*n+B for some integer n >= 0).
type SelPseudo struct {
	Kind PseudoKind
	A, B int
	Sub  *SelCompound // :not(...) argument
}

// SelCompound is one compound selector: tag/id/class/attr/pseudo
// conditions that must all hold.
type SelCompound struct {
	Tag     string // "" = any
	ID      string
	Classes []string
	Attrs   []SelCond
	Pseudos []SelPseudo
}

// SelStep is one compound plus the combinator relating it to the compound
// on its left; Comb is unused for the first step.
type SelStep struct {
	Comb byte // ' ' descendant, '>', '+', '~'
	C    SelCompound
}

// SelGroup is one comma-separated selector.
type SelGroup struct {
	Steps []SelStep
}

// SelProg is the compiled form of a selector source string.
type SelProg struct {
	Groups []SelGroup
}

// CompileSelector parses src (Section 8). An invalid selector is an error,
// which the VM surfaces as a raise catchable by `~`.
func CompileSelector(src string) (*Selector, error) {
	p := &selParser{src: src}
	groups, err := p.parse()
	if err != nil {
		return nil, err
	}
	return &Selector{Src: src, Prog: &SelProg{Groups: groups}}, nil
}

// selParser is a byte cursor over a selector source string.
type selParser struct {
	src string
	pos int
}

func (p *selParser) errf(format string, args ...any) error {
	return fmt.Errorf("selector %q at offset %d: %s", p.src, p.pos, fmt.Sprintf(format, args...))
}

func (p *selParser) ws() bool {
	start := p.pos
	for p.pos < len(p.src) && (p.src[p.pos] == ' ' || p.src[p.pos] == '\t') {
		p.pos++
	}
	return p.pos > start
}

func (p *selParser) ident() string {
	start := p.pos
	if p.pos >= len(p.src) {
		return ""
	}
	ch := p.src[p.pos]
	if !(ch == '_' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch >= 0x80 ||
		ch == '-' && p.pos+1 < len(p.src) && isSelIdentCont(p.src[p.pos+1])) {
		return ""
	}
	p.pos++
	for p.pos < len(p.src) && isSelIdentCont(p.src[p.pos]) {
		p.pos++
	}
	return p.src[start:p.pos]
}

func isSelIdentCont(ch byte) bool {
	return ch == '_' || ch == '-' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch >= '0' && ch <= '9' || ch >= 0x80
}

func isSelDigit(ch byte) bool { return ch >= '0' && ch <= '9' }

func (p *selParser) parse() ([]SelGroup, error) {
	var groups []SelGroup
	for {
		p.ws()
		g, err := p.group()
		if err != nil {
			return nil, err
		}
		groups = append(groups, g)
		p.ws()
		if p.pos >= len(p.src) {
			return groups, nil
		}
		if p.src[p.pos] == ',' {
			p.pos++
			continue
		}
		return nil, p.errf("unexpected %q", string(p.src[p.pos]))
	}
}

func (p *selParser) group() (SelGroup, error) {
	var g SelGroup
	comb := byte(' ')
	for {
		step := SelStep{Comb: comb}
		c, err := p.compound()
		if err != nil {
			return g, err
		}
		step.C = c
		g.Steps = append(g.Steps, step)
		sawWS := p.ws()
		if p.pos >= len(p.src) || p.src[p.pos] == ',' {
			return g, nil
		}
		ch := p.src[p.pos]
		if ch == '>' || ch == '+' || ch == '~' {
			comb = ch
			p.pos++
			p.ws()
			continue
		}
		if !sawWS {
			return g, p.errf("unexpected %q", string(ch))
		}
		comb = ' '
	}
}

func (p *selParser) compound() (SelCompound, error) {
	var c SelCompound
	if p.pos >= len(p.src) {
		return c, p.errf("expected a selector")
	}
	if ch := p.src[p.pos]; ch == '*' {
		p.pos++
	} else if n := p.ident(); n != "" {
		c.Tag = n
	}
	for p.pos < len(p.src) {
		switch p.src[p.pos] {
		case '#':
			p.pos++
			id := p.ident()
			if id == "" {
				return c, p.errf("expected an id after '#'")
			}
			c.ID = id
		case '.':
			p.pos++
			cls := p.ident()
			if cls == "" {
				return c, p.errf("expected a class after '.'")
			}
			c.Classes = append(c.Classes, cls)
		case '[':
			cond, err := p.attrCond()
			if err != nil {
				return c, err
			}
			c.Attrs = append(c.Attrs, cond)
		case ':':
			ps, err := p.pseudo()
			if err != nil {
				return c, err
			}
			c.Pseudos = append(c.Pseudos, ps)
		default:
			return c, nil
		}
	}
	return c, nil
}

func (p *selParser) attrCond() (SelCond, error) {
	p.pos++ // '['
	p.ws()
	name := p.ident()
	if name == "" {
		return SelCond{}, p.errf("expected an attribute name")
	}
	p.ws()
	if p.pos < len(p.src) && p.src[p.pos] == ']' {
		p.pos++
		return SelCond{Attr: name, Test: AttrPresent}, nil
	}
	if p.pos >= len(p.src) {
		return SelCond{}, p.errf("unterminated attribute selector")
	}
	var test AttrTest
	switch p.src[p.pos] {
	case '=':
		test = AttrEquals
		p.pos++
	default:
		if p.pos+1 < len(p.src) && p.src[p.pos+1] == '=' &&
			(p.src[p.pos] == '~' || p.src[p.pos] == '^' || p.src[p.pos] == '$' || p.src[p.pos] == '*') {
			switch p.src[p.pos] {
			case '~':
				test = AttrIncludes
			case '^':
				test = AttrPrefix
			case '$':
				test = AttrSuffix
			case '*':
				test = AttrContains
			}
			p.pos += 2
		} else {
			return SelCond{}, p.errf("expected an attribute operator")
		}
	}
	p.ws()
	val, err := p.attrValue()
	if err != nil {
		return SelCond{}, err
	}
	p.ws()
	if p.pos >= len(p.src) || p.src[p.pos] != ']' {
		return SelCond{}, p.errf("unterminated attribute selector")
	}
	p.pos++
	return SelCond{Attr: name, Test: test, Val: val}, nil
}

func (p *selParser) attrValue() (string, error) {
	if p.pos >= len(p.src) {
		return "", p.errf("expected an attribute value")
	}
	if q := p.src[p.pos]; q == '"' || q == '\'' {
		p.pos++
		start := p.pos
		for p.pos < len(p.src) && p.src[p.pos] != q {
			p.pos++
		}
		if p.pos >= len(p.src) {
			return "", p.errf("unterminated string in attribute selector")
		}
		v := p.src[start:p.pos]
		p.pos++
		return v, nil
	}
	return p.ident(), nil
}

func (p *selParser) pseudo() (SelPseudo, error) {
	p.pos++ // ':'
	if p.pos < len(p.src) && p.src[p.pos] == ':' {
		return SelPseudo{}, p.errf("pseudo-elements are not supported")
	}
	name := p.ident()
	if name == "" {
		return SelPseudo{}, p.errf("expected a pseudo-class")
	}
	switch name {
	case "first-child":
		return SelPseudo{Kind: PseudoFirstChild}, nil
	case "last-child":
		return SelPseudo{Kind: PseudoLastChild}, nil
	case "only-child":
		return SelPseudo{Kind: PseudoOnlyChild}, nil
	case "empty":
		return SelPseudo{Kind: PseudoEmpty}, nil
	case "nth-child":
		if p.pos >= len(p.src) || p.src[p.pos] != '(' {
			return SelPseudo{}, p.errf("expected '(' after nth-child")
		}
		p.pos++
		p.ws()
		a, b, err := p.anb()
		if err != nil {
			return SelPseudo{}, err
		}
		p.ws()
		if p.pos >= len(p.src) || p.src[p.pos] != ')' {
			return SelPseudo{}, p.errf("expected ')' after nth-child")
		}
		p.pos++
		return SelPseudo{Kind: PseudoNthChild, A: a, B: b}, nil
	case "not":
		if p.pos >= len(p.src) || p.src[p.pos] != '(' {
			return SelPseudo{}, p.errf("expected '(' after not")
		}
		p.pos++
		p.ws()
		c, err := p.compound()
		if err != nil {
			return SelPseudo{}, err
		}
		p.ws()
		if p.pos >= len(p.src) || p.src[p.pos] != ')' {
			return SelPseudo{}, p.errf("expected ')' after not")
		}
		p.pos++
		return SelPseudo{Kind: PseudoNot, Sub: &c}, nil
	}
	return SelPseudo{}, p.errf("unsupported pseudo-class %q", name)
}

// anb parses an an+b expression (also plain integers, odd, even).
func (p *selParser) anb() (int, int, error) {
	lower := strings.ToLower(p.src[p.pos:])
	if strings.HasPrefix(lower, "odd") {
		p.pos += 3
		return 2, 1, nil
	}
	if strings.HasPrefix(lower, "even") {
		p.pos += 4
		return 2, 0, nil
	}
	sign := 1
	if p.pos < len(p.src) && (p.src[p.pos] == '-' || p.src[p.pos] == '+') {
		if p.src[p.pos] == '-' {
			sign = -1
		}
		p.pos++
	}
	num, hasNum := 0, false
	for p.pos < len(p.src) && isSelDigit(p.src[p.pos]) {
		num = num*10 + int(p.src[p.pos]-'0')
		p.pos++
		hasNum = true
	}
	if p.pos < len(p.src) && (p.src[p.pos] == 'n' || p.src[p.pos] == 'N') {
		p.pos++
		a := 1
		if hasNum {
			a = sign * num
		} else if sign < 0 {
			a = -1
		}
		b := 0
		if p.pos < len(p.src) && (p.src[p.pos] == '+' || p.src[p.pos] == '-') {
			bsign := 1
			if p.src[p.pos] == '-' {
				bsign = -1
			}
			p.pos++
			for p.pos < len(p.src) && isSelDigit(p.src[p.pos]) {
				b = b*10 + int(p.src[p.pos]-'0')
				p.pos++
			}
			b *= bsign
		}
		return a, b, nil
	}
	if !hasNum {
		return 0, 0, p.errf("expected an an+b index")
	}
	return 0, sign * num, nil
}

// ------------------------------------------------------ selector matching

// selProg compiles sel on demand (selectors constructed by hosts may not
// have gone through CompileSelector).
func (st *State) selProg(sel *Selector) (*SelProg, error) {
	if sel.Prog != nil {
		return sel.Prog, nil
	}
	compiled, err := CompileSelector(sel.Src)
	if err != nil {
		return nil, err
	}
	sel.Prog = compiled.Prog
	return sel.Prog, nil
}

// entry fetches a table entry, nil when absent.
func (st *State) entry(id int64) *NodeEntry {
	e, _ := st.Nodes.table[id]
	return e
}

// childIDs lists the child handle ids of e (external nodes via the Host).
func (st *State) childIDs(e *NodeEntry) []int64 {
	if e.External {
		if st.Host == nil {
			return nil
		}
		var ids []int64
		for _, h := range st.Host.ChildrenExternal(e.Host) {
			ids = append(ids, st.extID(h))
		}
		return ids
	}
	return e.Data.Children
}

// parentEntry returns e's parent, nil at a root.
func (st *State) parentEntry(e *NodeEntry) *NodeEntry {
	if e.External {
		if st.Host == nil {
			return nil
		}
		p, ok := st.Host.ParentExternal(e.Host)
		if !ok || p == 0 {
			return nil
		}
		return st.entry(st.extID(p))
	}
	if e.Data == nil || e.Data.Parent < 0 {
		return nil
	}
	return st.entry(e.Data.Parent)
}

func (st *State) isElement(e *NodeEntry) bool {
	if e.External {
		return st.Host != nil && st.Host.KindExternal(e.Host) == KindElement
	}
	return e.Kind == KindElement
}

func (st *State) elemTagName(e *NodeEntry) string {
	if e.External {
		if st.Host == nil {
			return ""
		}
		if n, ok := st.Host.TagNameExternal(e.Host); ok {
			return n
		}
		return ""
	}
	if e.Data != nil {
		return e.Data.Tag
	}
	return ""
}

// attrValue reads one attribute as a Value, reporting presence.
func (st *State) attrValue(e *NodeEntry, name string) (Value, bool) {
	if e.External {
		if st.Host == nil {
			return Unit(), false
		}
		return st.Host.AttrGetExternal(e.Host, name)
	}
	if e.Data == nil || e.Data.Attrs == nil {
		return Unit(), false
	}
	return e.Data.Attrs.Get(Str(name))
}

// elemChildren lists the element children of e in order.
func (st *State) elemChildren(e *NodeEntry) []*NodeEntry {
	var out []*NodeEntry
	for _, id := range st.childIDs(e) {
		if c := st.entry(id); c != nil && st.isElement(c) {
			out = append(out, c)
		}
	}
	return out
}

// matchCompound tests the tag/id/class/attr/pseudo conditions on e.
func (st *State) matchCompound(e *NodeEntry, c *SelCompound) bool {
	if !st.isElement(e) {
		return false
	}
	if c.Tag != "" && st.elemTagName(e) != c.Tag {
		return false
	}
	if c.ID != "" {
		v, ok := st.attrValue(e, "id")
		if !ok || Display(v) != c.ID {
			return false
		}
	}
	for _, cls := range c.Classes {
		v, ok := st.attrValue(e, "class")
		if !ok || !hasWord(Display(v), cls) {
			return false
		}
	}
	for _, cond := range c.Attrs {
		if !st.matchAttr(e, cond) {
			return false
		}
	}
	for i := range c.Pseudos {
		if !st.matchPseudo(e, &c.Pseudos[i]) {
			return false
		}
	}
	return true
}

// hasWord reports whether word occurs in the whitespace-separated list s.
func hasWord(s, word string) bool {
	for _, f := range strings.Fields(s) {
		if f == word {
			return true
		}
	}
	return false
}

func (st *State) matchAttr(e *NodeEntry, cond SelCond) bool {
	v, ok := st.attrValue(e, cond.Attr)
	if !ok {
		return false
	}
	val := Display(v)
	switch cond.Test {
	case AttrPresent:
		return true
	case AttrEquals:
		return val == cond.Val
	case AttrIncludes:
		return hasWord(val, cond.Val)
	case AttrPrefix:
		return strings.HasPrefix(val, cond.Val)
	case AttrSuffix:
		return strings.HasSuffix(val, cond.Val)
	case AttrContains:
		return strings.Contains(val, cond.Val)
	}
	return false
}

func (st *State) matchPseudo(e *NodeEntry, ps *SelPseudo) bool {
	switch ps.Kind {
	case PseudoFirstChild:
		idx, n := st.elemIndex(e)
		if n == 0 {
			return true // root matches, as in CSS
		}
		return idx == 0
	case PseudoLastChild:
		idx, n := st.elemIndex(e)
		if n == 0 {
			return true
		}
		return idx == n-1
	case PseudoOnlyChild:
		_, n := st.elemIndex(e)
		return n <= 1
	case PseudoEmpty:
		for _, id := range st.childIDs(e) {
			c := st.entry(id)
			if c == nil {
				continue
			}
			if st.isElement(c) {
				return false
			}
			if st.nodeText(c) != "" {
				return false
			}
		}
		return true
	case PseudoNthChild:
		idx, n := st.elemIndex(e)
		if n == 0 {
			idx, n = 0, 1 // root behaves as index 1
		}
		i := idx + 1
		if ps.A == 0 {
			return i == ps.B
		}
		d := i - ps.B
		return d%ps.A == 0 && d/ps.A >= 0
	case PseudoNot:
		return ps.Sub == nil || !st.matchCompound(e, ps.Sub)
	}
	return false
}

// nodeText returns the text content of a Text node ("" otherwise).
func (st *State) nodeText(e *NodeEntry) string {
	if e.External {
		if st.Host == nil {
			return ""
		}
		if t, ok := st.Host.TextExternal(e.Host); ok {
			return t
		}
		return ""
	}
	if e.Data != nil && e.Kind == KindText {
		return e.Data.Text
	}
	return ""
}

// elemIndex returns e's 0-based index among its parent's element children
// and that count; n == 0 when e has no parent.
func (st *State) elemIndex(e *NodeEntry) (int, int) {
	p := st.parentEntry(e)
	if p == nil {
		return 0, 0
	}
	sibs := st.elemChildren(p)
	for i, s := range sibs {
		if s.ID == e.ID {
			return i, len(sibs)
		}
	}
	return 0, len(sibs)
}

// prevElemSiblings lists e's preceding element siblings, nearest first.
func (st *State) prevElemSiblings(e *NodeEntry) []*NodeEntry {
	p := st.parentEntry(e)
	if p == nil {
		return nil
	}
	var out []*NodeEntry
	for _, s := range st.elemChildren(p) {
		if s.ID == e.ID {
			return out
		}
		out = append([]*NodeEntry{s}, out...)
	}
	return out
}

// matchGroup tests e against one full complex selector, right to left.
func (st *State) matchGroup(e *NodeEntry, g *SelGroup) bool {
	return st.matchStep(e, g, len(g.Steps)-1)
}

func (st *State) matchStep(e *NodeEntry, g *SelGroup, i int) bool {
	if !st.matchCompound(e, &g.Steps[i].C) {
		return false
	}
	if i == 0 {
		return true
	}
	switch g.Steps[i].Comb {
	case '>':
		p := st.parentEntry(e)
		return p != nil && st.matchStep(p, g, i-1)
	case '+':
		sibs := st.prevElemSiblings(e)
		return len(sibs) > 0 && st.matchStep(sibs[0], g, i-1)
	case '~':
		for _, s := range st.prevElemSiblings(e) {
			if st.matchStep(s, g, i-1) {
				return true
			}
		}
		return false
	default: // descendant
		p := st.parentEntry(e)
		for p != nil {
			if st.matchStep(p, g, i-1) {
				return true
			}
			p = st.parentEntry(p)
		}
		return false
	}
}

// walkDescendants visits e's subtree in document order (excluding e).
func (st *State) walkDescendants(e *NodeEntry, f func(*NodeEntry)) {
	for _, id := range st.childIDs(e) {
		c := st.entry(id)
		if c == nil {
			continue
		}
		f(c)
		st.walkDescendants(c, f)
	}
}

// QueryAll returns every descendant of root (document order) matching
// sel; never an error for a non-matching selector ([] instead, Section 8).
func (st *State) QueryAll(root Value, sel *Selector) (Value, error) {
	re, err := st.nodeEntry(root)
	if err != nil {
		return Unit(), err
	}
	prog, err := st.selProg(sel)
	if err != nil {
		return Unit(), err
	}
	var out []Value
	st.walkDescendants(re, func(e *NodeEntry) {
		for i := range prog.Groups {
			if st.matchGroup(e, &prog.Groups[i]) {
				out = append(out, st.Nodes.NodeOf(e.ID))
				return
			}
		}
	})
	if out == nil {
		out = []Value{}
	}
	return ListOf(out), nil
}

// QueryFirst returns :Some[first matching descendant] or :None.
func (st *State) QueryFirst(root Value, sel *Selector) (Value, error) {
	re, err := st.nodeEntry(root)
	if err != nil {
		return Unit(), err
	}
	prog, err := st.selProg(sel)
	if err != nil {
		return Unit(), err
	}
	var found *NodeEntry
	st.walkDescendants(re, func(e *NodeEntry) {
		if found != nil {
			return
		}
		for i := range prog.Groups {
			if st.matchGroup(e, &prog.Groups[i]) {
				found = e
				return
			}
		}
	})
	if found == nil {
		return TaggedOf("None", nil), nil
	}
	return TaggedOf("Some", []Value{st.Nodes.NodeOf(found.ID)}), nil
}

// MatchesSelector reports whether node itself matches sel (Section 8).
func (st *State) MatchesSelector(node Value, sel *Selector) (Value, error) {
	e, err := st.nodeEntry(node)
	if err != nil {
		return Unit(), err
	}
	prog, err := st.selProg(sel)
	if err != nil {
		return Unit(), err
	}
	for i := range prog.Groups {
		if st.matchGroup(e, &prog.Groups[i]) {
			return Bool(true), nil
		}
	}
	return Bool(false), nil
}
