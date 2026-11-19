/* Strict C11 headless regression. See manual/mulib-gfx-retgtk-model-layout.md. */
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long fail_after = -1;
static size_t allocations;
static int allocation_fails(void)
{
    ++allocations;
    if (fail_after == 0) { fail_after = -1; return 1; }
    if (fail_after > 0) --fail_after;
    return 0;
}
static void *checked_malloc(size_t n)
{
    return allocation_fails() ? NULL : malloc(n);
}
static void *checked_calloc(size_t n, size_t s)
{
    return allocation_fails() ? NULL : calloc(n, s);
}
static void *checked_realloc(void *p, size_t n)
{
    return allocation_fails() ? NULL : realloc(p, n);
}

/* Inject failures into explicit header allocations, including grow/realloc. */
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#include "mulib-retgtk-box.h"
#include "mulib-retgtk-flex.h"
#include "mulib-retgtk-grid.h"
#include "mulib-retgtk-button.h"
#include "mulib-retgtk-label.h"
#include "mulib-retgtk-hotreload.h"
#include "mulib-retgtk-listmodel.h"
#include "mulib-retgtk-proxymodel.h"
#include "mulib-retgtk-tablemodel.h"
#include "mulib-retgtk-tableview.h"
#include "mulib-retgtk-treemodel.h"
#include "mulib-retgtk-textview.h"
#undef malloc
#undef calloc
#undef realloc

mulib_retgtk_widget_t *gfx_retgtk_peer_toggle(uint64_t);
mulib_retgtk_widget_t *gfx_retgtk_peer_entry(uint64_t);

static unsigned measures;
static void leaf_measure(mulib_retgtk_widget_t *w, mulib_gui_constraints_t c,
                         mulib_gui_size_t *s)
{
    (void)c;
    ++measures;
    *s = w->desired;
}
static const mulib_retgtk_widget_ops leaf_ops = {
    NULL, leaf_measure, NULL, NULL, NULL
};
static mulib_retgtk_widget_t *leaf(uint64_t id, int width, int height)
{
    mulib_retgtk_widget_t *w = mulib_retgtk_widget_create(id, &leaf_ops);
    assert(w);
    w->desired = (mulib_gui_size_t){width, height};
    return w;
}
static void adopt(mulib_retgtk_widget_t *parent, mulib_retgtk_widget_t *child)
{
    assert(parent && child && mulib_retgtk_widget_add(parent, child));
    mulib_retgtk_widget_unref(child);
    assert(child->parent == parent && child->object.refs == 1);
}

