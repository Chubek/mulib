package runtime

import (
	"bytes"
	"fmt"
	"testing"
)

func mustCompileSel(t *testing.T, src string) *Selector {
	t.Helper()
	sel, err := CompileSelector(src)
	if err != nil {
		t.Fatalf("selector %q: %v", src, err)
	}
	return sel
}

func TestValueEquality(t *testing.T) {
	if !Equals(Int(1), Float(1.0)) {
		t.Fatal("Int(1) should equal Float(1.0)")
	}
	if Equals(Int(1), Str("1")) {
		t.Fatal("Int should not equal String")
	}
	if !Equals(ListOf([]Value{Int(1), Int(2)}), ListOf([]Value{Int(1), Int(2)})) {
		t.Fatal("lists should compare structurally")
	}
	if Equals(TaggedOf("Some", []Value{Int(1)}), TaggedOf("None", nil)) {
		t.Fatal("tagged lists with different tags are unequal")
	}
	t1 := TableOf([][2]Value{{Tag("a"), Int(1)}})
	t2 := TableOf([][2]Value{{Tag("a"), Int(1)}})
	if !Equals(t1, t2) {
		t.Fatal("tables should compare structurally")
	}
	st := NewState()
	n1, n2 := st.CreateElement("div"), st.CreateElement("div")
	if Equals(n1, n2) {
		t.Fatal("nodes compare by identity, not structure")
	}
	if !Equals(n1, st.Nodes.NodeOf(n1.ref.(*Node).id)) {
		t.Fatal("same node handle should be equal")
	}
	s1 := mustCompileSel(t, "div > p")
	s2 := mustCompileSel(t, "div > p")
	if !Equals(SelectorOf(s1), SelectorOf(s2)) {
		t.Fatal("selectors compare by source")
	}
}

func TestTableOps(t *testing.T) {
	tbl := NewTable()
	if err := tbl.Set(Int(1), Str("one")); err != nil {
		t.Fatal(err)
	}
	// Int and Float share the numeric namespace (4.3).
	if v, ok := tbl.Get(Float(1.0)); !ok || v.s != "one" {
		t.Fatalf("Float(1.0) should hit the Int(1) slot, got %v %v", v, ok)
	}
	if err := tbl.Set(Str("k"), Unit()); err != nil {
		t.Fatal(err)
	}
	if err := tbl.Set(Tag("t"), Unit()); err != nil {
		t.Fatal(err)
	}
	// Insertion order for Keys.
	ks := tbl.Keys()
	if len(ks) != 3 || ks[0].i != 1 || ks[1].s != "k" || ks[2].s != "t" {
		t.Fatalf("unexpected key order: %s", Repr(ListOf(ks)))
	}
	if !tbl.Delete(Int(1)) || tbl.Len() != 2 {
		t.Fatal("Delete should remove the numeric entry")
	}
	// Lists are not valid keys.
	if err := tbl.Set(EmptyList(), Unit()); err == nil {
		t.Fatal("list key should be rejected")
	}
}

func TestNodeTree(t *testing.T) {
	st := NewState()
	doc := st.CreateDocument()
	div := st.CreateElement("div")
	p := st.CreateElement("p")
	txt := st.CreateText("hello")
	for _, pair := range [][2]Value{{doc, div}, {div, p}, {p, txt}} {
		if err := st.AppendChild(pair[0], pair[1]); err != nil {
			t.Fatal(err)
		}
	}
	kids, err := st.ChildrenOf(div)
	if err != nil {
		t.Fatal(err)
	}
	if kids.List().Tag != "" || len(kids.List().Elems) != 1 {
		t.Fatalf("ChildrenOf: %s", Repr(kids))
	}
	par, err := st.ParentOf(txt)
	if err != nil {
		t.Fatal(err)
	}
	if par.List().Tag != "Some" {
		t.Fatalf("ParentOf(text) should be :Some, got %s", Repr(par))
	}
	// Move semantics: appending p under doc detaches it from div.
	if err := st.AppendChild(doc, p); err != nil {
		t.Fatal(err)
	}
	kids, _ = st.ChildrenOf(div)
	if len(kids.List().Elems) != 0 {
		t.Fatal("append should move, not copy")
	}
	// Cycle: appending a node under its own descendant fails (p is an
	// ancestor of inner).
	inner := st.CreateElement("span")
	if err := st.AppendChild(p, inner); err != nil {
		t.Fatal(err)
	}
	if err := st.AppendChild(inner, p); err == nil {
		t.Fatal("cycle should fail")
	}
	// Attrs.
	if err := st.SetAttr(div, "class", Str("card wide")); err != nil {
		t.Fatal(err)
	}
	if v, err := st.GetAttr(div, "class"); err != nil || v.List().Tag != "Some" {
		t.Fatalf("GetAttr: %v %v", v, err)
	}
	if err := st.RemoveAttr(div, "class"); err != nil {
		t.Fatal(err)
	}
	if v, _ := st.GetAttr(div, "class"); v.List().Tag != "None" {
		t.Fatal("attr should be gone")
	}
	// ReplaceChild: doc holds [div, p] here; p swaps for np.
	np := st.CreateElement("p")
	if err := st.ReplaceChild(doc, p, np); err != nil {
		t.Fatal(err)
	}
	kids, _ = st.ChildrenOf(doc)
	elems := kids.List().Elems
	if len(elems) != 2 || elems[1].ref.(*Node).id != np.ref.(*Node).id {
		t.Fatalf("ReplaceChild result: %s", Repr(kids))
	}
	// ReleaseNode detaches and dead-ends later ops.
	id := inner.ref.(*Node).id
	if err := st.ReleaseNode(id); err != nil {
		t.Fatal(err)
	}
	if _, err := st.ChildrenOf(inner); err == nil {
		t.Fatal("ops on a released handle should fail")
	}
}

