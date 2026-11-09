/** @file mulib-linebreak.h
 * @brief Seven UTF-8 wrapping strategies, including Knuth-Plass and Unicode.
 * Define MULIB_IMPL_LINEBREAK, UNIBREAK, UNIPROC, DYNLOAD and UTF8 once.
 * Only the UNICODE strategy requires an ICU runtime. See the manual for the
 * additive measurement model, whitespace policy, limits and ownership.
 */
#ifndef MULIB_LINEBREAK_H
#define MULIB_LINEBREAK_H
#include "mulib-unibreak.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_linebreak_algorithm {
    MULIB_LINEBREAK_GREEDY, MULIB_LINEBREAK_CHARACTER,
    MULIB_LINEBREAK_MINIMUM_RAGGEDNESS, MULIB_LINEBREAK_BALANCED,
    MULIB_LINEBREAK_LOOKAHEAD, MULIB_LINEBREAK_KNUTH_PLASS,
    MULIB_LINEBREAK_UNICODE
} mulib_linebreak_algorithm_t;
typedef enum mulib_linebreak_status {
    MULIB_LINEBREAK_OK = 0, MULIB_LINEBREAK_INVALID = -1,
    MULIB_LINEBREAK_MEMORY = -2, MULIB_LINEBREAK_LIMIT = -3,
    MULIB_LINEBREAK_BACKEND = -4, MULIB_LINEBREAK_INFEASIBLE = -5
} mulib_linebreak_status_t;
typedef enum mulib_linebreak_flags {
    MULIB_LINEBREAK_HARD = 1, MULIB_LINEBREAK_HYPHEN = 2,
    MULIB_LINEBREAK_OVERFULL = 4
} mulib_linebreak_flags_t;
/** @brief Additive scalar advance, finite in [0,1e12]. Negative/NaN fails.
 * Borrowed context; called once per visible scalar, never for hard separators,
 * soft hyphen, zero-width space or word joiner. Must not mutate inputs/output. */
typedef double (*mulib_linebreak_measure_fn)(void *context, uint32_t scalar);
typedef struct mulib_linebreak_options {
    double width;
    mulib_linebreak_measure_fn measure;
    void *context;
    mulib_uniproc_t *unicode;
    const char *locale;
    double hyphen_width, space_stretch, space_shrink, tolerance;
    double line_penalty, hyphen_penalty, fitness_demerit, flagged_demerit;
    size_t max_work;
} mulib_linebreak_options_t;
/** @brief Borrowed UTF-8 display slice [start,end); next consumes separators.
 * width is natural advance, adjustment is the Knuth-Plass glue ratio (else 0).
 * HYPHEN requests an appended hyphen; soft hyphens inside slices stay invisible.
 * HARD includes a consumed hard separator; OVERFULL means natural width>limit. */
typedef struct mulib_linebreak_line {
    size_t start, end, next;
    double width, adjustment;
    unsigned flags;
} mulib_linebreak_line_t;
typedef struct mulib_linebreak_result {
    mulib_linebreak_line_t *lines;
    size_t count;
    double cost;
} mulib_linebreak_result_t;
/** @brief Set defaults: scalar width 1, tab 4, stretch .5, shrink 1/3,
 * tolerance 3, hyphen width 1, penalties 10/50/100/100, work 10000000.
 * NULL is a no-op. Width is validated by mulib_linebreak, not this initializer. */
void mulib_linebreak_options_init(mulib_linebreak_options_t *options, double width);
/** @brief Wrap strict UTF-8 (embedded NUL allowed), length <=1 MiB.
 * width must be finite in [1e-6,1e12]. All offsets are UTF-8 byte offsets.
 * Empty input yields zero lines; terminal separators add no phantom line.
 * Replace a zero-initialized/previous result on success only; all failures
 * preserve it. Inputs/options/backend are borrowed only during this call.
 * UNICODE requires options->unicode; no implicit backend or emergency splits.
 * Other strategies use SP/TAB, hyphen, soft hyphen and ZWSP opportunities;
 * CHARACTER instead permits every scalar boundary (not every grapheme).
 * Knuth-Plass can return INFEASIBLE; other strategies allow indivisible overflow.
 * Cost is squared raggedness, balancing error or Knuth-Plass demerits; greedy,
 * character and Unicode report zero. See manual/mulib-linebreak.md. */
int mulib_linebreak(const char *text, size_t length,
                    mulib_linebreak_algorithm_t algorithm,
                    const mulib_linebreak_options_t *options,
                    mulib_linebreak_result_t *result);