static void test_layout(void)
{
    mulib_retgtk_widget_t *box = mulib_retgtk_box(1, 0, 3, 2);
    mulib_retgtk_widget_t *a = leaf(2, 30, 10), *b = leaf(3, 20, 25);
    mulib_retgtk_widget_t *hidden = leaf(4, 1000, 1000);
    a->weight = 0;
    hidden->visible = 0;
    hidden->bounds = (mgx_rect){7, 8, 9, 10};
    adopt(box, a); adopt(box, hidden); adopt(box, b);
    measures = 0;
    mulib_gui_size_t size = mulib_retgtk_widget_measure(box,
        (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX});
    assert(size.w == 57 && size.h == 29 && measures == 2);
    size = mulib_retgtk_widget_measure(box,
        (mulib_gui_constraints_t){60, 40, 70, 50});
    assert(size.w == 60 && size.h == 40);
    mulib_retgtk_widget_arrange(box, (mgx_rect){10, 20, 100, 40});
    assert(a->bounds.x == 12 && a->bounds.y == 22 && a->bounds.w == 30);
    assert(b->bounds.x == 45 && b->bounds.w == 63 && b->bounds.h == 36);
    assert(hidden->bounds.x == 7 && hidden->bounds.w == 9);
    mulib_retgtk_widget_arrange(box, (mgx_rect){0, 0, 37, 40});
    assert(a->bounds.w == 18 && b->bounds.w == 12 && b->bounds.x == 23);
    mulib_retgtk_widget_t *outer = mulib_retgtk_box(5, 1, 0, 0);
    adopt(outer, box);
    size = mulib_retgtk_widget_measure(outer,
        (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX});
    assert(size.w == 57 && size.h == 29);
    mulib_retgtk_widget_unref(outer);

    box = mulib_retgtk_flex(10, 0, 0);
    a = mulib_retgtk_constraint_box(11, (mulib_gui_constraints_t){20, 0, 20, 8});
    b = mulib_retgtk_constraint_box(12, (mulib_gui_constraints_t){10, 0, 30, INT_MAX});
    mulib_retgtk_widget_t *inner = leaf(13, 200, 30), *c = leaf(14, 10, 10);
    adopt(a, inner);
    adopt(b, leaf(15, 10, 10));
    adopt(box, a); adopt(box, b); adopt(box, c);
    mulib_retgtk_widget_arrange(box, (mgx_rect){0, 0, 100, 40});
    assert(a->bounds.w == 20 && b->bounds.w == 30 && c->bounds.w == 50);
    assert(a->bounds.h == 8 && inner->bounds.w == 20 && inner->bounds.h == 8);
    mulib_retgtk_widget_arrange(box, (mgx_rect){0, 0, 5, 40});
    assert(a->bounds.w == 20 && b->bounds.w == 10 && c->bounds.w == 0);
    assert(b->bounds.x == 20); /* Minima overflow, rather than become negative. */
    assert(!mulib_retgtk_constraint_box(99, (mulib_gui_constraints_t){2, 0, 1, 0}));
    mulib_retgtk_widget_unref(box);

    box = mulib_retgtk_box(20, 0, 0, 0);
    a = leaf(21, 0, 1); b = leaf(22, 0, 1); c = leaf(23, 0, 1);
    adopt(box, a); adopt(box, b); adopt(box, c);
    mulib_retgtk_widget_arrange(box, (mgx_rect){0, 0, 100, 10});
    assert(a->bounds.w + b->bounds.w + c->bounds.w == 100);
    assert(c->bounds.x + c->bounds.w == 100);
    mgx_rect saved = a->bounds;
    fail_after = 0;
    mulib_retgtk_widget_arrange(box, (mgx_rect){0, 0, 30, 10});
    assert(!memcmp(&a->bounds, &saved, sizeof saved));
    a->weight = DBL_MAX; b->weight = DBL_MIN; c->weight = NAN;
    mulib_retgtk_widget_arrange(box, (mgx_rect){INT_MAX - 2, INT_MAX - 3, INT_MAX, 10});
    assert(a->bounds.w >= 0 && b->bounds.w >= 0 && c->bounds.w >= 0);
    mulib_retgtk_widget_unref(box);

    box = mulib_retgtk_box(30, 1, INT_MAX, INT_MAX);
    adopt(box, leaf(31, INT_MAX, INT_MAX));
    adopt(box, leaf(32, INT_MAX, INT_MAX));
    size = mulib_retgtk_widget_measure(box,
        (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX});
    assert(size.w == INT_MAX && size.h == INT_MAX);
    mulib_retgtk_widget_arrange(box,
        (mgx_rect){INT_MAX - 2, INT_MAX - 3, INT_MAX, INT_MAX});
    mulib_retgtk_widget_unref(box);

    box = mulib_retgtk_grid(40, 2, 1);
    a = leaf(41, 20, 10); b = leaf(42, 30, 15); c = leaf(43, 10, 8);
    hidden = leaf(44, 999, 999); hidden->visible = 0;
    adopt(box, a); adopt(box, hidden); adopt(box, b); adopt(box, c);
    measures = 0;
    size = mulib_retgtk_widget_measure(box,
        (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX});
    assert(size.w == 64 && size.h == 34 && measures == 3);
    mulib_retgtk_widget_arrange(box, (mgx_rect){0, 0, 100, 60});
    assert(a->bounds.x == 1 && a->bounds.y == 1 && a->bounds.w == 48);
    assert(b->bounds.x == 51 && b->bounds.y == 1 && b->bounds.h == 28);
    assert(c->bounds.x == 1 && c->bounds.y == 31);
    a->visible = b->visible = c->visible = 0;
    size = mulib_retgtk_widget_measure(box,
        (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX});
    assert(!size.w && !size.h);
    mulib_retgtk_widget_unref(box);
}