// selTree builds:
//
//	document
//	  div#main.card
//	    p.lead   "a"
//	    p        "b"
//	    span.note
//	  div
//	    p        "c"
func selTree(t *testing.T) (*State, Value) {
	t.Helper()
	st := NewState()
	doc := st.CreateDocument()
	main := st.CreateElement("div")
	other := st.CreateElement("div")
	lead := st.CreateElement("p")
	second := st.CreateElement("p")
	note := st.CreateElement("span")
	third := st.CreateElement("p")
	must(t, st.AppendChild(doc, main))
	must(t, st.AppendChild(doc, other))
	must(t, st.AppendChild(main, lead))
	must(t, st.AppendChild(main, second))
	must(t, st.AppendChild(main, note))
	must(t, st.AppendChild(other, third))
	must(t, st.SetAttr(main, "id", Str("main")))
	must(t, st.SetAttr(main, "class", Str("card deck")))
	must(t, st.SetAttr(lead, "class", Str("lead")))
	must(t, st.SetAttr(note, "class", Str("note")))
	return st, doc
}

func must(t *testing.T, err error) {
	t.Helper()
	if err != nil {
		t.Fatal(err)
	}
}

func tagCount(v Value) int { return len(v.List().Elems) }

func TestSelectorCompileErrors(t *testing.T) {
	for _, bad := range []string{"[[not valid", "div >", "p:", "[a=", "p::before", "p:nth-child(x)"} {
		if _, err := CompileSelector(bad); err == nil {
			t.Fatalf("%q should not compile", bad)
		}
	}
	if _, err := CompileSelector(""); err == nil {
		t.Fatal("empty selector should not compile")
	}
}

func TestSelectorQuery(t *testing.T) {
	st, doc := selTree(t)
	cases := []struct {
		sel  string
		want int
	}{
		{"div", 2},
		{".card > p", 2},
		{"div.card > p", 2},
		{"#main > p", 2},
		{".note", 1},
		{"div .note", 1},
		{"div > .note", 1},
		{"p + span", 1},
		{"p ~ span", 1},
		{"p:first-child", 2},
		{"p:last-child", 1},
		{"p:nth-child(2)", 1},
		{"span:nth-child(odd)", 1},
		{"[class~=card]", 1},
		{"[class^=le]", 1},
		{"[class$=te]", 1},
		{"[class*=ead]", 1},
		{"p:not(.lead)", 2},
		{"div, span", 3},
		{"*", 6},
		{"div:empty", 0},
	}
	for _, tc := range cases {
		sel := mustCompileSel(t, tc.sel)
		got, err := st.QueryAll(doc, sel)
		if err != nil {
			t.Fatalf("%q: %v", tc.sel, err)
		}
		if tagCount(got) != tc.want {
			t.Fatalf("%q: want %d matches, got %s", tc.sel, tc.want, Repr(got))
		}
	}
	// QueryFirst.
	sel := mustCompileSel(t, ".card > p")
	first, err := st.QueryFirst(doc, sel)
	if err != nil || first.List().Tag != "Some" {
		t.Fatalf("QueryFirst: %s %v", Repr(first), err)
	}
	none, err := st.QueryFirst(doc, mustCompileSel(t, "article"))
	if err != nil || none.List().Tag != "None" {
		t.Fatalf("QueryFirst miss: %s %v", Repr(none), err)
	}
}

