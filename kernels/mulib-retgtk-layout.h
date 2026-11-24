/* MuLIB graphical extension: retgtk-layout. C11, header-only. */
#ifndef MULIB_RETGTK_LAYOUT_H
#define MULIB_RETGTK_LAYOUT_H
#include "mulib-retgtk-constraint.h"

static inline int mret_layout_int(long double x)
{
    return x >= INT_MAX ? INT_MAX : x <= INT_MIN ? INT_MIN : (int)x;
}

static inline mgx_rect mret_layout_inset(mgx_rect r, int padding)
{
    r.w = mgx_max(0, r.w);
    r.h = mgx_max(0, r.h);
    int x = mgx_min(mgx_max(0, padding), r.w / 2);
    int y = mgx_min(mgx_max(0, padding), r.h / 2);
    r.x = mret_layout_int((long double)r.x + x);
    r.y = mret_layout_int((long double)r.y + y);
    r.w -= 2 * x;
    r.h -= 2 * y;
    return r;
}

static inline void mulib_retgtk_layout_linear_measure(
    mulib_retgtk_widget_t *w, mulib_gui_constraints_t c,
    mulib_gui_size_t *out, int vertical, int gap, int padding)
{
    gap = mgx_max(0, gap);
    padding = mgx_max(0, padding);
    c = mret_size_limits(c);
    mulib_gui_constraints_t child = {0, 0, INT_MAX, INT_MAX};
    if (vertical)
        child.max_w = mret_layout_int(fmaxl(0, (long double)c.max_w - 2.L * padding));
    else
        child.max_h = mret_layout_int(fmaxl(0, (long double)c.max_h - 2.L * padding));
    long double main = 0;
    int cross = 0;
    size_t count = 0, n = 0;
    mulib_retgtk_widget_t **children =
        mulib_retgtk_widget_children_snapshot(w, &n);
    if (w->n && !children) mulib_retgtk_widget_layout_retry(w);
    for (size_t i = 0; i < n; ++i) {
        if (children[i]->parent != w || !children[i]->visible) continue;
        mulib_gui_size_t s = mulib_retgtk_widget_measure(children[i], child);
        if (children[i]->parent != w || !children[i]->visible) continue;
        main += vertical ? s.h : s.w;
        cross = mgx_max(cross, vertical ? s.w : s.h);
        ++count;
    }
    mulib_retgtk_widget_children_release(children, n);
    if (count) main += (long double)(count - 1) * gap;
    int a = mret_layout_int(main + 2.L * padding);
    int b = mret_layout_int((long double)cross + 2.L * padding);
    *out = vertical ? (mulib_gui_size_t){b, a} : (mulib_gui_size_t){a, b};
}

typedef struct {
    mulib_retgtk_widget_t *child;
    int minimum, maximum;
    long double extent, weight;
} mret_layout_slot;

/* Intrinsic basis + weighted surplus; shrink proportionally to each child's
 * room above its minimum. Capped grow weights are redistributed. A zero grow
 * weight keeps the intrinsic basis, rather than making the child disappear.
 * Layout is UI-thread-only. If scratch allocation fails, child bounds stay put. */