static uint32_t random_state = 0x324f8791u;
static unsigned next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static void test_layout_distribution(void)
{
    for (unsigned run = 0; run < 1500; ++run) {
        unsigned n = 2 + next_random() % 8;
        int available = (int)(next_random() % 1200), min_sum = 0, max_sum = 0;
        mulib_retgtk_widget_t *box = mulib_retgtk_box(1, run % 2, 0, 0);
        assert(box);
        for (unsigned i = 0; i < n; ++i) {
            int minimum = (int)(next_random() % 40);
            int maximum = minimum + (int)(next_random() % 200);
            int natural = (int)(next_random() % 200);
            mulib_gui_constraints_t limits = run % 2 ?
                (mulib_gui_constraints_t){0, minimum, INT_MAX, maximum} :
                (mulib_gui_constraints_t){minimum, 0, maximum, INT_MAX};
            mulib_retgtk_widget_t *slot = mulib_retgtk_constraint_box(i + 2, limits);
            assert(slot);
            slot->weight = 1 + next_random() % 16;
            adopt(slot, leaf(i + 20, natural, natural));
            adopt(box, slot);
            min_sum += minimum;
            max_sum += maximum;
        }
        mulib_retgtk_widget_arrange(box, run % 2 ?
            (mgx_rect){0, 0, 100, available} :
            (mgx_rect){0, 0, available, 100});
        int sum = 0;
        for (size_t i = 0; i < box->n; ++i) {
            mulib_retgtk_widget_t *slot = box->children[i];
            mulib_gui_constraints_t limits = mulib_retgtk_layout_constraints(slot);
            int extent = run % 2 ? slot->bounds.h : slot->bounds.w;
            assert(extent >= (run % 2 ? limits.min_h : limits.min_w));
            assert(extent <= (run % 2 ? limits.max_h : limits.max_w));
            assert(sum == (run % 2 ? slot->bounds.y : slot->bounds.x));
            sum += extent;
        }
        int target = mgx_max(min_sum, mgx_min(max_sum, available));
        assert(sum == target);
        mulib_retgtk_widget_unref(box);
    }
}

typedef struct {
    mulib_retgtk_widget_t *parent, *victim, *hidden, *added;
    int phase, fired;
    unsigned destroyed;
} layout_mutation;
static void mutate_layout(layout_mutation *s)
{
    if (s->fired) return;
    s->fired = 1;
    assert(mulib_retgtk_widget_remove(s->parent, s->victim));
    mulib_retgtk_widget_set_visible(s->hidden, 1);
    s->added = leaf(99, 7, 7);
    adopt(s->parent, s->added);
}
static void mutation_measure(mulib_retgtk_widget_t *w, mulib_gui_constraints_t c,
                             mulib_gui_size_t *out)
{
    (void)c;
    layout_mutation *s = w->data;
    if (s->phase == 1) mutate_layout(s);
    *out = w->desired;
}
static void mutation_arrange(mulib_retgtk_widget_t *w, mgx_rect r)
{
    (void)r;
    layout_mutation *s = w->data;
    if (s->phase == 2) mutate_layout(s);
}
static void mutation_destroy(mulib_retgtk_widget_t *w)
{
    ++((layout_mutation *)w->data)->destroyed;
}
static const mulib_retgtk_widget_ops mutation_ops = {
    NULL, mutation_measure, mutation_arrange, NULL, NULL
};
static const mulib_retgtk_widget_ops victim_ops = {
    mutation_destroy, leaf_measure, NULL, NULL, NULL
};
static void test_layout_mutation(void)
{
    for (int container = 0; container < 3; ++container)
        for (int phase = 1; phase <= 2; ++phase) {
            layout_mutation s = {0};
            s.phase = phase;
            if (!container) s.parent = mulib_retgtk_box(1, 0, 0, 0);
            else if (container == 1) s.parent = mulib_retgtk_grid(1, 2, 0);
            else s.parent = mulib_retgtk_constraint_box(1,
                (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX});
            assert(s.parent);
            mulib_retgtk_widget_t *mutator =
                mulib_retgtk_widget_create(2, &mutation_ops);
            s.victim = mulib_retgtk_widget_create(3, &victim_ops);
            assert(mutator && s.victim);
            mutator->data = s.victim->data = &s;
            mutator->desired = s.victim->desired = (mulib_gui_size_t){10, 10};
            s.hidden = leaf(4, 10, 10);
            mulib_retgtk_widget_set_visible(s.hidden, 0);
            adopt(s.parent, mutator); adopt(s.parent, s.victim); adopt(s.parent, s.hidden);
            if (phase == 1)
                (void)mulib_retgtk_widget_measure(s.parent,
                    (mulib_gui_constraints_t){0, 0, INT_MAX, INT_MAX});
            else
                mulib_retgtk_widget_arrange(s.parent, (mgx_rect){0, 0, 100, 100});
            assert(s.fired && s.destroyed == 1 && s.parent->n == 3);
            assert(s.added && s.added->parent == s.parent);
            assert(!s.added->bounds.w && !s.added->bounds.h);
            mulib_retgtk_widget_arrange(s.parent, (mgx_rect){0, 0, 100, 100});
            assert(s.added->bounds.w > 0 && s.added->bounds.h > 0);
            mulib_retgtk_widget_unref(s.parent);
        }
}

