#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MULIB_IMPL_BUFFER
#include "mulib-buffer.h"
#define MULIB_IMPL_HTML_TEXTSTREAM
#include "mulib-html-textstream.h"

/* Fail precisely the nth allocation, and account for owners across retries.
   Install the hooks after libc and the independent oracle implementations. */
static size_t allocation, fail_at, live;
static int fault(void)
{
  return ++allocation == fail_at;
}
static void *checked_malloc(size_t bytes)
{
  void *p = fault() ? NULL : malloc(bytes);
  if (p) ++live;
  return p;
}
static void *checked_calloc(size_t count, size_t width)
{
  void *p = fault() ? NULL : calloc(count, width);
  if (p) ++live;
  return p;
}
static void *checked_realloc(void *p, size_t bytes)
{
  int fresh = p == NULL;
  void *next = fault() ? NULL : realloc(p, bytes);
  if (next && fresh) ++live;
  return next;
}
static void checked_free(void *p)
{
  if (p) {
    assert(live != 0);
    --live;
  }
  free(p);
}

#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_CDD_MARKUP_TREE
#define MULIB_IMPL_CDD_MARKUP_STREAM
#define MULIB_IMPL_CDD_MARKUP_SPANS
#define MULIB_IMPL_CDD_MARKUP_GRAPH
#define MULIB_IMPL_CDD_MARKUP_BRIDGE
#include "mulib-markup-bridge.h"
#undef free
#undef realloc
#undef calloc
#undef malloc

static void expect_run(const mulib_mkir_run_t *r, size_t begin, size_t end,
                       const size_t *ids, size_t count)
{
  assert(r->begin == begin && r->end == end && r->count == count);
  if (count) assert(!memcmp(r->annotations, ids, count * sizeof(size_t)));
  else assert(!r->annotations);
}

static void test_spans(void)
{
  const char bytes[] = {'a', 0, 'b', (char)0xff};
  const size_t ids[] = {0, 2};
  mulib_mkir_spans_t s = {0}, invalid = {0};
  mulib_mkir_runs_t r = {0};
  mulib_mkir_tree_t t = {0};
  mulib_buffer_t oracle;
  size_t id = 99;
  invalid.item_capacity = 1;
  assert(!mulib_mkir_spans_init(&invalid, NULL, 0));
  assert(!mulib_mkir_spans_init(&s, bytes, SIZE_MAX));
  assert(mulib_mkir_spans_init(&s, bytes, sizeof(bytes)));
  mulib_buffer_init(&oracle);
  assert(mulib_buffer_append(&oracle, bytes, sizeof(bytes)));
  assert(mulib_buffer_append(&oracle, oracle.data + 1, sizeof(bytes) - 1));
  assert(mulib_mkir_spans_append(&s, s.text + 1, sizeof(bytes) - 1));
  assert(s.length == oracle.length && !memcmp(s.text, oracle.data, oracle.length));
  assert(s.capacity > s.length + 1);
  assert(!mulib_mkir_spans_append(&s, s.text + s.length + 1, 1));
  assert(!mulib_mkir_spans_append(&s, s.text + s.length, 2));
  assert(!mulib_mkir_spans_append(&s, bytes, SIZE_MAX));
  assert(mulib_mkir_spans_append(&s, s.text + s.length, 1));
  assert(s.length == oracle.length + 1 && s.text[s.length - 1] == 0);
  mulib_buffer_free(&oracle);
  assert(!mulib_mkir_spans_add(&s, 5, 4, "bad", 0, &id) && id == 99);
  assert(mulib_mkir_spans_add(&s, 1, 6, "wide", 3, &id) && id == 0);
  assert(mulib_mkir_spans_add(&s, 3, 3, "bookmark", 7, NULL));
  assert(mulib_mkir_spans_add(&s, 2, 7, "overlap", 0, NULL));
  assert(!mulib_cdd_mkir_spans_runs_limited(&s, 4, 6, &r) && !r.items && !r.count);
  assert(!mulib_cdd_mkir_spans_runs_limited(&s, 5, 3, &r) && !r.items && !r.count);
  assert(mulib_cdd_mkir_spans_runs_limited(&s, 5, 4, &r));
  assert(r.count == 5);
  expect_run(&r.items[0], 0, 1, NULL, 0);
  expect_run(&r.items[1], 1, 2, ids, 1);
  expect_run(&r.items[2], 2, 6, ids, 2);
  expect_run(&r.items[3], 6, 7, ids + 1, 1);
  expect_run(&r.items[4], 7, 8, NULL, 0);
  assert(!mulib_mkir_spans_runs(&s, &r) && r.count == 5);
  assert(!mulib_mkir_spans_to_tree(&s, &t) && !t.nodes);
  assert(s.items[1].begin == 3 && s.items[1].end == 3 && s.items[1].flags == 7);
  mulib_mkir_runs_free(&r);
  invalid = s;
  invalid.capacity = invalid.length;
  assert(!mulib_mkir_spans_validate(&invalid));
  assert(!mulib_mkir_spans_append(&invalid, bytes, 1));
  invalid = s;
  invalid.item_capacity = SIZE_MAX;
  assert(!mulib_mkir_spans_validate(&invalid));
  mulib_mkir_spans_free(&s);
  assert(mulib_mkir_spans_init(&s, NULL, 0));
  assert(mulib_cdd_mkir_spans_runs_limited(&s, 0, 0, &r) && !r.items && !r.count);
  assert(mulib_mkir_spans_to_tree(&s, &t) && t.count == 1);
  mulib_mkir_tree_free(&t);
  mulib_mkir_spans_free(&s);
}