/** @brief Free owned lines and zero result; NULL/repeated calls are safe. */
void mulib_linebreak_result_free(mulib_linebreak_result_t *result);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_LINEBREAK) && !defined(MULIB_LINEBREAK_IMPLEMENTED)
#define MULIB_LINEBREAK_IMPLEMENTED
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_linebreak_scalar_ {
    size_t byte;
    uint32_t cp;
    double width, stretch, shrink;
} mulib_linebreak_scalar_;
typedef struct mulib_linebreak_node_ {
    size_t at, end;
    unsigned flags;
} mulib_linebreak_node_;
typedef struct mulib_linebreak_work_ {
    const mulib_linebreak_options_t *o;
    mulib_linebreak_scalar_ *p;
    mulib_linebreak_node_ *b;
    mulib_linebreak_result_t result;
    size_t work;
    int status;
} mulib_linebreak_work_;
typedef struct mulib_linebreak_edge_ {
    double width, stretch, shrink, base;
} mulib_linebreak_edge_;

void mulib_linebreak_options_init(mulib_linebreak_options_t *o, double width) {
    if (!o) return;
    memset(o, 0, sizeof *o);
    o->width = width; o->hyphen_width = 1;
    o->space_stretch = .5; o->space_shrink = 1.0 / 3.0; o->tolerance = 3;
    o->line_penalty = 10; o->hyphen_penalty = 50;
    o->fitness_demerit = 100; o->flagged_demerit = 100; o->max_work = 10000000;
}
void mulib_linebreak_result_free(mulib_linebreak_result_t *r) {
    if (r) { free(r->lines); memset(r, 0, sizeof *r); }
}
static int mulib_linebreak_number_(double x, double maximum) {
    return isfinite(x) && x >= 0 && x <= maximum;
}
static int mulib_linebreak_hard_(uint32_t cp) {
    return cp == '\r' || cp == '\n' || cp == 0xb || cp == 0xc ||
           cp == 0x85 || cp == 0x2028 || cp == 0x2029;
}
static int mulib_linebreak_space_(uint32_t cp) { return cp == ' ' || cp == '\t'; }
static int mulib_linebreak_edge_get_(mulib_linebreak_work_ *w, size_t i, size_t j,
                                     mulib_linebreak_edge_ *e) {
    size_t start = w->b[i].at, end = w->b[j].end;
    if (w->work == w->o->max_work) { w->status = MULIB_LINEBREAK_LIMIT; return 0; }
    ++w->work;
    if (end < start) end = start;
    e->base = w->p[end].width - w->p[start].width;
    e->width = e->base + ((w->b[j].flags & MULIB_LINEBREAK_HYPHEN) ? w->o->hyphen_width : 0);
    e->stretch = w->p[end].stretch - w->p[start].stretch;
    e->shrink = w->p[end].shrink - w->p[start].shrink;
    return 1;
}
static double mulib_linebreak_square_(double x) { return x * x; }
static double mulib_linebreak_rag_(mulib_linebreak_work_ *w, double width, int last) {
    return last && width <= w->o->width ? 0 : mulib_linebreak_square_(w->o->width - width);
}
/* Find the last fitting candidate; never split an indivisible segment. A soft
 * hyphen's extra advance is not monotone, but the base prefix advance is. */