typedef struct { char (*keys)[12]; size_t n, reads; } sort_source;
static size_t source_rows(void *u) { return ((sort_source *)u)->n; }
static size_t source_cols(void *u) { (void)u; return 1; }
static const char *source_cell(void *u, size_t r, size_t c)
{
    sort_source *s = u;
    assert(r < s->n && c == 0);
    ++s->reads;
    return s->keys[r];
}
static void assert_sorted(const mulib_retgtk_proxymodel_t *p, sort_source *s,
                          int ascending)
{
    for (size_t i = 1; i < p->n; ++i) {
        size_t a = p->indices[i - 1], b = p->indices[i];
        int cmp = strcmp(s->keys[a], s->keys[b]);
        assert(ascending ? cmp <= 0 : cmp >= 0);
        if (!cmp) assert(a < b);
    }
}
static unsigned paints;
static void record_paint(void *u, mret_paint *p, mgx_rect r, const char *s, int on)
{
    (void)u; (void)p; (void)on;
    assert(s && r.w >= 0 && r.h >= 0);
    ++paints;
}
static const char *null_cell(void *u, size_t r, size_t c)
{
    (void)u; (void)r; (void)c;
    return NULL;
}
typedef struct { size_t rows, cols, calls, last_column; } wide_source;
static size_t wide_rows(void *u) { return ((wide_source *)u)->rows; }
static size_t wide_cols(void *u) { return ((wide_source *)u)->cols; }
static const char *wide_cell(void *u, size_t r, size_t c)
{
    wide_source *s = u;
    assert(r < s->rows && c < s->cols);
    if (s->calls) assert(c > s->last_column);
    s->last_column = c;
    ++s->calls;
    return "wide";
}