static void test_graph(void)
{
  mulib_mkir_graph_t g = {0}, broken;
  const size_t want[] = {1, 2, 3, 0, 4, 5};
  const size_t sentinels[] = {90, 91, 92, 93, 94, 95, 96};
  size_t members[] = {4, 0, 4}, order[7], id = 99;
  char edge_type[] = "edge";
  assert(mulib_mkir_graph_toposort(&g, NULL, 0));
  g.acyclic = 1;
  for (size_t i = 0; i < 6; ++i) {
    assert(mulib_mkir_graph_vertex(&g, MULIB_MKIR_REF_FREE, i, &id) && id == i);
  }
  assert(mulib_mkir_graph_edge(&g, 3, 0, edge_type, &id) && id == 0);
  edge_type[0] = 'X';
  assert(!strcmp(g.edges[0].type, "edge"));
  assert(mulib_mkir_graph_edge(&g, 1, 3, "parallel", NULL));
  assert(mulib_mkir_graph_edge(&g, 1, 3, "parallel", NULL));
  assert(mulib_mkir_graph_edge(&g, 2, 3, "parent", NULL));
  assert(mulib_mkir_graph_edge(&g, 0, 4, "tail", NULL));
  assert(mulib_mkir_graph_hyperedge(&g, members, 3, "ordered", &id) && id == 0);
  members[0] = 1;
  assert(g.hyperedges[0].members[0] == 4 && g.hyperedges[0].members[2] == 4);
  assert(!mulib_mkir_graph_hyperedge(&g, members, SIZE_MAX, "bad", &id) && id == 0);
  assert(mulib_mkir_graph_path(&g, 1, 4) == 1);
  assert(mulib_mkir_graph_path(&g, 1, 5) == 0);
  assert(mulib_mkir_graph_path(&g, 4, 0) == 0);
  assert(mulib_mkir_graph_path(&g, 5, 5) == 1);
  assert(mulib_mkir_graph_path(&g, 5, 6) == -1);
  assert(!mulib_mkir_graph_edge(&g, 4, 1, "cycle", &id) && id == 0);
  assert(!mulib_mkir_graph_edge(&g, 5, 5, "self", &id) && id == 0);
  memcpy(order, sentinels, sizeof(order));
  assert(!mulib_mkir_graph_toposort(&g, order, 5) && !memcmp(order, sentinels, sizeof(order)));
  assert(mulib_mkir_graph_toposort(&g, order, 7));
  assert(!memcmp(order, want, sizeof(want)) && order[6] == 96);
  assert(mulib_mkir_graph_property(&g, MULIB_MKIR_P_VERTEX, 0, "key", "x"));
  assert(mulib_mkir_graph_property(&g, MULIB_MKIR_P_EDGE, 0, "key", ""));
  assert(mulib_mkir_graph_property(&g, MULIB_MKIR_P_HYPEREDGE, 0, "key", "z"));
  assert(mulib_mkir_graph_property(&g, MULIB_MKIR_P_VERTEX, 0, g.properties[0].key,
                                   g.properties[0].value));
  assert(!strcmp(mulib_mkir_graph_get_property(&g, MULIB_MKIR_P_VERTEX, 0, "key"), "x"));
  assert(!mulib_mkir_graph_property(&g, MULIB_MKIR_P_HYPEREDGE, 1, "key", "invalid"));
  assert(g.property_count == 3 && mulib_mkir_graph_validate(&g));
  g.properties[1].owner = MULIB_MKIR_P_VERTEX;
  assert(!mulib_mkir_graph_validate(&g));
  g.properties[1].owner = MULIB_MKIR_P_EDGE;
  broken = g;
  broken.edges = NULL;
  assert(mulib_mkir_graph_path(&broken, 5, 5) == -1);
  broken = g;
  broken.edge_count = SIZE_MAX;
  assert(!mulib_mkir_graph_toposort(&broken, order, 7));
  g.edges[0].from = SIZE_MAX;
  assert(mulib_mkir_graph_path(&g, 5, 5) == -1);
  assert(!mulib_mkir_graph_validate(&g));
  g.edges[0].from = 3;
  g.acyclic = 0;
  assert(mulib_mkir_graph_edge(&g, 4, 1, "cycle", NULL));
  memcpy(order, sentinels, sizeof(order));
  assert(mulib_mkir_graph_validate(&g));
  assert(!mulib_mkir_graph_toposort(&g, order, 7) && !memcmp(order, sentinels, sizeof(order)));
  g.acyclic = 1;
  assert(!mulib_mkir_graph_validate(&g));
  mulib_mkir_graph_free(&g);
}