static size_t mulib_linebreak_greedy_(mulib_linebreak_work_ *w, size_t i, size_t end) {
    size_t j, best = i + 1;
    for (j = i + 1; j <= end; ++j) {
        mulib_linebreak_edge_ e;
        if (!mulib_linebreak_edge_get_(w, i, j, &e)) return 0;
        if (e.width <= w->o->width) best = j;
        if (e.base > w->o->width) break;
    }
    return best;
}
static int mulib_linebreak_emit_(mulib_linebreak_work_ *w, size_t i, size_t j, double ratio) {
    mulib_linebreak_edge_ e;
    mulib_linebreak_line_t *line;
    if (!mulib_linebreak_edge_get_(w, i, j, &e)) return 0;
    line = &w->result.lines[w->result.count++];
    line->start = w->p[w->b[i].at].byte;
    line->end = w->p[w->b[j].end < w->b[i].at ? w->b[i].at : w->b[j].end].byte;
    line->next = w->p[w->b[j].at].byte;
    line->width = e.width; line->adjustment = ratio;
    line->flags = w->b[j].flags | (e.width > w->o->width ? (unsigned)MULIB_LINEBREAK_OVERFULL : 0u);
    return 1;
}
static int mulib_linebreak_knuth_(mulib_linebreak_work_ *w, const mulib_linebreak_edge_ *e,
                                  int last, double *ratio, size_t *fitness, double *badness) {
    double delta = w->o->width - e->width, capacity;
    if (last && delta >= 0) *ratio = 0;
    else if (delta == 0) *ratio = 0;
    else {
        capacity = delta > 0 ? e->stretch : e->shrink;
        if (capacity <= 0) return 0;
        *ratio = delta / capacity;
        if (*ratio < -1 || *ratio > w->o->tolerance) return 0;
    }
    *fitness = *ratio < -.5 ? 0 : *ratio <= .5 ? 1 : *ratio <= 1 ? 2 : 3;
    capacity = fabs(*ratio);
    *badness = capacity >= 4.641588833612779 ? 10000 : 100 * capacity * capacity * capacity;
    return 1;
}
static int mulib_linebreak_local_(mulib_linebreak_work_ *w, size_t start, size_t end, int lookahead) {
    size_t i = start;
    while (i < end) {
        size_t best = mulib_linebreak_greedy_(w, i, end), j;
        double score = DBL_MAX;
        if (w->status) return w->status;
        if (lookahead) {
            for (j = i + 1; j <= end; ++j) {
                mulib_linebreak_edge_ e, second;
                double candidate;
                size_t next;
                if (!mulib_linebreak_edge_get_(w, i, j, &e)) return w->status;
                if (e.width > w->o->width && j != i + 1) {
                    if (e.base > w->o->width) break;
                    continue;
                }
                candidate = mulib_linebreak_rag_(w, e.width, j == end);
                if (j != end) {
                    next = mulib_linebreak_greedy_(w, j, end);
                    if (w->status || !mulib_linebreak_edge_get_(w, j, next, &second)) return w->status;
                    candidate += mulib_linebreak_rag_(w, second.width, next == end);
                }
                if (candidate <= score) { score = candidate; best = j; }
            }
        }
        if (!mulib_linebreak_emit_(w, i, best, 0)) return w->status;
        if (lookahead) w->result.cost += mulib_linebreak_rag_(w, w->result.lines[w->result.count - 1].width, best == end);
        i = best;
    }
    return MULIB_LINEBREAK_OK;
}
/* A shortest path over legal breakpoints. Knuth-Plass retains four fitness
 * states; balancing adds a line-count dimension instead of a last-line waiver. */