static void test_models(void)
{
    sort_source s = {0};
    s.n = 16384;
    s.keys = calloc(s.n, sizeof *s.keys);
    assert(s.keys);
    for (size_t i = 0; i < s.n; ++i)
        snprintf(s.keys[i], sizeof s.keys[i], "%04u", (unsigned)((s.n - i) % 257));
    mulib_retgtk_model_t model = {&s, source_rows, source_cols, source_cell, NULL, 0};
    mulib_retgtk_proxymodel_t p = {0};
    assert(mulib_retgtk_proxymodel_build(&p, &model, 0, NULL, 1));
    assert(p.n == s.n && s.reads == s.n);
    assert_sorted(&p, &s, 1);
    s.reads = 0;
    assert(mulib_retgtk_proxymodel_build(&p, &model, 0, NULL, 0));
    assert(s.reads == s.n);
    assert_sorted(&p, &s, 0);
    for (long failure = 0; failure < 3; ++failure) {
        size_t *old = p.indices;
        fail_after = failure;
        assert(!mulib_retgtk_proxymodel_build(&p, &model, 0, NULL, 1));
        assert(p.indices == old && p.n == s.n);
    }
    assert(mulib_retgtk_proxymodel_build(&p, &model, 0, "001", 1));
    for (size_t i = 0; i < p.n; ++i) assert(strstr(s.keys[p.indices[i]], "001"));
    assert(!mulib_retgtk_proxymodel_cell(&p, SIZE_MAX, 0));
    assert(!mulib_retgtk_proxymodel_cell(&p, 0, SIZE_MAX));
    assert(!mulib_retgtk_proxymodel_cell(NULL, 0, 0));
    mulib_retgtk_model_t proxy = mulib_retgtk_proxymodel_interface(&p);
    assert(proxy.rows(proxy.user) == p.n);
    ++model.revision;
    assert(!mulib_retgtk_proxymodel_cell(&p, 0, 0));
    assert(mulib_retgtk_proxymodel_source_row(&p, 0) == SIZE_MAX);
    assert(proxy.rows(proxy.user) == 0);
    assert(mulib_retgtk_proxymodel_build(&p, &model, 0, NULL, 1));
    --s.n;
    assert(!mulib_retgtk_proxymodel_current(&p));
    s.n = 0;
    assert(mulib_retgtk_proxymodel_build(&p, &model, 0, NULL, 1) && !p.n);
    mulib_retgtk_proxymodel_free(&p);
    free(s.keys);

    mulib_retgtk_listmodel_t list = {0};
    for (unsigned i = 0; i < 10; ++i) assert(mulib_retgtk_listmodel_append(&list, "row"));
    model = mulib_retgtk_listmodel_interface(&list);
    assert(mulib_retgtk_proxymodel_build(&p, &model, 0, NULL, 1));
    assert(mulib_retgtk_model_set(&model, 3, 0, "changed") && model.revision == 1);
    assert(!mulib_retgtk_proxymodel_current(&p));
    assert(mulib_retgtk_proxymodel_build(&p, &model, 0, NULL, 1));
    assert(mulib_retgtk_proxymodel_source_row(&p, 0) == 3);
    mulib_retgtk_proxymodel_free(&p);
    assert(!strcmp(mulib_retgtk_model_cell(&model, 3, 0), "changed"));
    fail_after = 0;
    assert(!mulib_retgtk_model_set(&model, 3, 0, "allocation failure"));
    assert(!strcmp(mulib_retgtk_model_cell(&model, 3, 0), "changed"));
    assert(!mulib_retgtk_model_set(&model, SIZE_MAX, 0, "bad"));
    assert(!mulib_retgtk_model_set(&model, 0, 1, "bad"));
    assert(!mulib_retgtk_model_set(&model, 0, 0, NULL));
    assert(!mulib_retgtk_model_set(NULL, 0, 0, "bad"));
    assert(model.revision == 1);
    mulib_retgtk_model_t nullable = model;
    nullable.cell = null_cell;
    assert(mulib_retgtk_model_set(&nullable, 4, 0, "was null"));
    mulib_retgtk_widget_t *view = mulib_retgtk_list(100, &model);
    assert(view);
    mret_list_view *v = view->data;
    v->delegate.paint = record_paint;
    view->bounds = (mgx_rect){0, 0, 100, 49};
    paints = 0;
    mret_list_paint(view, NULL);
    assert(paints == 3); /* Includes a clipped partial last row. */
    v->first = SIZE_MAX; v->selected = 9;
    while (list.n > 2) assert(mulib_retgtk_listmodel_remove(&list, list.n - 1));
    assert(mret_list_sync(view) == 2 && !v->first && v->selected == SIZE_MAX);
    mgx_event e = {0};
    e.type = MGX_EVENT_BUTTON; e.x = 2; e.y = 25;
    assert(mret_list_event(view, &e) && v->selected == 1);
    e.code = 1;
    assert(!mret_list_event(view, &e));
    view->enabled = 0; e.code = 0;
    assert(!mret_list_event(view, &e));
    view->enabled = 1;
    e.type = MGX_EVENT_SCROLL; e.dy = NAN;
    assert(!mret_list_event(view, &e));
    e.dy = -DBL_MAX;
    assert(mret_list_event(view, &e) && v->first == 0);
    v->row_h = 0;
    assert(!mret_list_event(view, &e));
    mret_list_paint(view, NULL);
    mulib_retgtk_widget_unref(view);

    view = mulib_retgtk_tableview(101, &model);
    assert(view);
    ((mret_list_view *)view->data)->delegate.paint = record_paint;
    view->bounds = (mgx_rect){0, 0, 100, 49};
    paints = 0;
    mret_tableview_paint(view, NULL);
    assert(paints == 2);
    view->bounds.w = -1;
    mret_tableview_paint(view, NULL);
    mulib_retgtk_widget_unref(view);
    mulib_retgtk_listmodel_free(&list);

    wide_source wide = {1, SIZE_MAX, 0, 0};
    model = (mulib_retgtk_model_t){&wide, wide_rows, wide_cols, wide_cell, NULL, 0};
    view = mulib_retgtk_tableview(102, &model);
    assert(view);
    ((mret_list_view *)view->data)->delegate.paint = record_paint;
    view->bounds = (mgx_rect){0, 0, 7, 24};
    mret_tableview_paint(view, NULL);
    assert(wide.calls == 7 && wide.last_column == SIZE_MAX - 1);
    mulib_retgtk_widget_unref(view);

    mulib_retgtk_tablemodel_t table = {0};
    assert(mulib_retgtk_tablemodel_init(&table, 2, 2));
    assert(!mulib_retgtk_tablemodel_init(&table, 1, 1));
    assert(!mulib_retgtk_tablemodel_init(NULL, 1, 1));
    model = mulib_retgtk_tablemodel_interface(&table);
    assert(mulib_retgtk_model_set(&model, 1, 1, "cell"));
    assert(!strcmp(mulib_retgtk_model_cell(&model, 1, 1), "cell"));
    assert(!mulib_retgtk_model_set(&model, 2, 0, "bad"));
    assert(!mulib_retgtk_tablemodel_init(NULL, 0, 0));
    mulib_retgtk_tablemodel_free(&table);

    mulib_retgtk_treemodel_t tree = {0};
    assert(mulib_retgtk_treemodel_add(&tree, SIZE_MAX, "root"));
    assert(mulib_retgtk_treemodel_add(&tree, 0, "child"));
    assert(!mulib_retgtk_treemodel_add(&tree, 99, "invalid"));
    unsigned depth = 99;
    assert(!mulib_retgtk_treemodel_visible(&tree, 1, &depth));
    tree.v[0].expanded = 1;
    assert(mulib_retgtk_treemodel_visible(&tree, 1, &depth) && depth == 1);
    tree.v[1].parent = 1;
    assert(!mulib_retgtk_treemodel_visible(&tree, 1, &depth));
    tree.v[1].parent = 200;
    assert(!mulib_retgtk_treemodel_visible(&tree, 1, &depth));
    mulib_retgtk_treemodel_free(&tree);
}