static int collect_html_text(void *data, const mulib_html_textstream_event_t *event)
{
  return event->kind != MULIB_HTML_STREAM_TEXT ||
         mulib_buffer_append((mulib_buffer_t *)data, event->start, event->len);
}

static void test_bridge(void)
{
  const char html[] = "<p><em>raw &amp; text</em><!--ignored--></p><br/>";
  const char binary[] = {'x', '\0', 'y'};
  mulib_mkir_tree_t t = {0}, copy = {0}, rejected = {0};
  mulib_mkir_spans_t s = {0}, invalid = {0};
  mulib_mkir_graph_t g = {0};
  mulib_mkir_hybrid_t h = {0};
  mulib_buffer_t text;
  size_t root, first, second, child, added;
  mulib_buffer_init(&text);
  assert(mulib_html_textstream_scan(html, sizeof(html) - 1, collect_html_text, &text, 20));
  assert(mulib_mkir_parse(html, sizeof(html) - 1, 20, &t, NULL));
  invalid.capacity = 1;
  assert(!mulib_mkir_tree_to_spans(&t, 0, &invalid) && invalid.capacity == 1);
  assert(mulib_mkir_tree_to_spans(&t, 0, &s));
  assert(s.length == text.length && !memcmp(s.text, text.data, text.length));
  assert(s.count == 3 && s.items[2].begin == text.length && s.items[2].end == text.length);
  assert(!mulib_mkir_spans_to_tree(&s, &copy));
  rejected.attr_capacity = 1;
  assert(!mulib_mkir_spans_to_tree(&s, &rejected) && rejected.attr_capacity == 1);
  mulib_buffer_free(&text);
  mulib_mkir_spans_free(&s);
  mulib_mkir_tree_free(&t);

  assert(mulib_mkir_tree_add(&t, MULIB_MKIR_NONE, MULIB_MKIR_DOCUMENT, NULL, NULL, 0, 0, 8, &root));
  assert(mulib_mkir_tree_add(&t, root, MULIB_MKIR_ELEMENT, "inner", NULL, 0, 1, 3, &first));
  assert(mulib_mkir_tree_add(&t, root, MULIB_MKIR_ELEMENT, "outer", NULL, 0, 0, 8, &second));
  assert(mulib_mkir_tree_add(&t, first, MULIB_MKIR_CDATA, NULL, binary, 3, 1, 3, &child));
  assert(mulib_mkir_tree_attach(&t, first, second));
  assert(mulib_mkir_tree_attr(&t, first, "class", "kept"));
  assert(mulib_mkir_tree_to_spans(&t, second, &s));
  assert(s.count == 2 && !strcmp(s.items[0].label, "outer") && !strcmp(s.items[1].label, "inner"));
  assert(s.length == sizeof(binary) && !memcmp(s.text, binary, sizeof(binary)));
  assert(mulib_mkir_spans_to_tree(&s, &copy));
  assert(copy.count == 4 && copy.nodes[2].parent == 1);
  assert(!strcmp(copy.nodes[1].name, "outer") && !strcmp(copy.nodes[2].name, "inner"));
  assert(copy.nodes[3].value_length == sizeof(binary) &&
         !memcmp(copy.nodes[3].value, binary, sizeof(binary)));
  g.property_capacity = 1;
  assert(!mulib_mkir_tree_to_graph(&t, &g) && g.property_capacity == 1);
  assert(!mulib_mkir_spans_to_graph(&s, &g) && g.property_capacity == 1);
  g.property_capacity = 0;
  assert(!mulib_cdd_mkir_spans_to_graph_limited(&s, 1, 1, &g) && !g.vertices);
  assert(mulib_cdd_mkir_spans_to_graph_limited(&s, 1, 2, &g));
  assert(g.acyclic && g.vertex_count == 3 && g.edge_count == 2 && g.hyper_count == 1);
  assert(g.edges[0].from == 0 && g.edges[1].from == 1 && g.edges[1].to == 2);
  assert(g.hyperedges[0].count == 3 && g.hyperedges[0].members[0] == 2 &&
         g.hyperedges[0].members[1] == 0 && g.hyperedges[0].members[2] == 1);
  assert(mulib_mkir_hybrid_from_tree(&t, second, &h));
  assert(h.tree.nodes[first].parent == second && h.tree.nodes[child].parent == first);
  assert(h.tree.nodes[first].name != t.nodes[first].name);
  assert(!strcmp(mulib_mkir_tree_get_attr(&h.tree, first, "class"), "kept"));
  assert(h.spans.count == 2 && h.spans.length == sizeof(binary));
  assert(mulib_mkir_hybrid_reference(&h, child, second, "back"));
  assert(mulib_mkir_hybrid_anchor(&h, second, 0, "anchor"));
  assert(mulib_mkir_spans_add(&h.spans, 0, 1, "new", 0, &added) && added == 2);
  assert(!mulib_mkir_hybrid_anchor(&h, second, added, "stale-run"));
  assert(mulib_mkir_tree_add(&h.tree, second, MULIB_MKIR_TEXT, NULL, "", 0, 0, 0, &added));
  assert(!mulib_mkir_hybrid_reference(&h, added, second, "stale-vertex"));
  assert(mulib_mkir_hybrid_index(&h));
  assert(mulib_mkir_hybrid_anchor(&h, added, 2, "indexed"));
  assert(mulib_mkir_hybrid_reference(&h, added, second, "indexed"));
  assert(!strcmp(h.graph.edges[h.graph.edge_count - 2].type, "indexed"));
  for (size_t i = 0; i < h.graph.edge_count; ++i) {
    assert(strcmp(h.graph.edges[i].type, "back") && strcmp(h.graph.edges[i].type, "anchor"));
  }
  mulib_mkir_hybrid_free(&h);
  mulib_mkir_graph_free(&g);
  mulib_mkir_tree_free(&copy);
  mulib_mkir_spans_free(&s);
  mulib_mkir_tree_free(&t);
}