static int mulib_linebreak_optimize_(mulib_linebreak_work_ *w, size_t start, size_t end,
                                     mulib_linebreak_algorithm_t algorithm) {
    size_t n = end - start + 1, states = algorithm == MULIB_LINEBREAK_KNUTH_PLASS ? 4 : 1;
    size_t layers = 1, lines = 0, cells, layer, j, i, f, index, best, first = w->result.count;
    size_t *previous = NULL;
    double *cost = NULL, target = w->o->width;
    int balanced = algorithm == MULIB_LINEBREAK_BALANCED, rc = MULIB_LINEBREAK_OK;
    if (balanced) {
        for (i = start; i < end; ++lines) {
            i = mulib_linebreak_greedy_(w, i, end);
            if (w->status) return w->status;
        }
        layers = lines + 1;
        target = (w->p[w->b[end].end].width - w->p[w->b[start].at].width) / (double)lines;
    }
    if (n > SIZE_MAX / states || n * states > SIZE_MAX / layers) return MULIB_LINEBREAK_LIMIT;
    cells = n * states * layers;
    if (cells > w->o->max_work || cells > SIZE_MAX / sizeof *cost || cells > SIZE_MAX / sizeof *previous)
        return MULIB_LINEBREAK_LIMIT;
    cost = (double *)malloc(cells * sizeof *cost);
    previous = (size_t *)malloc(cells * sizeof *previous);
    if (!cost || !previous) { rc = MULIB_LINEBREAK_MEMORY; goto done; }
    for (i = 0; i < cells; ++i) { cost[i] = DBL_MAX; previous[i] = SIZE_MAX; }
    cost[states == 4 ? 1 : 0] = 0;
    for (layer = balanced ? 1 : 0; layer < layers; ++layer) {
        for (j = 1; j < n; ++j) for (i = 0; i < j; ++i) {
            mulib_linebreak_edge_ e;
            double ratio = 0, badness = 0, linecost;
            size_t fitness = 0, destination, source_layer = balanced ? layer - 1 : layer;
            if (!mulib_linebreak_edge_get_(w, start + i, start + j, &e)) { rc = w->status; goto done; }
            if (states == 4) {
                if (!mulib_linebreak_knuth_(w, &e, j == n - 1, &ratio, &fitness, &badness)) continue;
                linecost = mulib_linebreak_square_(w->o->line_penalty + badness);
                if (w->b[start + j].flags & MULIB_LINEBREAK_HYPHEN)
                    linecost += mulib_linebreak_square_(w->o->hyphen_penalty);
            } else {
                if (e.width > w->o->width && j != i + 1) continue;
                linecost = balanced ? mulib_linebreak_square_(target - e.width) : mulib_linebreak_rag_(w, e.width, j == n - 1);
            }
            destination = (layer * n + j) * states + fitness;
            for (f = 0; f < states; ++f) {
                double candidate;
                size_t source = (source_layer * n + i) * states + f;
                if (cost[source] == DBL_MAX) continue;
                candidate = cost[source] + linecost;
                if (states == 4 && (f + 1 < fitness || fitness + 1 < f)) candidate += w->o->fitness_demerit;
                if (states == 4 && (w->b[start + i].flags & w->b[start + j].flags & MULIB_LINEBREAK_HYPHEN))
                    candidate += w->o->flagged_demerit;
                if (candidate < cost[destination]) { cost[destination] = candidate; previous[destination] = source; }
            }
        }
    }
    best = ((layers - 1) * n + n - 1) * states;
    for (f = 0; f < states; ++f) {
        index = ((layers - 1) * n + n - 1) * states + f;
        if (cost[index] < cost[best]) best = index;
    }
    if (cost[best] == DBL_MAX) { rc = MULIB_LINEBREAK_INFEASIBLE; goto done; }
    w->result.cost += cost[best];
    for (index = best; previous[index] != SIZE_MAX; index = previous[index]) {
        size_t from = previous[index], a = (from / states) % n, b = (index / states) % n, fit;
        double ratio = 0, badness;
        mulib_linebreak_edge_ e;
        if (states == 4) {
            if (!mulib_linebreak_edge_get_(w, start + a, start + b, &e)) { rc = w->status; goto done; }
            (void)mulib_linebreak_knuth_(w, &e, b == n - 1, &ratio, &fit, &badness);
        }
        if (!mulib_linebreak_emit_(w, start + a, start + b, ratio)) { rc = w->status; goto done; }
    }
    for (i = first, j = w->result.count; i < j && i < --j; ++i) {
        mulib_linebreak_line_t swap = w->result.lines[i];
        w->result.lines[i] = w->result.lines[j]; w->result.lines[j] = swap;
    }
done:
    free(cost); free(previous); return rc;
}
int mulib_linebreak(const char *text, size_t length, mulib_linebreak_algorithm_t algorithm,
                    const mulib_linebreak_options_t *o, mulib_linebreak_result_t *out) {
    mulib_linebreak_work_ w;
    mulib_unibreak_boundary_t *unicode = NULL;
    size_t count = 0, bytes, used, k, nb = 1, nu = 0, u = 1, start = 0, trim = 0;
    uint32_t cp;
    int rc = MULIB_LINEBREAK_OK;
    memset(&w, 0, sizeof w); w.o = o;
    if (!o || !out || (!text && length) || algorithm < MULIB_LINEBREAK_GREEDY || algorithm > MULIB_LINEBREAK_UNICODE ||
        !isfinite(o->width) || o->width < 1e-6 || o->width > 1e12 || !o->max_work ||
        !mulib_linebreak_number_(o->hyphen_width, 1e12) || !mulib_linebreak_number_(o->space_stretch, 1000) ||
        !mulib_linebreak_number_(o->space_shrink, 1) || !mulib_linebreak_number_(o->tolerance, 1000) ||
        !mulib_linebreak_number_(o->line_penalty, 1e6) || !mulib_linebreak_number_(o->hyphen_penalty, 1e6) ||
        !mulib_linebreak_number_(o->fitness_demerit, 1e12) || !mulib_linebreak_number_(o->flagged_demerit, 1e12))
        return MULIB_LINEBREAK_INVALID;
    if (length > 1048576) return MULIB_LINEBREAK_LIMIT;
    for (bytes = 0; bytes < length; bytes += used) {
        if (!mulib_utf8_decode((const unsigned char *)text + bytes, length - bytes, &cp, &used)) return MULIB_LINEBREAK_INVALID;
        ++count;
    }
    if (algorithm == MULIB_LINEBREAK_UNICODE) {
        if (!o->unicode) return MULIB_LINEBREAK_BACKEND;
        if (mulib_unibreak_find(o->unicode, MULIB_UNIBREAK_LINE, o->locale, text, length, &unicode, &nu))
            return MULIB_LINEBREAK_BACKEND;
    }
    if (!count) { mulib_linebreak_result_free(out); free(unicode); return MULIB_LINEBREAK_OK; }
    if (count + 1 > SIZE_MAX / sizeof *w.p || count + 1 > SIZE_MAX / sizeof *w.b ||
        count > SIZE_MAX / sizeof *w.result.lines) { rc = MULIB_LINEBREAK_LIMIT; goto done; }
    w.p = (mulib_linebreak_scalar_ *)calloc(count + 1, sizeof *w.p);
    w.b = (mulib_linebreak_node_ *)calloc(count + 1, sizeof *w.b);
    w.result.lines = (mulib_linebreak_line_t *)malloc(count * sizeof *w.result.lines);
    if (!w.p || !w.b || !w.result.lines) { rc = MULIB_LINEBREAK_MEMORY; goto done; }
    for (k = 0, bytes = 0; k < count; ++k, bytes += used) {
        double width = 0;
        (void)mulib_utf8_decode((const unsigned char *)text + bytes, length - bytes, &cp, &used);
        w.p[k].byte = bytes; w.p[k].cp = cp;
        if (!mulib_linebreak_hard_(cp) && cp != 0xad && cp != 0x200b && cp != 0x2060)
            width = o->measure ? o->measure(o->context, cp) : cp == '\t' ? 4 : 1;
        if (!mulib_linebreak_number_(width, 1e12)) { rc = MULIB_LINEBREAK_INVALID; goto done; }
        w.p[k + 1].width = w.p[k].width + width;
        w.p[k + 1].stretch = w.p[k].stretch + (mulib_linebreak_space_(cp) ? width * o->space_stretch : 0);
        w.p[k + 1].shrink = w.p[k].shrink + (mulib_linebreak_space_(cp) ? width * o->space_shrink : 0);
    }
    w.p[count].byte = length;
    for (k = 1; k <= count; ++k) {
        uint32_t before = w.p[k - 1].cp, after = w.p[k].cp;
        unsigned flags = 0;
        int hard = mulib_linebreak_hard_(before), candidate = 0;
        size_t end;
        if (before == '\r' && after == '\n' && k < count) continue;
        if (hard) flags = MULIB_LINEBREAK_HARD;
        if (!mulib_linebreak_space_(before) && !hard) trim = k;
        end = trim;
        if (algorithm == MULIB_LINEBREAK_UNICODE) {
            while (u < nu && unicode[u].offset < w.p[k].byte) ++u;
            candidate = u < nu && unicode[u].offset == w.p[k].byte;
            if (candidate && unicode[u].status >= 100) { hard = 1; flags |= MULIB_LINEBREAK_HARD; }
        } else if (algorithm == MULIB_LINEBREAK_CHARACTER) candidate = 1;
        else candidate = (mulib_linebreak_space_(before) && !mulib_linebreak_space_(after) && trim > w.b[start].at) ||
                         before == '-' || before == 0xad || before == 0x200b;
        if (k == count || hard) candidate = 1;
        if (!candidate) continue;
        if (before == 0xad && !hard && k != count) { flags |= MULIB_LINEBREAK_HYPHEN; end = k - 1; }
        if (before == 0x200b) end = k - 1;
        w.b[nb].at = k; w.b[nb].end = end; w.b[nb].flags = flags; ++nb;
        if (hard) { start = nb - 1; trim = k; }
    }
    start = 0;
    for (k = 1; k < nb; ++k) if ((w.b[k].flags & MULIB_LINEBREAK_HARD) || k == nb - 1) {
        if (algorithm == MULIB_LINEBREAK_MINIMUM_RAGGEDNESS || algorithm == MULIB_LINEBREAK_BALANCED ||
            algorithm == MULIB_LINEBREAK_KNUTH_PLASS) rc = mulib_linebreak_optimize_(&w, start, k, algorithm);
        else rc = mulib_linebreak_local_(&w, start, k, algorithm == MULIB_LINEBREAK_LOOKAHEAD);
        if (rc) goto done;
        start = k;
    }
    mulib_linebreak_result_free(out); *out = w.result; memset(&w.result, 0, sizeof w.result);
done:
    free(unicode); free(w.p); free(w.b); mulib_linebreak_result_free(&w.result); return rc;
}
#ifdef __cplusplus
}
#endif
#endif