typedef struct { unsigned made, destroyed; uint64_t fail_id; } factory_state;
static void tracked_destroy(mulib_retgtk_widget_t *w)
{
    ++((factory_state *)w->data)->destroyed;
}
static const mulib_retgtk_widget_ops tracked_ops = {
    tracked_destroy, NULL, NULL, NULL, NULL
};
static mulib_retgtk_widget_t *tracked_factory(uint64_t id, const char *text, void *u)
{
    factory_state *state = u;
    if (id == state->fail_id) return NULL;
    mulib_retgtk_widget_t *w = mulib_retgtk_widget_create(id, &tracked_ops);
    if (!w) return NULL;
    ++state->made;
    w->data = state;
    w->kind = MRET_KIND_USER;
    if (text && !mulib_retgtk_widget_text(w, text)) {
        mulib_retgtk_widget_unref(w);
        return NULL;
    }
    return w;
}
static mulib_retgtk_widget_t *box_factory(uint64_t id, const char *text, void *u)
{
    (void)text; (void)u;
    return mulib_retgtk_box(id, 1, 2, 2);
}
static mulib_retgtk_widget_t *entry_factory(uint64_t id, const char *text, void *u)
{
    (void)u;
    return mulib_retgtk_entry(id, text);
}
static mulib_retgtk_widget_t *toggle_factory(uint64_t id, const char *text, void *u)
{
    (void)u;
    return mulib_retgtk_toggle(id, text, 0);
}
static const mret_builder_factory reload_factories[] = {
    {"box", box_factory, NULL}, {"entry", entry_factory, NULL},
    {"toggle", toggle_factory, NULL}
};
static const char *reload_markup =
    "<box id='1'><entry id='2' text='document'/><toggle id='3'/></box>";