static void test_allocation_failures(void)
{
  const char src[] = "<a>left<b>inside</b>right</a>";
  mulib_mkir_tree_t t = {0};
  mulib_mkir_spans_t s = {0};
  mulib_mkir_graph_t g = {0};
  mulib_mkir_hybrid_t h = {0};
  size_t baseline, n, order[] = {99, 98, 97};
  {
    mulib_mkir_spans_t append = {0};
    char *before;
    size_t id = 99;
    assert(mulib_mkir_spans_init(&append, "xy", 2));
    before = append.text;
    baseline = live;
    allocation = 0; fail_at = 1;
    assert(!mulib_mkir_spans_append(&append, append.text, 2));
    assert(append.text == before && append.length == 2 && append.capacity == 3);
    assert(!memcmp(append.text, "xy", 3) && live == baseline);
    for (n = 1; n <= 2; ++n) {
      allocation = 0; fail_at = n;
      assert(!mulib_mkir_spans_add(&append, 0, 1, "x", 0, &id));
      assert(!append.items && !append.count && !append.item_capacity && id == 99);
      assert(live == baseline);
    }
    fail_at = 0;
    mulib_mkir_spans_free(&append);
  }
  assert(mulib_mkir_parse(src, sizeof(src) - 1, 8, &t, NULL));
  assert(mulib_mkir_tree_to_spans(&t, 0, &s));
  baseline = live;
  for (n = 1; n < 256; ++n) {
    mulib_mkir_runs_t runs = {0};
    mulib_mkir_graph_t graph = {0};
    mulib_mkir_tree_t copy = {0};
    mulib_mkir_hybrid_t hybrid = {0};
    int r, d, c, y;
    allocation = 0; fail_at = n;
    r = mulib_mkir_spans_runs(&s, &runs);
    if (r) mulib_mkir_runs_free(&runs);
    else assert(!runs.items && !runs.count);
    assert(live == baseline);
    allocation = 0;
    d = mulib_mkir_spans_to_graph(&s, &graph);
    if (d) mulib_mkir_graph_free(&graph);
    else assert(!graph.vertices && !graph.edges && !graph.hyperedges);
    assert(live == baseline);
    allocation = 0;
    c = mulib_mkir_spans_to_tree(&s, &copy);
    if (c) mulib_mkir_tree_free(&copy);
    else assert(!copy.nodes && !copy.count);
    assert(live == baseline);
    allocation = 0;
    y = mulib_mkir_hybrid_from_tree(&t, 0, &hybrid);
    if (y) mulib_mkir_hybrid_free(&hybrid);
    else assert(!hybrid.tree.nodes && !hybrid.spans.text && !hybrid.graph.vertices);
    assert(live == baseline);
    fail_at = 0;
    if (r && d && c && y) break;
  }
  assert(n > 8 && n < 256);
  assert(mulib_mkir_hybrid_from_tree(&t, 0, &h));
  assert(mulib_mkir_hybrid_reference(&h, 0, 0, "custom"));
  for (n = 1; n < 256; ++n) {
    size_t edges = h.graph.edge_count, span_begin = h.span_vertex_begin, run_begin = h.run_vertex_begin;
    mulib_mkir_edge_t *old_edges = h.graph.edges;
    int result;
    baseline = live;
    allocation = 0; fail_at = n;
    result = mulib_mkir_hybrid_index(&h);
    fail_at = 0;
    if (result) break;
    assert(live == baseline && h.graph.edges == old_edges && h.graph.edge_count == edges);
    assert(h.span_vertex_begin == span_begin && h.run_vertex_begin == run_begin);
    assert(!strcmp(h.graph.edges[edges - 1].type, "custom"));
  }
  assert(n > 8 && n < 256);
  assert(mulib_mkir_graph_vertex(&g, MULIB_MKIR_REF_FREE, 0, NULL));
  assert(mulib_mkir_graph_vertex(&g, MULIB_MKIR_REF_FREE, 0, NULL));
  assert(mulib_mkir_graph_vertex(&g, MULIB_MKIR_REF_FREE, 0, NULL));
  assert(mulib_mkir_graph_edge(&g, 0, 1, "child", NULL));
  assert(mulib_mkir_graph_edge(&g, 1, 2, "child", NULL));
  assert(mulib_mkir_graph_property(&g, MULIB_MKIR_P_VERTEX, 0, "key", "old"));
  baseline = live;
  for (n = 1; n <= 3; ++n) {
    allocation = 0; fail_at = n;
    assert(mulib_mkir_graph_path(&g, 0, 2) == -1 && live == baseline);
    if (n <= 2) {
      allocation = 0;
      assert(!mulib_mkir_graph_toposort(&g, order, 3) && live == baseline);
      assert(order[0] == 99 && order[1] == 98 && order[2] == 97);
    }
  }
  allocation = 0; fail_at = 1;
  assert(!mulib_mkir_graph_property(&g, MULIB_MKIR_P_VERTEX, 0, "key", "new"));
  assert(!strcmp(mulib_mkir_graph_get_property(&g, MULIB_MKIR_P_VERTEX, 0, "key"), "old"));
  fail_at = 0;
  assert(live == baseline);
  mulib_mkir_graph_free(&g);
  mulib_mkir_hybrid_free(&h);
  mulib_mkir_spans_free(&s);
  mulib_mkir_tree_free(&t);
}

int main(void)
{
  test_spans();
  test_graph();
  test_bridge();
  test_allocation_failures();
  assert(live == 0);
  puts("markup spans, graph and bridge tests passed");
  return 0;
}
