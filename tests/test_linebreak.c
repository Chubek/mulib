#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_LINEBREAK
#include "mulib-linebreak.h"
#include <assert.h>

static void check_lines(const char *s, size_t n, const mulib_linebreak_result_t *r) {
    size_t i, next = 0;
    for (i = 0; i < r->count; ++i) {
        const mulib_linebreak_line_t *line = &r->lines[i];
        assert(line->start == next && line->start <= line->end && line->end <= line->next && line->next <= n);
        assert(line->next > line->start);
        assert(line->start == n || ((unsigned char)s[line->start] & 0xc0) != 0x80);
        assert(line->end == n || ((unsigned char)s[line->end] & 0xc0) != 0x80);
        assert(line->next == n || ((unsigned char)s[line->next] & 0xc0) != 0x80);
        assert(isfinite(line->width) && line->width >= 0 && isfinite(line->adjustment));
        next = line->next;
    }
    assert(next == n && isfinite(r->cost));
}
static double measured(void *context, uint32_t cp) {
    size_t *calls = (size_t *)context;
    ++*calls;
    return cp == 'W' ? 3 : cp == 0x301 ? 0 : 1;
}
static double invalid_measure(void *context, uint32_t cp) {
    (void)cp; return *(double *)context;
}
static void basics(void) {
    mulib_linebreak_options_t o;
    mulib_linebreak_result_t r = {0};
    const char text[] = "aaa bb cc ddddd";
    int a;
    mulib_linebreak_options_init(&o, 6);
    for (a = MULIB_LINEBREAK_GREEDY; a < MULIB_LINEBREAK_UNICODE; ++a) {
        if (a == MULIB_LINEBREAK_KNUTH_PLASS) continue;
        assert(!mulib_linebreak(text, sizeof text - 1, (mulib_linebreak_algorithm_t)a, &o, &r));
        check_lines(text, sizeof text - 1, &r);
        if (a == MULIB_LINEBREAK_GREEDY) {
            assert(r.count == 3 && r.lines[0].end == 6 && r.lines[1].width == 2);
        }
        if (a == MULIB_LINEBREAK_MINIMUM_RAGGEDNESS || a == MULIB_LINEBREAK_BALANCED || a == MULIB_LINEBREAK_LOOKAHEAD) {
            assert(r.count == 3 && r.lines[0].end == 3 && r.lines[1].width == 5);
        }
    }
    assert(!mulib_linebreak(NULL, 0, MULIB_LINEBREAK_GREEDY, &o, &r) && !r.lines && !r.count);
    mulib_linebreak_result_free(&r); mulib_linebreak_result_free(&r); mulib_linebreak_result_free(NULL);
    mulib_linebreak_options_init(NULL, 0);
}
static void hard_and_offsets(void) {
    const char text[] = "a\r\n\n b\t\r\xc2\x85\xe2\x80\xa8\xe2\x80\xa9";
    const char wide[] = "\xc3\xa9\xf0\x9f\x98\x80z";
    const char shy[] = "ab\xc2\xad" "cd ef";
    mulib_linebreak_options_t o;
    mulib_linebreak_result_t r = {0};
    int a;
    mulib_linebreak_options_init(&o, 20);
    for (a = MULIB_LINEBREAK_GREEDY; a < MULIB_LINEBREAK_UNICODE; ++a) {
        assert(!mulib_linebreak(text, sizeof text - 1, (mulib_linebreak_algorithm_t)a, &o, &r));
        check_lines(text, sizeof text - 1, &r);
        assert(r.count == 6 && r.lines[0].end == 1 && r.lines[0].next == 3);
        assert(r.lines[1].start == r.lines[1].end && r.lines[2].width == 2);
        assert(r.lines[2].end == 6);
        assert(r.lines[5].flags & MULIB_LINEBREAK_HARD);
    }
    o.width = 1;
    assert(!mulib_linebreak(wide, sizeof wide - 1, MULIB_LINEBREAK_CHARACTER, &o, &r));
    assert(r.count == 3 && r.lines[0].next == 2 && r.lines[1].next == 6);
    check_lines(wide, sizeof wide - 1, &r);
    assert(!mulib_linebreak(wide, sizeof wide - 1, MULIB_LINEBREAK_GREEDY, &o, &r));
    assert(r.count == 1 && r.lines[0].width == 3 && (r.lines[0].flags & MULIB_LINEBREAK_OVERFULL));
    o.width = 3;
    assert(!mulib_linebreak(shy, sizeof shy - 1, MULIB_LINEBREAK_GREEDY, &o, &r));
    check_lines(shy, sizeof shy - 1, &r);
    assert(r.count == 3 && r.lines[0].end == 2 && r.lines[0].next == 4 && r.lines[0].width == 3);
    assert(r.lines[0].flags & MULIB_LINEBREAK_HYPHEN);
    o.width = 20;
    assert(!mulib_linebreak("  \t", 3, MULIB_LINEBREAK_BALANCED, &o, &r));
    assert(r.count == 1 && r.lines[0].end == 0 && r.lines[0].next == 3);
    assert(!mulib_linebreak("a\0b", 3, MULIB_LINEBREAK_GREEDY, &o, &r));
    assert(r.count == 1 && r.lines[0].end == 3 && r.lines[0].width == 3);
    mulib_linebreak_result_free(&r);
}
static void callbacks_and_errors(void) {
    mulib_linebreak_options_t o;
    mulib_linebreak_result_t r = {0};
    mulib_linebreak_line_t *saved;
    size_t calls = 0;
    double bad[] = {-1, NAN, INFINITY, 1e13};
    size_t i;
    mulib_linebreak_options_init(&o, 3);
    o.measure = measured; o.context = &calls;
    assert(!mulib_linebreak("We\xcc\x81 W", 6, MULIB_LINEBREAK_CHARACTER, &o, &r));
    assert(calls == 5 && r.count == 3 && r.lines[1].width == 1);
    saved = r.lines;
    assert(mulib_linebreak("\xc0\x80", 2, MULIB_LINEBREAK_GREEDY, &o, &r) == MULIB_LINEBREAK_INVALID);
    assert(r.lines == saved);
    assert(mulib_linebreak(NULL, 1, MULIB_LINEBREAK_GREEDY, &o, &r) == MULIB_LINEBREAK_INVALID);
    assert(mulib_linebreak("", 0, (mulib_linebreak_algorithm_t)99, &o, &r) == MULIB_LINEBREAK_INVALID);
    assert(mulib_linebreak("", 0, MULIB_LINEBREAK_GREEDY, NULL, &r) == MULIB_LINEBREAK_INVALID);
    assert(mulib_linebreak("", 0, MULIB_LINEBREAK_GREEDY, &o, NULL) == MULIB_LINEBREAK_INVALID);
    assert(mulib_linebreak("x", SIZE_MAX, MULIB_LINEBREAK_GREEDY, &o, &r) == MULIB_LINEBREAK_LIMIT);
    o.measure = invalid_measure;
    for (i = 0; i < sizeof bad / sizeof *bad; ++i) {
        o.context = &bad[i];
        assert(mulib_linebreak("x", 1, MULIB_LINEBREAK_GREEDY, &o, &r) == MULIB_LINEBREAK_INVALID);
        o.width = bad[i];
        assert(mulib_linebreak("", 0, MULIB_LINEBREAK_GREEDY, &o, &r) == MULIB_LINEBREAK_INVALID);
        o.width = 3;
    }
    mulib_linebreak_options_init(&o, 3); o.max_work = 1;
    for (i = 0; i < MULIB_LINEBREAK_UNICODE; ++i)
        assert(mulib_linebreak("a b c d", 7, (mulib_linebreak_algorithm_t)i, &o, &r) == MULIB_LINEBREAK_LIMIT);
    mulib_linebreak_options_init(&o, 3);
    assert(mulib_linebreak("x", 1, MULIB_LINEBREAK_UNICODE, &o, &r) == MULIB_LINEBREAK_BACKEND);
    assert(mulib_linebreak("abcdef", 6, MULIB_LINEBREAK_KNUTH_PLASS, &o, &r) == MULIB_LINEBREAK_INFEASIBLE);
    assert(r.lines == saved);
    mulib_linebreak_result_free(&r);
}
static void unicode_lines(void) {
    mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
    mulib_linebreak_options_t o;
    mulib_linebreak_result_t r = {0};
    const char cjk[] = "\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89\xe5\x9b\x9b";
    const char nbsp[] = "a\xc2\xa0" "b c";
    const char combining[] = "e\xcc\x81 x";
    const char family[] = "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9 x";
    if (!u) { assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS")); puts("SKIP: ICU line breaking"); return; }
    mulib_linebreak_options_init(&o, 2); o.unicode = u;
    assert(!mulib_linebreak(cjk, sizeof cjk - 1, MULIB_LINEBREAK_UNICODE, &o, &r));
    assert(r.count == 2 && r.lines[0].end == 6);
    check_lines(cjk, sizeof cjk - 1, &r);
    o.width = 1;
    assert(!mulib_linebreak(nbsp, sizeof nbsp - 1, MULIB_LINEBREAK_UNICODE, &o, &r));
    assert(r.count == 2 && r.lines[0].end == 4 && (r.lines[0].flags & MULIB_LINEBREAK_OVERFULL));
    assert(!mulib_linebreak(combining, sizeof combining - 1, MULIB_LINEBREAK_UNICODE, &o, &r));
    assert(r.count == 2 && r.lines[0].end == 3);
    assert(!mulib_linebreak(family, sizeof family - 1, MULIB_LINEBREAK_UNICODE, &o, &r));
    assert(r.count == 2 && r.lines[0].end == 11);
    assert(!mulib_linebreak("a\r\nb\n", 5, MULIB_LINEBREAK_UNICODE, &o, &r));
    assert(r.count == 2 && r.lines[0].next == 3 && r.lines[1].next == 5);
    assert(!mulib_linebreak(NULL, 0, MULIB_LINEBREAK_UNICODE, &o, &r) && !r.count);
    mulib_linebreak_result_free(&r); mulib_uniproc_close(u);
}
static void knuth_lines(void) {
    mulib_linebreak_options_t o;
    mulib_linebreak_result_t r = {0};
    mulib_linebreak_options_init(&o, 6);
    assert(!mulib_linebreak("aa bb cc", 8, MULIB_LINEBREAK_KNUTH_PLASS, &o, &r));
    assert(r.count == 2 && r.lines[0].width == 5 && r.lines[0].adjustment == 2);
    assert(r.cost == 656400); /* (10+800)^2 + two fitness transitions + 10^2 */
    o.tolerance = 1;
    assert(mulib_linebreak("aa bb cc", 8, MULIB_LINEBREAK_KNUTH_PLASS, &o, &r) == MULIB_LINEBREAK_INFEASIBLE);
    o.width = 5; o.space_shrink = 1;
    assert(!mulib_linebreak("aaa bb cc", 9, MULIB_LINEBREAK_KNUTH_PLASS, &o, &r));
    assert(r.count == 2 && r.lines[0].width == 6 && r.lines[0].adjustment == -1);
    assert(r.lines[0].flags & MULIB_LINEBREAK_OVERFULL);
    assert(r.cost == 12200);
    mulib_linebreak_options_init(&o, 3);
    assert(!mulib_linebreak("ab\xc2\xad" "cd", 6, MULIB_LINEBREAK_KNUTH_PLASS, &o, &r));
    assert(r.count == 2 && (r.lines[0].flags & MULIB_LINEBREAK_HYPHEN));
    assert(r.cost == 2700);
    /* Every selected discretionary is flagged, including repeated hyphens. */
    assert(!mulib_linebreak("ab\xc2\xad" "cd\xc2\xad" "ef", 10, MULIB_LINEBREAK_KNUTH_PLASS, &o, &r));
    assert(r.count == 3 && r.cost == 5400);
    mulib_linebreak_result_free(&r);
}
/* Exhaustively enumerate all partitions of small ASCII paragraphs, independently
 * checking the three global objectives rather than only selected breakpoints. */
static void optimality(void) {
    unsigned seed = 321, trial;
    for (trial = 0; trial < 250; ++trial) {
        char text[100];
        size_t lengths[7], words, n = 0, i, j, lines, mask, a;
        double oracle[3] = {DBL_MAX, DBL_MAX, DBL_MAX}, target;
        mulib_linebreak_options_t o;
        mulib_linebreak_result_t r = {0};
        seed = seed * 1664525u + 1013904223u; words = seed % 7 + 1;
        seed = seed * 1664525u + 1013904223u;
        mulib_linebreak_options_init(&o, seed % 11 + 2);
        for (i = 0; i < words; ++i) {
            seed = seed * 1664525u + 1013904223u; lengths[i] = seed % 9 + 1;
            if (i) text[n++] = ' ';
            for (j = 0; j < lengths[i]; ++j) text[n++] = 'a';
        }
        assert(!mulib_linebreak(text, n, MULIB_LINEBREAK_GREEDY, &o, &r));
        lines = r.count; target = (double)n / (double)lines;
        for (mask = 0; mask < ((size_t)1 << (words - 1)); ++mask) {
            double rag = 0, balance = 0, knuth = 0;
            size_t begin = 0, count = 0, previous_fitness = 1;
            int legal = 1, feasible = 1;
            for (i = 0; i < words; ++i) if (i == words - 1 || (mask & ((size_t)1 << i))) {
                double width = (double)(i - begin), delta, ratio = 0, badness;
                size_t fitness;
                for (j = begin; j <= i; ++j) width += (double)lengths[j];
                ++count;
                if (width > o.width && begin != i) legal = 0;
                if (i != words - 1 || width > o.width) rag += (o.width - width) * (o.width - width);
                balance += (target - width) * (target - width);
                delta = o.width - width;
                if (!(i == words - 1 && delta >= 0) && delta != 0) {
                    double capacity = (double)(i - begin) * (delta > 0 ? o.space_stretch : o.space_shrink);
                    if (!capacity) feasible = 0;
                    else { ratio = delta / capacity; if (ratio < -1 || ratio > o.tolerance) feasible = 0; }
                }
                fitness = ratio < -.5 ? 0 : ratio <= .5 ? 1 : ratio <= 1 ? 2 : 3;
                badness = fmin(10000, 100 * pow(fabs(ratio), 3));
                knuth += (10 + badness) * (10 + badness);
                if (fitness + 1 < previous_fitness || previous_fitness + 1 < fitness) knuth += 100;
                previous_fitness = fitness; begin = i + 1;
            }
            if (legal && rag < oracle[0]) oracle[0] = rag;
            if (legal && count == lines && balance < oracle[1]) oracle[1] = balance;
            if (feasible && knuth < oracle[2]) oracle[2] = knuth;
        }
        for (a = 0; a < 3; ++a) {
            const mulib_linebreak_algorithm_t algorithms[] = {MULIB_LINEBREAK_MINIMUM_RAGGEDNESS,
                MULIB_LINEBREAK_BALANCED, MULIB_LINEBREAK_KNUTH_PLASS};
            int rc = mulib_linebreak(text, n, algorithms[a], &o, &r);
            if (oracle[a] == DBL_MAX) assert(rc == MULIB_LINEBREAK_INFEASIBLE);
            else {
                assert(!rc); check_lines(text, n, &r);
                assert(fabs(r.cost - oracle[a]) <= 1e-7 * (1 + oracle[a]));
                if (a == 1) assert(r.count == lines);
            }
        }
        mulib_linebreak_result_free(&r);
    }
}
static void mixed_text(void) {
    const uint32_t alphabet[] = {' ', '\t', 'a', '-', 0xad, 0x200b, 0xa0, 0x2060,
        0, '\r', '\n', 0xb, 0xc, 0x85, 0x2028, 0x2029, 0x1f600, 0x301};
    mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
    mulib_linebreak_result_t r = {0};
    mulib_linebreak_options_t o;
    unsigned seed = 891, trial;
    for (trial = 0; trial < 100; ++trial) {
        char text[100]; size_t n = 0, i, j;
        int a;
        seed = seed * 1664525u + 1013904223u;
        mulib_linebreak_options_init(&o, seed % 12 + 1); o.unicode = u;
        for (i = 0; i < 20; ++i) {
            seed = seed * 1664525u + 1013904223u;
            n += mulib_utf8_encode(alphabet[seed % (sizeof alphabet / sizeof *alphabet)],
                                   (unsigned char *)text + n, sizeof text - n);
        }
        for (a = MULIB_LINEBREAK_GREEDY; a <= MULIB_LINEBREAK_UNICODE; ++a) {
            int rc;
            if (a == MULIB_LINEBREAK_UNICODE && !u) continue;
            rc = mulib_linebreak(text, n, (mulib_linebreak_algorithm_t)a, &o, &r);
            if (a == MULIB_LINEBREAK_KNUTH_PLASS && rc == MULIB_LINEBREAK_INFEASIBLE) continue;
            assert(!rc); check_lines(text, n, &r);
            for (i = 0; i < r.count; ++i) {
                const mulib_linebreak_line_t *line = &r.lines[i];
                for (j = line->start; j < line->next;) {
                    uint32_t cp; size_t used;
                    assert(mulib_utf8_decode((const unsigned char *)text + j, n - j, &cp, &used));
                    j += used;
                    if (cp == '\r' && j < n && text[j] == '\n') continue;
                    if (cp == '\r' || cp == '\n' || cp == 0xb || cp == 0xc || cp == 0x85 || cp == 0x2028 || cp == 0x2029) {
                        assert(j == line->next && (line->flags & MULIB_LINEBREAK_HARD));
                    }
                }
            }
            if (a == MULIB_LINEBREAK_UNICODE) {
                mulib_unibreak_boundary_t *b = NULL; size_t count;
                assert(!mulib_unibreak_find(u, MULIB_UNIBREAK_LINE, NULL, text, n, &b, &count));
                for (i = 0; i < r.count; ++i) {
                    for (j = 0; j < count && b[j].offset != r.lines[i].next; ++j) {}
                    assert(j < count);
                }
                free(b);
            }
        }
    }
    mulib_linebreak_result_free(&r); mulib_uniproc_close(u);
}
int main(void) {
    basics(); hard_and_offsets(); callbacks_and_errors(); unicode_lines(); knuth_lines(); optimality(); mixed_text();
    return 0;
}