static void test_documents(void)
{
    mulib_retgtk_uiml_t u = {0};
    assert(mulib_retgtk_uiml_parse(&u, "<n id='1'><n id='2' text='child'/></n>"));
    mret_builder_node *original = u.v;
    const char *bad[] = {
        "", "<", "<n", "<n ", "<n x=", "<n id='", "<n text='&amp;'/>",
        "<n id='0'/>", "<n id='-1'/>", "<n id=' 1'/>",
        "<n id='18446744073709551616'/>", "<n id='1'id='2'/>",
        "<n text='a'text='b'/>", "<n text='a' text='b'/>",
        "<n id='1' id='2'/>", "<n><n id='1'/></n>", "<n></m>",
        "<n/><n/>", "<n>text</n>", "<n text='bad\001'/>", "<n/>trailing"
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        assert(!mulib_retgtk_uiml_parse(&u, bad[i]));
        assert(u.v == original && u.n == 2 && u.v[1].id == 2);
    }
    char truncated[] = "<n id='8'><n text='child'/></n>";
    size_t length = strlen(truncated);
    for (size_t i = 0; i < length; ++i) {
        char saved = truncated[i]; truncated[i] = 0;
        assert(!mulib_retgtk_uiml_parse(&u, truncated));
        truncated[i] = saved;
    }
    const char mutations[] = "<>/\"'=& \001";
    for (size_t i = 0; i < length; ++i)
        for (size_t j = 0; j < sizeof mutations - 1; ++j) {
            char saved = truncated[i]; truncated[i] = mutations[j];
            mulib_retgtk_uiml_t fuzz = {0};
            (void)mulib_retgtk_uiml_parse(&fuzz, truncated);
            mulib_retgtk_uiml_free(&fuzz);
            truncated[i] = saved;
        }
    char deep[129 * 7 + 1];
    char *end = deep;
    for (unsigned i = 0; i < 129; ++i) { memcpy(end, "<n>", 3); end += 3; }
    for (unsigned i = 0; i < 129; ++i) { memcpy(end, "</n>", 4); end += 4; }
    *end = 0;
    assert(!mulib_retgtk_uiml_parse(&u, deep) && u.v == original);
    char *large = malloc(MULIB_RETGTK_UIML_MAX_BYTES + 2);
    assert(large);
    memset(large, ' ', MULIB_RETGTK_UIML_MAX_BYTES + 1);
    large[MULIB_RETGTK_UIML_MAX_BYTES + 1] = 0;
    assert(!mulib_retgtk_uiml_parse(&u, large));
    free(large);
    /* The bounded scan must not read a sentinel beyond its byte budget. */
    large = malloc(MULIB_RETGTK_UIML_MAX_BYTES);
    assert(large);
    memset(large, ' ', MULIB_RETGTK_UIML_MAX_BYTES);
    assert(!mulib_retgtk_uiml_parse(&u, large) && u.v == original);
    free(large);
    assert(mulib_retgtk_uiml_parse(&u, "<n id='18446744073709551615'/>"));
    assert(u.n == 1 && u.v[0].id == UINT64_MAX);
    mulib_retgtk_uiml_free(&u);

    factory_state state = {0};
    mret_builder_factory f = {"n", tracked_factory, &state};
    mret_builder_node nodes[] = {
        {"n", "root", 1, SIZE_MAX}, {"n", "branch", 2, 0},
        {"n", "leaf", 3, 1}, {"n", "sibling", 4, 0}
    };
    mulib_retgtk_widget_t *root = mulib_retgtk_builder_build(&f, 1, nodes, 4);
    assert(root && state.made == 4 && root->object.refs == 1);
    mulib_retgtk_widget_t *held = root->children[0]->children[0];
    assert(held->object.refs == 1 && mulib_retgtk_object_ref(&held->object));
    mulib_retgtk_widget_unref(root);
    assert(state.destroyed == 3 && held->parent == NULL && held->object.refs == 1);
    mulib_retgtk_widget_unref(held);
    assert(state.destroyed == 4);
    state = (factory_state){0, 0, 3};
    assert(!mulib_retgtk_builder_build(&f, 1, nodes, 4));
    assert(state.made == 2 && state.destroyed == 2);
    state = (factory_state){0};
    nodes[3].id = 2;
    assert(!mulib_retgtk_builder_build(&f, 1, nodes, 4) && !state.made);
    nodes[3].id = 4; nodes[2].parent = 3;
    assert(!mulib_retgtk_builder_build(&f, 1, nodes, 4) && !state.made);
    nodes[2].parent = 1; nodes[3].tag = "unknown";
    assert(!mulib_retgtk_builder_build(&f, 1, nodes, 4) && !state.made);
    nodes[3].tag = "n";
    allocations = 0;
    root = mulib_retgtk_builder_build(&f, 1, nodes, 4);
    assert(root);
    size_t total = allocations;
    mulib_retgtk_widget_unref(root);
    for (size_t i = 0; i < total; ++i) {
        state = (factory_state){0};
        fail_after = (long)i;
        assert(!mulib_retgtk_builder_build(&f, 1, nodes, 4));
        assert(state.made == state.destroyed);
    }

    assert(mulib_retgtk_uiml_parse(&u, reload_markup));
    allocations = 0;
    assert(mulib_retgtk_uiml_parse(&u, reload_markup));
    total = allocations;
    original = u.v;
    for (size_t i = 0; i < total; ++i) {
        fail_after = (long)i;
        assert(!mulib_retgtk_uiml_parse(&u, reload_markup));
        assert(u.v == original && u.n == 3);
    }
    mulib_retgtk_uiml_free(&u);
}