static inline void mulib_retgtk_layout_linear(mulib_retgtk_widget_t *w,
    mgx_rect r, int vertical, int gap, int padding)
{
    if (!w) return;
    size_t snapshot_n = 0;
    mulib_retgtk_widget_t **children =
        mulib_retgtk_widget_children_snapshot(w, &snapshot_n);
    if (w->n && !children) mulib_retgtk_widget_layout_retry(w);
    size_t n = 0;
    for (size_t i = 0; i < snapshot_n; ++i) n += children[i]->visible != 0;
    if (!n || snapshot_n > SIZE_MAX / sizeof(mret_layout_slot)) {
        mulib_retgtk_widget_children_release(children, snapshot_n);
        return;
    }
    mret_layout_slot *v = malloc(snapshot_n * sizeof *v);
    if (!v) {
        mulib_retgtk_widget_layout_retry(w);
        mulib_retgtk_widget_children_release(children, snapshot_n);
        return;
    }
    r = mret_layout_inset(r, padding);
    gap = mgx_max(0, gap);
    long double basis = 0, minimum = 0;
    mulib_gui_constraints_t c = {0, 0, vertical ? r.w : INT_MAX,
                                         vertical ? INT_MAX : r.h};
    size_t k = 0;
    for (size_t i = 0; i < snapshot_n; ++i) {
        mulib_retgtk_widget_t *child = children[i];
        if (child->parent != w || !child->visible) continue;
        mulib_gui_constraints_t limits = mulib_retgtk_layout_constraints(child);
        mulib_gui_size_t s = mulib_retgtk_widget_measure(child, c);
        if (child->parent != w || !child->visible) continue;
        int lo = vertical ? limits.min_h : limits.min_w;
        int hi = vertical ? limits.max_h : limits.max_w;
        int natural = mgx_max(lo, mgx_min(hi, vertical ? s.h : s.w));
        v[k++] = (mret_layout_slot){child, lo, hi, natural,
            isfinite(child->weight) && child->weight > 0 ? child->weight : 0};
        basis += natural;
        minimum += lo;
    }
    n = k;
    if (!n) {
        free(v);
        mulib_retgtk_widget_children_release(children, snapshot_n);
        return;
    }
    long double available = fmaxl(0, (vertical ? r.h : r.w) -
                                     (long double)(n - 1) * gap);
    if (basis > available) {
        long double room = basis - minimum;
        long double factor = room > 0 ? fmaxl(0, available - minimum) / room : 0;
        for (size_t i = 0; i < n; ++i)
            v[i].extent = v[i].minimum +
                          (v[i].extent - v[i].minimum) * factor;
    } else {
        long double extra = available - basis;
        while (extra > 0) {
            long double weights = 0, largest = 0;
            for (size_t i = 0; i < n; ++i)
                if (v[i].extent < v[i].maximum)
                    largest = fmaxl(largest, v[i].weight);
            if (!largest) break;
            for (size_t i = 0; i < n; ++i)
                if (v[i].extent < v[i].maximum)
                    weights += v[i].weight / largest;
            int capped = 0;
            long double budget = extra;
            for (size_t i = 0; i < n; ++i) {
                if (!v[i].weight || v[i].extent >= v[i].maximum) continue;
                long double share = budget * (v[i].weight / largest) / weights;
                long double room = v[i].maximum - v[i].extent;
                if (room <= share) {
                    v[i].extent = v[i].maximum;
                    extra = fmaxl(0, extra - room);
                    capped = 1;
                }
            }
            if (capped) continue;
            for (size_t i = 0; i < n; ++i)
                if (v[i].extent < v[i].maximum)
                    v[i].extent += extra * (v[i].weight / largest) / weights;
            break;
        }
    }
    long double offset = 0, fraction = 0;
    for (size_t i = 0; i < n; ++i) {
        long double rounded = floorl(v[i].extent + fraction + 1e-12L);
        int extent = mgx_max(v[i].minimum,
                            mgx_min(v[i].maximum, mret_layout_int(rounded)));
        fraction += v[i].extent - extent;
        mgx_rect b = r;
        if (vertical) {
            b.y = mret_layout_int((long double)r.y + offset);
            b.h = extent;
            mulib_gui_constraints_t limits =
                mulib_retgtk_layout_constraints(v[i].child);
            b.w = mgx_max(limits.min_w, mgx_min(limits.max_w, b.w));
        } else {
            b.x = mret_layout_int((long double)r.x + offset);
            b.w = extent;
            mulib_gui_constraints_t limits =
                mulib_retgtk_layout_constraints(v[i].child);
            b.h = mgx_max(limits.min_h, mgx_min(limits.max_h, b.h));
        }
        if (v[i].child->parent == w && v[i].child->visible)
            mulib_retgtk_widget_arrange(v[i].child, b);
        offset += (long double)extent + gap;
    }
    free(v);
    mulib_retgtk_widget_children_release(children, snapshot_n);
}
#endif