func TestMatchesSelector(t *testing.T) {
	st, doc := selTree(t)
	kids, err := st.ChildrenOf(doc)
	if err != nil {
		t.Fatal(err)
	}
	main := kids.List().Elems[0]
	mk, err := st.MatchesSelector(main, mustCompileSel(t, "div.card#main"))
	if err != nil || !mk.b {
		t.Fatalf("main should match div.card#main: %v %v", mk, err)
	}
	mk, err = st.MatchesSelector(main, mustCompileSel(t, "div > p"))
	if err != nil || mk.b {
		t.Fatal("main should not match div > p")
	}
	_ = tagCount
}

func TestDisplay(t *testing.T) {
	if got := Display(Str("hi")); got != "hi" {
		t.Fatalf("Display(String): %q", got)
	}
	if got := Repr(Str("hi")); got != "\"hi\"" {
		t.Fatalf("Repr(String): %q", got)
	}
	if got := Display(Int(-42)); got != "-42" {
		t.Fatalf("Display(Int): %q", got)
	}
	l := TaggedOf("Some", []Value{Int(1)})
	if got := Display(l); got != ":Some[1]" {
		t.Fatalf("Display(Some): %q", got)
	}
	tbl := NewTable()
	_ = tbl.Set(Tag("a"), Int(1))
	if got := Display(Value{kind: KindTable, ref: tbl}); got != "%{:a => 1}" {
		t.Fatalf("Display(Table): %q", got)
	}
	st := NewState()
	if got := Display(st.CreateElement("div")); got != "<div>" {
		t.Fatalf("Display(Node): %q", got)
	}
}

func TestAsyncLifecycle(t *testing.T) {
	st := NewState()
	tok1 := st.NewAsyncToken()
	tok2 := st.NewAsyncToken()
	st.ParkAsync(tok1, "runner-1")
	st.ParkAsync(tok2, "runner-2")
	if st.PendingCount() != 2 {
		t.Fatalf("PendingCount = %d", st.PendingCount())
	}
	st.ResolveAsync(tok2, Int(20), nil)
	st.ResolveAsync(tok1, Int(10), nil)
	resolved := st.TakeResolved()
	if len(resolved) != 2 || resolved[0] != tok2 || resolved[1] != tok1 {
		t.Fatalf("resolution order should be honored: %v", resolved)
	}
	if len(st.TakeResolved()) != 0 {
		t.Fatal("TakeResolved should drain")
	}
}

func TestEvents(t *testing.T) {
	st := NewState()
	btn := st.CreateElement("button")
	calls := 0
	st.CallDomscript = func(fn *Fn, args []Value) (Value, error) {
		calls++
		if len(args) != 1 || args[0].kind != KindTable {
			return Unit(), fmt.Errorf("handler args: %s", Repr(ListOf(args)))
		}
		ty, ok := args[0].Table().Get(Tag("type"))
		if !ok || ty.s != "click" {
			return Unit(), fmt.Errorf("event type: %s", Repr(ty))
		}
		return Unit(), nil
	}
	handler := &Fn{Name: "h", NParams: 1}
	sub, err := st.Subscribe(btn, "click", handler)
	if err != nil {
		t.Fatal(err)
	}
	if sub.List().Tag != "Subscription" {
		t.Fatalf("Subscribe result: %s", Repr(sub))
	}
	if err := st.Emit(btn, "click", Int(7)); err != nil {
		t.Fatal(err)
	}
	if calls != 1 {
		t.Fatalf("handler calls = %d", calls)
	}
	if err := st.Off(sub); err != nil {
		t.Fatal(err)
	}
	if err := st.Emit(btn, "click", Unit()); err != nil {
		t.Fatal(err)
	}
	if calls != 1 {
		t.Fatal("Off should detach the handler")
	}
}

func TestModuleRegistry(t *testing.T) {
	st := NewState()
	exp := NewTable()
	_ = exp.Set(Tag("pi"), Float(3.14))
	st.RegisterModule(&Module{Name: "math", Exports: Value{kind: KindTable, ref: exp}})
	m, ok := st.LookupModule("math")
	if !ok || m.Name != "math" {
		t.Fatal("module should be registered")
	}
	if v, ok := m.Exports.Table().Get(Tag("pi")); !ok || v.f != 3.14 {
		t.Fatalf("module exports: %v", v)
	}
	if len(st.LoadedModules()) != 1 {
		t.Fatal("LoadedModules")
	}
}

func TestPrintSink(t *testing.T) {
	st := NewState()
	var buf bytes.Buffer
	st.Output = &buf
	st.Print(Str("hello"))
	st.Print(Int(5))
	if buf.String() != "hello\n5\n" {
		t.Fatalf("Print output: %q", buf.String())
	}
}