static void test_reload(void)
{
    mulib_retgtk_widget_t *old = gfx_retgtk_peer_toggle(9);
    mulib_retgtk_widget_t *fresh = mulib_retgtk_toggle(9, "local", 0);
    assert(old && fresh);
    assert(old->ops != fresh->ops); /* Distinct TU-local tables, same type. */
    old->visible = 0;
    assert(mulib_retgtk_hotreload_transfer_checked(old, fresh));
    assert(fresh->value == 1 && !fresh->visible);
    mulib_retgtk_widget_unref(old); mulib_retgtk_widget_unref(fresh);
    old = gfx_retgtk_peer_entry(10);
    fresh = mulib_retgtk_entry(10, "local");
    assert(old && fresh);
    assert(mulib_retgtk_textview_editor(old) == old->data);
    old->focused = 1;
    mgx_event disabled_text = {0};
    disabled_text.type = MGX_EVENT_TEXT;
    memcpy(disabled_text.text, "x", 2);
    mulib_retgtk_widget_set_enabled(old, 0);
    assert(!mret_entry_event(old, &disabled_text));
    mulib_retgtk_widget_set_enabled(old, 1);
    ((mulib_texteditor_t *)old->data)->selection = (mulib_textselection_t){1, 4};
    assert(mulib_retgtk_hotreload_transfer_checked(old, fresh));
    assert(!strcmp(fresh->text, "edited in peer"));
    assert(((mulib_texteditor_t *)fresh->data)->selection.caret == 4);
    mulib_retgtk_widget_unref(fresh);
    fresh = mulib_retgtk_label(10, "changed type");
    assert(mulib_retgtk_hotreload_transfer_checked(old, fresh));
    assert(!strcmp(fresh->text, "changed type"));
    mulib_retgtk_widget_unref(old); mulib_retgtk_widget_unref(fresh);

    mulib_retgtk_scene_t scene = {0};
    assert(mulib_retgtk_hotreload(&scene, reload_factories, 3, reload_markup));
    old = scene.root;
    uint64_t generation = scene.generation;
    assert(!mulib_retgtk_hotreload(&scene, reload_factories, 3, "<box>"));
    assert(scene.root == old && scene.generation == generation);
    assert(!mulib_retgtk_hotreload(&scene, reload_factories, 3, "<unknown/>"));
    assert(scene.root == old && scene.generation == generation);
    mulib_retgtk_tree_find(old, 3)->value = 1;
    mulib_retgtk_widget_t *entry = mulib_retgtk_tree_find(old, 2);
    assert(mulib_texteditor_insert(entry->data, " edited", 7));
    allocations = 0;
    assert(mulib_retgtk_hotreload(&scene, reload_factories, 3, reload_markup));
    size_t total = allocations;
    assert(scene.generation == generation + 1 && scene.root != old);
    assert(mulib_retgtk_tree_find(scene.root, 3)->value == 1);
    assert(!strcmp(mulib_retgtk_tree_find(scene.root, 2)->text, "document edited"));
    for (size_t i = 0; i < total; ++i) {
        old = scene.root; generation = scene.generation;
        fail_after = (long)i;
        assert(!mulib_retgtk_hotreload(&scene, reload_factories, 3, reload_markup));
        assert(scene.root == old && scene.generation == generation);
        assert(!strcmp(mulib_retgtk_tree_find(old, 2)->text, "document edited"));
    }
    mulib_retgtk_scene_set(&scene, NULL);
    assert(!scene.root);
}

static void test_headless_paint(void)
{
    mgx_surface surface = {0};
    mulib_glx_font_t font = {0};
    assert(mgx_surface_init(&surface, 240, 120));
    assert(mulib_glx_font_builtin(&font));
    mulib_retgtk_widget_t *root = mulib_retgtk_box(1, 1, 4, 4);
    assert(root);
    adopt(root, mulib_retgtk_button(2, "Apply"));
    adopt(root, mulib_retgtk_toggle(3, "Enabled", 1));
    adopt(root, mulib_retgtk_entry(4, "RetGTK"));
    mret_paint paint = {&surface, &font, mgui_default_theme()};
    mgx_clear(&surface, paint.theme.background);
    mulib_retgtk_widget_arrange(root, (mgx_rect){0, 0, 240, 120});
    mulib_retgtk_widget_paint(root, &paint);
    size_t changed = 0, text = 0;
    for (int y = 0; y < surface.h; ++y)
        for (int x = 0; x < surface.w; ++x) {
            uint32_t pixel = surface.pixels[(size_t)y * surface.stride + (size_t)x];
            changed += pixel != paint.theme.background;
            text += pixel == paint.theme.text;
        }
    assert(changed > 1000 && text > 0);
    assert(!root->dirty);
    mulib_retgtk_widget_unref(root);
    mulib_glx_font_free(&font);
    mgx_surface_free(&surface);
}

int main(void)
{
    test_layout();
    test_layout_distribution();
    test_layout_mutation();
    test_models();
    test_documents();
    test_reload();
    test_headless_paint();
    puts("RetGTK model/layout/builder regressions passed");
    return 0;
}
