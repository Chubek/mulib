/* Headless UTF-8 editor regressions, moving realloc and allocation failures.
 * cc -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -Iinclude
 *    tests/test-gfx-text.c -lm -o /tmp/opencode/test-gfx-text
 */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { void *p; size_t n; } allocation;
static allocation allocations[1024];
static size_t live, calls, fail_at;

static allocation *find_allocation(void *p)
{
    for (size_t i = 0; i < sizeof allocations / sizeof *allocations; i++)
        if (allocations[i].p == p) return &allocations[i];
    assert(!"unknown allocation or exhausted tracking slots");
    return NULL;
}

static void *test_malloc(size_t n)
{
    void *p;
    allocation *a;
    if (++calls == fail_at) return NULL;
    p = malloc(n ? n : 1);
    if (!p) return NULL;
    a = find_allocation(NULL);
    a->p = p;
    a->n = n;
    live++;
    return p;
}

static void test_free(void *p)
{
    if (p) {
        allocation *a = find_allocation(p);
        a->p = NULL;
        live--;
        free(p);
    }
}

static void *test_calloc(size_t n, size_t size)
{
    void *p;
    if (size && n > SIZE_MAX / size) return NULL;
    p = test_malloc(n * size);
    if (p) memset(p, 0, n * size);
    return p;
}

/* Always moves: stale aliases that happen to survive libc realloc cannot hide. */
static void *test_realloc(void *p, size_t n)
{
    allocation *a;
    void *next;
    if (!p) return test_malloc(n);
    if (!n) {
        test_free(p);
        return NULL;
    }
    if (++calls == fail_at) return NULL;
    a = find_allocation(p);
    next = malloc(n);
    if (!next) return NULL;
    memcpy(next, p, n < a->n ? n : a->n);
    free(p);
    a->p = next;
    a->n = n;
    return next;
}

#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#include "mulib-texteditor.h"
#include "mulib-richtext.h"
#include "mulib-gui-undo.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void expect_text(const mulib_textbuffer_t *b, const char *s, size_t n)
{
    assert(mulib_textbuffer_valid(b));
    assert(b->n == n);
    assert(!n || !memcmp(b->data, s, n));
    assert(!b->data || b->data[n] == 0);
}

static void check_history(const mulib_texthistory_t *h)
{
    size_t bytes = 0;
    assert(mgx_texthistory_storage(h));
    for (size_t i = 0; i < h->n; i++) {
        assert(mulib_textselection_valid(&h->v[i].text, h->v[i].selection));
        assert(h->v[i].text.cap == h->v[i].text.n + 1);
        bytes += h->v[i].text.cap;
    }
    assert(bytes == h->bytes);
    assert(h->n <= mgx_texthistory_limit(h->limit));
    assert(bytes <= mgx_texthistory_byte_limit(h->byte_limit));
}

static void check_editor(const mulib_texteditor_t *e)
{
    assert(mulib_texteditor_valid(e));
    assert(mulib_textselection_valid(&e->text, e->selection));
    check_history(&e->history);
    if (e->history.n)
        expect_text(&e->text, e->history.v[e->history.pos].text.data,
                    e->history.v[e->history.pos].text.n);
}

/* Exact failure comparison includes pointers, capacities and the redo branch. */
typedef struct {
    mulib_texteditor_t editor;
    char text[256];
    mgx_text_snapshot snapshots[32];
    char contents[32][256];
} editor_image;

static void save_editor(editor_image *out, const mulib_texteditor_t *e)
{
    assert(e->text.n < sizeof out->text && e->history.n <= 32);
    memcpy(&out->editor, e, sizeof *e);
    if (e->text.data) memcpy(out->text, e->text.data, e->text.n + 1);
    for (size_t i = 0; i < e->history.n; i++) {
        assert(e->history.v[i].text.n < sizeof out->contents[i]);
        memcpy(&out->snapshots[i], &e->history.v[i], sizeof out->snapshots[i]);
        memcpy(out->contents[i], e->history.v[i].text.data,
               e->history.v[i].text.n + 1);
    }
}

static void unchanged_editor(const editor_image *old, const mulib_texteditor_t *e)
{
    assert(!memcmp(&old->editor, e, sizeof *e));
    if (e->text.data) assert(!memcmp(old->text, e->text.data, e->text.n + 1));
    for (size_t i = 0; i < e->history.n; i++) {
        assert(!memcmp(&old->snapshots[i], &e->history.v[i],
                       sizeof old->snapshots[i]));
        assert(!memcmp(old->contents[i], e->history.v[i].text.data,
                       e->history.v[i].text.n + 1));
    }
}

static size_t encode_scalar(uint32_t cp, char s[4])
{
    if (cp < 0x80) { s[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        s[0] = (char)(0xc0 | (cp >> 6));
        s[1] = (char)(0x80 | (cp & 63));
        return 2;
    }
    if (cp < 0x10000) {
        s[0] = (char)(0xe0 | (cp >> 12));
        s[1] = (char)(0x80 | ((cp >> 6) & 63));
        s[2] = (char)(0x80 | (cp & 63));
        return 3;
    }
    s[0] = (char)(0xf0 | (cp >> 18));
    s[1] = (char)(0x80 | ((cp >> 12) & 63));
    s[2] = (char)(0x80 | ((cp >> 6) & 63));
    s[3] = (char)(0x80 | (cp & 63));
    return 4;
}

static void test_utf8(void)
{
    static const struct { const char *s; size_t n; } bad[] = {
        {"\x80", 1}, {"\xbf", 1}, {"\xc0\x80", 2}, {"\xc1\xbf", 2},
        {"\xc2", 1}, {"\xc2x", 2}, {"\xe0\x80\x80", 3}, {"\xe0\xa0", 2},
        {"\xed\xa0\x80", 3}, {"\xed\xbf\xbf", 3}, {"\xef\xbf", 2},
        {"\xf0\x80\x80\x80", 4}, {"\xf0\x90\x80", 3},
        {"\xf4\x90\x80\x80", 4}, {"\xf5\x80\x80\x80", 4},
        {"\xff", 1}, {"\xe2\x28\xa1", 3}, {"\xf0\x28\x8c\xbc", 4}
    };
    uint32_t cp = 123;
    size_t used = 456;
    assert(mgx_utf8_valid(NULL, 0) && !mgx_utf8_valid(NULL, 1));
    assert(!mgx_utf8_decode(NULL, 1, &cp, &used));
    assert(!mgx_utf8_decode("", 0, &cp, &used));
    assert(!mgx_utf8_decode("a", 1, NULL, &used));
    assert(!mgx_utf8_decode("a", 1, &cp, NULL));
    assert(cp == 123 && used == 456);
    assert(mgx_utf8_boundary(NULL, 0, 0));
    assert(!mgx_utf8_boundary(NULL, 0, 1));
    assert(!mgx_utf8_boundary(NULL, 1, 0));
    assert(!mgx_utf8_boundary(NULL, 1, 1));
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        assert(!mgx_utf8_valid(bad[i].s, bad[i].n));
        assert(!mgx_utf8_decode(bad[i].s, bad[i].n, &cp, &used));
        assert(cp == 123 && used == 456);
    }
    /* Every Unicode scalar, including NUL/noncharacters, not just ASCII. */
    for (uint32_t c = 0; c <= 0x10ffff; c++) {
        char s[4];
        size_t n;
        if (c >= 0xd800 && c <= 0xdfff) continue;
        n = encode_scalar(c, s);
        assert(mgx_utf8_valid(s, n));
        assert(mgx_utf8_decode(s, n, &cp, &used) && cp == c && used == n);
        for (size_t i = 1; i < n; i++) {
            assert(!mgx_utf8_valid(s, i));
            assert(!mgx_utf8_boundary(s, n, i));
        }
    }
}

static void test_buffer(void)
{
    static const char text[] = "A\xc3\xa9\xf0\x9f\x98\x80Z";
    mulib_textbuffer_t b = {0}, old, broken = {NULL, 1, 0};
    size_t baseline;
    assert(mulib_textbuffer_valid(&b) && !mulib_textbuffer_valid(&broken));
    assert(!mulib_textbuffer_set(NULL, "", 0));
    assert(!mulib_textbuffer_replace(&broken, 0, 0, "", 0));
    assert(mulib_textbuffer_set(&b, text, sizeof text - 1));
    old = b;
    baseline = calls;
    for (size_t i = 0; i <= b.n; i++) {
        if (mgx_utf8_boundary(b.data, b.n, i)) continue;
        assert(!mulib_textbuffer_replace(&b, i, i, "x", 1));
        assert(!mulib_textbuffer_replace(&b, 0, i, "", 0));
        assert(!mulib_textbuffer_replace(&b, i, b.n, "", 0));
    }
    assert(!mulib_textbuffer_replace(&b, 4, 3, "", 0));
    assert(!mulib_textbuffer_replace(&b, 0, SIZE_MAX, "", 0));
    assert(!mulib_textbuffer_replace(&b, 0, 0, "", SIZE_MAX));
    assert(!mulib_textbuffer_replace(&b, 0, 0, NULL, 1));
    assert(!mulib_textbuffer_set(&b, "\xed\xa0\x80", 3));
    assert(calls == baseline && !memcmp(&b, &old, sizeof b));
    expect_text(&b, text, sizeof text - 1);
    b.data[b.n] = 'x';
    assert(!mulib_textbuffer_valid(&b));
    assert(!mulib_textbuffer_set(&b, "replacement", 11));
    b.data[b.n] = 0;
    b.data[0] = (char)0x80;
    assert(!mulib_textbuffer_valid(&b));
    assert(!mulib_textbuffer_replace(&b, 0, b.n, "a", 1));
    b.data[0] = 'A';
    mulib_textbuffer_free(&b);

    assert(mulib_textbuffer_set(&b, "abcde", 5));
    assert(mulib_textbuffer_replace(&b, b.n, b.n, b.data + 1, 4));
    expect_text(&b, "abcdebcde", 9);
    assert(mulib_textbuffer_set(&b, b.data + 5, 4));
    expect_text(&b, "bcde", 4);
    {
        const char nul[] = {'a', 0, 'b'};
        assert(mulib_textbuffer_set(&b, nul, sizeof nul));
        expect_text(&b, nul, sizeof nul);
    }
    mulib_textbuffer_free(&b);
    mulib_textbuffer_free(NULL);

    for (size_t fail = 1; ; fail++) {
        size_t before;
        int ok;
        assert(fail < 10);
        assert(mulib_textbuffer_set(&b, "abcde", 5));
        old = b;
        before = live;
        calls = 0;
        fail_at = fail;
        ok = mulib_textbuffer_replace(&b, b.n, b.n, b.data + 1, 4);
        fail_at = 0;
        if (!ok) {
            assert(!memcmp(&b, &old, sizeof b) && live == before);
            expect_text(&b, "abcde", 5);
        } else expect_text(&b, "abcdebcde", 9);
        mulib_textbuffer_free(&b);
        assert(!live);
        if (ok) break;
    }
}

static void test_cursor_selection(void)
{
    const char text[] = "A\xcc\x81\r\n"
        "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb"
        "\xf0\x9f\x87\xa6\xf0\x9f\x87\xa7\xf0\x9f\x87\xa8Z";
    const size_t edges[] = {0, 3, 5, 16, 24, 28, 29};
    mulib_textbuffer_t b = {0};
    mulib_textselection_t s = {0, 0}, old;
    assert(mulib_textbuffer_set(&b, text, sizeof text - 1));
    for (size_t k = 1; k < sizeof edges / sizeof *edges; k++) {
        for (size_t i = edges[k - 1]; i < edges[k]; i++)
            assert(mulib_textcursor_next(&b, i) == edges[k]);
        for (size_t i = edges[k - 1] + 1; i <= edges[k]; i++)
            assert(mulib_textcursor_prev(&b, i) == edges[k - 1]);
    }
    assert(mulib_textcursor_next(&b, SIZE_MAX) == b.n);
    assert(mulib_textcursor_prev(&b, SIZE_MAX) == 28);
    assert(mulib_textcursor_prev(&b, 0) == 0);
    assert(mulib_textcursor_next(NULL, 1) == 0);
    assert(mulib_textcursor_prev(NULL, 1) == 0);
    assert(mulib_textselection_set(&s, &b, 0, 3));
    old = s;
    assert(!mulib_textselection_set(&s, &b, 2, 3));
    assert(!mulib_textselection_set(&s, &b, 0, SIZE_MAX));
    assert(!memcmp(&s, &old, sizeof s));
    mulib_textselection_move(&s, &b, -1, 0);
    assert(s.anchor == 0 && s.caret == 0);
    mulib_textselection_move(&s, &b, 1, 1);
    assert(s.anchor == 0 && s.caret == 3);
    mulib_textselection_move(&s, &b, 1, 1);
    assert(s.anchor == 0 && s.caret == 5);
    mulib_textselection_move(&s, &b, 1, 0);
    assert(s.anchor == 5 && s.caret == 5);
    assert(mulib_textselection_set(&s, &b, 16, 3));
    mulib_textselection_move(&s, &b, 1, 0);
    assert(s.anchor == 16 && s.caret == 16);
    s.anchor = 2;
    old = s;
    mulib_textselection_move(&s, &b, -1, 0);
    assert(!memcmp(&s, &old, sizeof s));
    s = (mulib_textselection_t){1, 1}; /* Valid scalar edge within a cluster. */
    mulib_textselection_move(&s, &b, 1, 0);
    assert(s.anchor == 3 && s.caret == 3);
    old = s;
    mulib_textselection_move(&s, &b, 0, 0);
    assert(!memcmp(&s, &old, sizeof s));
    b.data[0] = (char)0x80;
    assert(!mulib_textselection_valid(&b, s));
    assert(mulib_textcursor_next(&b, 1) == 0);
    assert(mulib_textcursor_prev(&b, 1) == 0);
    b.data[0] = 'A';
    mulib_textbuffer_free(&b);
}

static void record_string(mulib_texthistory_t *h, mulib_textbuffer_t *b,
                          const char *s)
{
    size_t n = strlen(s);
    assert(mulib_textbuffer_set(b, s, n));
    assert(mulib_texthistory_record(h, b, (mulib_textselection_t){n, n}));
    check_history(h);
}

static void test_history_limits(void)
{
    mulib_texthistory_t h = {0}, old;
    mulib_textbuffer_t b = {0};
    mulib_textselection_t s = {0, 0};
    assert(mulib_texthistory_set_limits(&h, 0, 9));
    record_string(&h, &b, "");
    record_string(&h, &b, "a");
    record_string(&h, &b, "ab");
    record_string(&h, &b, "abc");
    assert(h.n == 3 && h.bytes == 9 && h.pos == 2);
    expect_text(&h.v[0].text, "a", 1);
    assert(mulib_texthistory_step(&h, &b, &s, 0));
    expect_text(&b, "ab", 2);
    record_string(&h, &b, "d");
    assert(h.n == 3 && h.bytes == 7 && h.pos == 2);
    assert(!mulib_texthistory_step(&h, &b, &s, 1));
    assert(mulib_texthistory_set_limits(&h, 2, 9));
    assert(h.n == 2 && h.bytes == 5 && h.pos == 1);
    assert(mulib_texthistory_set_limits(&h, 1, 2));
    assert(h.n == 1 && h.bytes == 2 && h.pos == 0);
    old = h;
    assert(!mulib_texthistory_set_limits(&h, 4, 1));
    assert(!memcmp(&h, &old, sizeof h));
    assert(mulib_textbuffer_set(&b, "xx", 2));
    assert(!mulib_texthistory_record(&h, &b, (mulib_textselection_t){2, 2}));
    assert(!memcmp(&h, &old, sizeof h));
    mulib_texthistory_free(&h);

    record_string(&h, &b, "a");
    record_string(&h, &b, "bb");
    record_string(&h, &b, "ccc");
    record_string(&h, &b, "dddd");
    assert(mulib_texthistory_step(&h, &b, &s, 0));
    assert(mulib_texthistory_step(&h, &b, &s, 0));
    assert(h.pos == 1);
    assert(mulib_texthistory_set_limits(&h, 2, 7));
    assert(h.n == 2 && h.pos == 1 && h.bytes == 5);
    expect_text(&h.v[h.pos].text, "bb", 2);
    assert(mulib_texthistory_set_limits(&h, 2, 3));
    assert(h.n == 1 && h.pos == 0 && h.bytes == 3);
    check_history(&h);
    mulib_texthistory_free(&h);

    for (size_t i = 0; i < 8; i++) record_string(&h, &b, "x");
    /* Source may live inside the vector that record reallocates. */
    assert(mulib_texthistory_record(&h, &h.v[0].text, h.v[0].selection));
    assert(h.n == 9 && h.bytes == 18);
    check_history(&h);
    h.limit = 1; /* Even a direct lowering is fully enforced on the next record. */
    record_string(&h, &b, "y");
    assert(h.n == 1 && h.bytes == 2);
    old = h;
    assert(!mulib_texthistory_record(&h, &b, (mulib_textselection_t){2, 2}));
    assert(!memcmp(&h, &old, sizeof h));
    assert(!mulib_texthistory_step(&h, &b, NULL, 0));
    mulib_texthistory_free(&h);
    for (size_t i = 0; i < 70; i++) {
        char text[3] = {(char)('0' + i / 10), (char)('0' + i % 10), 0};
        record_string(&h, &b, text);
    }
    assert(h.n == 64 && h.bytes == 64 * 3);
    expect_text(&h.v[0].text, "06", 2);
    expect_text(&h.v[h.pos].text, "69", 2);
    mulib_texthistory_free(&h);
    record_string(&h, &b, "\xc3\xa9");
    record_string(&h, &b, "x");
    s = (mulib_textselection_t){1, 1};
    h.v[0].selection.caret = 1; /* A corrupt target must not reach live text. */
    assert(!mulib_texthistory_step(&h, &b, &s, 0));
    assert(h.pos == 1 && s.anchor == 1 && s.caret == 1);
    expect_text(&b, "x", 1);
    h.v[0].selection.caret = 2;
    h.v[0].text.data[0] = (char)0x80;
    assert(!mulib_texthistory_step(&h, &b, &s, 0));
    assert(h.pos == 1 && s.anchor == 1 && s.caret == 1);
    expect_text(&b, "x", 1);
    h.v[0].text.data[0] = (char)0xc3;
    assert(mulib_texthistory_step(&h, &b, &s, 0));
    expect_text(&b, "\xc3\xa9", 2);
    mulib_textbuffer_free(&b);
    mulib_texthistory_free(&h);
    mulib_texthistory_free(NULL);
}

static void test_editor(void)
{
    mulib_texteditor_t e;
    editor_image old;
    size_t n;
    assert(mulib_texteditor_init(&e, "a\xc3\xa9z", 4));
    assert(mulib_textselection_set(&e.selection, &e.text, 3, 1));
    assert(mulib_texteditor_insert(&e, "X", 1));
    expect_text(&e.text, "aXz", 3);
    assert(mulib_texteditor_undo(&e, 0));
    expect_text(&e.text, "a\xc3\xa9z", 4);
    assert(e.selection.anchor == 3 && e.selection.caret == 1);
    assert(mulib_texteditor_delete(&e, 1));
    expect_text(&e.text, "az", 2);
    assert(mulib_texteditor_undo(&e, 0));
    assert(e.selection.anchor == 3 && e.selection.caret == 1);
    assert(mulib_texteditor_undo(&e, 1));
    expect_text(&e.text, "az", 2);
    assert(!mulib_texteditor_undo(&e, 1)); /* Old X branch was discarded. */
    assert(mulib_textselection_set(&e.selection, &e.text, 2, 2));
    assert(mulib_texteditor_delete(&e, 1));
    expect_text(&e.text, "a", 1);
    assert(mulib_texteditor_undo(&e, 0));
    assert(e.selection.anchor == 2 && e.selection.caret == 2);
    assert(mulib_textselection_set(&e.selection, &e.text, 0, 0));
    n = e.history.n;
    assert(mulib_texteditor_delete(&e, 1));
    assert(mulib_texteditor_insert(&e, NULL, 0));
    assert(e.history.n == n && e.history.pos + 1 < n);
    assert(mulib_texteditor_undo(&e, 1)); /* No-op kept redo. */
    assert(mulib_texteditor_undo(&e, 0));
    assert(e.selection.anchor == 0 && e.selection.caret == 0);
    assert(mulib_texteditor_delete(&e, 0));
    expect_text(&e.text, "z", 1);
    check_editor(&e);
    save_editor(&old, &e);
    assert(!mulib_texteditor_insert(&e, "\xc0\x80", 2));
    assert(!mulib_texteditor_insert(&e, "", SIZE_MAX));
    assert(!mulib_texteditor_replace(&e, 2, 2, "x", 1));
    unchanged_editor(&old, &e);
    mulib_texteditor_free(&e);

    assert(mulib_texteditor_init(&e, "a\xc3\xa9z", 4));
    e.selection.caret = 2;
    save_editor(&old, &e);
    assert(!mulib_texteditor_insert(&e, "x", 1));
    assert(!mulib_texteditor_delete(&e, 1));
    assert(!mulib_texteditor_delete(&e, 0));
    assert(!mulib_texteditor_undo(&e, 0));
    unchanged_editor(&old, &e);
    e.selection.caret = e.text.n;
    assert(!mulib_texteditor_replace(&e, 0, 2, "x", 1));
    assert(mulib_texteditor_insert(&e, e.text.data + 1, 2));
    expect_text(&e.text, "a\xc3\xa9z\xc3\xa9", 6);
    check_editor(&e);
    mulib_texteditor_free(&e);

    /* Direct nested-buffer edits must not manufacture invalid undo selections. */
    assert(mulib_texteditor_init(&e, "a", 1));
    assert(mulib_textbuffer_set(&e.text, "different text", 14));
    e.selection = (mulib_textselection_t){14, 14};
    assert(!mulib_texteditor_valid(&e));
    save_editor(&old, &e);
    assert(!mulib_texteditor_insert(&e, "x", 1));
    assert(!mulib_texteditor_undo(&e, 0));
    unchanged_editor(&old, &e);
    mulib_texteditor_free(&e);

    assert(!mulib_texteditor_init_with_limits(&e, "", SIZE_MAX, 0, SIZE_MAX));
    assert(!e.text.data && !e.history.v);
    assert(!mulib_texteditor_init_with_limits(&e, "", 10, 0, 3));
    assert(!e.text.data && !e.history.v);
    assert(mulib_texteditor_init_with_limits(&e, "abc", 3, 0, 4));
    save_editor(&old, &e);
    assert(!mulib_texteditor_insert(&e, "d", 1));
    unchanged_editor(&old, &e);
    assert(mulib_texthistory_set_limits(&e.history, 0, 8));
    assert(mulib_texteditor_insert(&e, "d", 1));
    assert(e.history.n == 1 && e.history.bytes == 5); /* 4 + 5 exceeds 8. */
    assert(!mulib_texteditor_undo(&e, 0));
    check_editor(&e);
    mulib_texteditor_free(&e);
    assert(mulib_texteditor_init_with_limits(&e, NULL, 0, 1, 1));
    assert(e.history.n == 1 && e.history.bytes == 1);
    save_editor(&old, &e);
    assert(!mulib_texteditor_insert(&e, "x", 1));
    unchanged_editor(&old, &e);
    mulib_texteditor_free(&e);
    mulib_texteditor_free(NULL);
}

static uint32_t random_u32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Deterministic state-machine stress, with independently constructed edit text. */
static void test_editor_model(void)
{
    static const struct { const char *s; size_t n; int valid; } pieces[] = {
        {"x", 1, 1}, {"\xc3\xa9", 2, 1}, {"\xf0\x9f\x98\x80", 4, 1},
        {"\xcc\x81", 2, 1}, {"\r\n", 2, 1}, {"\xe2\x80\x8d", 3, 1},
        {"\xf0\x9f\x87\xa6", 4, 1}, {NULL, 0, 1}, {"\xed\xa0\x80", 3, 0}
    };
    mulib_texteditor_t e;
    uint32_t rng = UINT32_C(0x1bb0cafe);
    assert(mulib_texteditor_init_with_limits(&e, "", 0, 8, 64));
    for (size_t step = 0; step < 2000; step++) {
        unsigned op = random_u32(&rng) % 4;
        editor_image old;
        if (op < 2) {
            size_t offsets[256], count = 0, start, end, p, len, anchor, caret;
            char expected[256];
            int should_succeed, ok;
            for (size_t i = 0; i <= e.text.n; i++)
                if (mgx_utf8_boundary(e.text.data, e.text.n, i))
                    offsets[count++] = i;
            assert(count);
            anchor = offsets[random_u32(&rng) % count];
            caret = offsets[random_u32(&rng) % count];
            assert(mulib_textselection_set(&e.selection, &e.text, anchor, caret));
            p = random_u32(&rng) % (sizeof pieces / sizeof *pieces);
            start = mulib_textselection_start(e.selection);
            end = mulib_textselection_end(e.selection);
            len = e.text.n - (end - start) + pieces[p].n;
            should_succeed = pieces[p].valid &&
                len + 1 <= mgx_texthistory_byte_limit(e.history.byte_limit);
            assert(len < sizeof expected);
            if (start) memcpy(expected, e.text.data, start);
            if (pieces[p].n) memcpy(expected + start, pieces[p].s, pieces[p].n);
            if (e.text.n > end)
                memcpy(expected + start + pieces[p].n, e.text.data + end,
                       e.text.n - end);
            save_editor(&old, &e);
            ok = mulib_texteditor_insert(&e, pieces[p].s, pieces[p].n);
            assert(ok == should_succeed);
            if (!ok) unchanged_editor(&old, &e);
            else {
                expect_text(&e.text, expected, len);
                assert(e.selection.anchor == start + pieces[p].n);
                assert(e.selection.caret == start + pieces[p].n);
            }
        } else if (op == 2) {
            int redo = (int)(random_u32(&rng) & 1);
            int possible = redo ? e.history.pos < e.history.n - 1 :
                                  e.history.pos > 0;
            save_editor(&old, &e);
            assert(mulib_texteditor_undo(&e, redo) == possible);
            if (!possible) unchanged_editor(&old, &e);
            else {
                size_t p = redo ? old.editor.history.pos + 1 :
                                  old.editor.history.pos - 1;
                expect_text(&e.text, old.contents[p], old.snapshots[p].text.n);
                assert(!memcmp(&e.selection, &old.snapshots[p].selection,
                               sizeof e.selection));
            }
        } else {
            size_t budget = 1 + random_u32(&rng) % 64;
            size_t limit = 1 + random_u32(&rng) % 8;
            int possible = e.text.n + 1 <= budget;
            save_editor(&old, &e);
            assert(mulib_texthistory_set_limits(&e.history, limit, budget) ==
                   possible);
            if (!possible) unchanged_editor(&old, &e);
            else expect_text(&e.text, old.text, old.editor.text.n);
        }
        check_editor(&e);
    }
    mulib_texteditor_free(&e);
    assert(!live);
}

static void test_editor_failures(void)
{
    for (size_t fail = 1; ; fail++) {
        mulib_texteditor_t e;
        int ok;
        assert(fail < 16 && !live);
        calls = 0;
        fail_at = fail;
        ok = mulib_texteditor_init(&e, "initial", 7);
        fail_at = 0;
        if (!ok) assert(!e.text.data && !e.history.v && !live);
        else check_editor(&e);
        mulib_texteditor_free(&e);
        assert(!live);
        if (ok) break;
    }

    /* Includes vector growth, redo-branch replacement, deletion and both steps. */
    for (unsigned kind = 0; kind < 6; kind++) {
        for (size_t fail = 1; ; fail++) {
            mulib_texteditor_t e;
            editor_image old;
            size_t before;
            int ok;
            assert(fail < 32 && !live);
            assert(mulib_texteditor_init(&e, "a\xc3\xa9", 3));
            for (size_t i = 0; i < 7; i++)
                assert(mulib_texteditor_insert(&e, "x", 1));
            assert(e.history.n == 8);
            if (kind == 1 || kind == 4) assert(mulib_texteditor_undo(&e, 0));
            if (kind == 2)
                assert(mulib_textselection_set(&e.selection, &e.text, 3, 1));
            save_editor(&old, &e);
            before = live;
            calls = 0;
            fail_at = fail;
            if (kind == 0 || kind == 1)
                ok = mulib_texteditor_insert(&e, "large insertion", 15);
            else if (kind == 2) ok = mulib_texteditor_delete(&e, 1);
            else if (kind == 3) ok = mulib_texteditor_undo(&e, 0);
            else if (kind == 4) ok = mulib_texteditor_undo(&e, 1);
            else ok = mulib_texthistory_record(&e.history, &e.text, e.selection);
            fail_at = 0;
            if (!ok) {
                unchanged_editor(&old, &e);
                assert(live == before);
            } else check_editor(&e);
            mulib_texteditor_free(&e);
            assert(!live);
            if (ok) break;
        }
    }

    /* A failed restoration may need both a source copy and destination growth. */
    for (size_t fail = 1; ; fail++) {
        mulib_texthistory_t h = {0};
        mulib_textbuffer_t b = {0}, old;
        mulib_textselection_t s = {0, 0};
        size_t before;
        int ok;
        assert(fail < 10);
        record_string(&h, &b, "a very long initial snapshot");
        record_string(&h, &b, "x");
        mulib_textbuffer_free(&b);
        assert(mulib_textbuffer_set(&b, "x", 1));
        old = b;
        before = live;
        calls = 0;
        fail_at = fail;
        ok = mulib_texthistory_step(&h, &b, &s, 0);
        fail_at = 0;
        if (!ok) {
            assert(h.pos == 1 && s.anchor == 0 && s.caret == 0);
            assert(!memcmp(&b, &old, sizeof b) && live == before);
            expect_text(&b, "x", 1);
        } else expect_text(&b, "a very long initial snapshot", 28);
        mulib_textbuffer_free(&b);
        mulib_texthistory_free(&h);
        assert(!live);
        if (ok) break;
    }
}

static void test_richtext(void)
{
    mulib_richtext_t r = {0}, old;
    mgx_richspan span = {1, 3, 0xff123456, 1};
    assert(mulib_richtext_set(&r, "a\xc3\xa9z", 4));
    assert(mulib_richtext_span(&r, span));
    assert(mulib_richtext_at(&r, 1).color == span.color);
    assert(mulib_richtext_at(&r, 2).color == 0xffffffff);
    assert(mulib_richtext_at(&r, SIZE_MAX).color == 0xffffffff);
    old = r;
    span.end = 2;
    assert(!mulib_richtext_span(&r, span));
    span.end = SIZE_MAX;
    assert(!mulib_richtext_span(&r, span));
    assert(!mulib_richtext_set(&r, "\xff", 1));
    assert(!memcmp(&old, &r, sizeof r));
    assert(mulib_richtext_span(&r, (mgx_richspan){0, 4, 0xffabcdef, 2}));
    assert(mulib_richtext_at(&r, 1).color == 0xffabcdef);
    assert(mulib_richtext_set(&r, r.text.data + 1, 2));
    assert(!r.n);
    expect_text(&r.text, "\xc3\xa9", 2);
    assert(mulib_richtext_at(NULL, 0).end == 0);
    assert(!mulib_richtext_span(NULL, span));
    mulib_richtext_free(&r);

    for (unsigned kind = 0; kind < 2; kind++) {
        for (size_t fail = 1; ; fail++) {
            size_t before;
            int ok;
            assert(fail < 10);
            assert(mulib_richtext_set(&r, "a", 1));
            if (kind)
                assert(mulib_richtext_span(&r, (mgx_richspan){0, 1, 7, 2}));
            old = r;
            before = live;
            calls = 0;
            fail_at = fail;
            ok = kind ? mulib_richtext_set(&r, "long replacement", 16) :
                mulib_richtext_span(&r, (mgx_richspan){0, 1, 1, 0});
            fail_at = 0;
            if (!ok) {
                assert(!memcmp(&old, &r, sizeof r) && live == before);
                expect_text(&r.text, "a", 1);
                if (kind) assert(r.v[0].color == 7 && r.v[0].flags == 2);
            }
            mulib_richtext_free(&r);
            assert(!live);
            if (ok) break;
        }
    }
    mulib_richtext_free(NULL);
}

typedef struct {
    mulib_gui_undo_t *stack;
    int *value;
    int delta, fail_apply, fail_revert, released, applies, reverts, reenter;
} command;

static int command_apply(void *p);
static int command_revert(void *p);
static void command_release(void *p);

static mgui_undo_cmd make_command(command *c)
{
    mgui_undo_cmd out = {command_apply, command_revert, command_release, c};
    return out;
}

static void check_reentry(command *c)
{
    if (!c->reenter) return;
    assert(c->stack->busy);
    assert(!mulib_gui_undo_push(c->stack, make_command(c)));
    assert(!mulib_gui_undo_step(c->stack, 0));
    assert(!mulib_gui_undo_step(c->stack, 1));
    mulib_gui_undo_free(c->stack);
    assert(c->stack->busy);
}

static int command_apply(void *p)
{
    command *c = (command *)p;
    c->applies++;
    check_reentry(c);
    if (c->fail_apply) return 0;
    *c->value += c->delta;
    return 1;
}

static int command_revert(void *p)
{
    command *c = (command *)p;
    c->reverts++;
    check_reentry(c);
    if (c->fail_revert) return 0;
    *c->value -= c->delta;
    return 1;
}

static void command_release(void *p)
{
    command *c = (command *)p;
    check_reentry(c);
    assert(!c->released);
    c->released++;
}

static void test_command_undo(void)
{
    mulib_gui_undo_t u = {0}, old;
    int value = 0;
    command a = {&u, &value, 1, 0, 0, 0, 0, 0, 1};
    command b = {&u, &value, 10, 0, 0, 0, 0, 0, 1};
    command c = {&u, &value, 100, 1, 0, 0, 0, 0, 1};
    old = u;
    assert(!mulib_gui_undo_push(&u, make_command(&c)));
    assert(!memcmp(&u, &old, sizeof u) && !c.released && value == 0 && !live);
    calls = 0;
    fail_at = 1;
    assert(!mulib_gui_undo_push(&u, make_command(&a)));
    fail_at = 0;
    assert(!memcmp(&u, &old, sizeof u) && !a.applies && !a.released);
    assert(mulib_gui_undo_push(&u, make_command(&a)));
    assert(mulib_gui_undo_push(&u, make_command(&b)));
    assert(value == 11);
    assert(mulib_gui_undo_step(&u, 0) && value == 1);
    old = u;
    assert(!mulib_gui_undo_push(&u, make_command(&c)));
    assert(!memcmp(&u, &old, sizeof u) && !b.released && !c.released && value == 1);
    assert(mulib_gui_undo_step(&u, 1) && value == 11);
    assert(mulib_gui_undo_step(&u, 0) && value == 1);
    c.fail_apply = 0;
    assert(mulib_gui_undo_push(&u, make_command(&c)) && value == 101);
    assert(b.released == 1);
    old = u;
    c.fail_revert = 1;
    assert(!mulib_gui_undo_step(&u, 0) && value == 101);
    assert(!memcmp(&u, &old, sizeof u));
    c.fail_revert = 0;
    assert(mulib_gui_undo_step(&u, 0) && value == 1);
    c.fail_apply = 1;
    old = u;
    assert(!mulib_gui_undo_step(&u, 1) && value == 1);
    assert(!memcmp(&u, &old, sizeof u));
    c.fail_apply = 0;
    assert(mulib_gui_undo_step(&u, 1));
    mulib_gui_undo_free(&u);
    assert(a.released == 1 && b.released == 1 && c.released == 1 && !live);

    /* Failed apply after a staged vector growth must preserve allocation/cap. */
    {
        command commands[9] = {{0}};
        size_t before;
        value = 0;
        for (size_t i = 0; i < 9; i++) {
            commands[i].stack = &u;
            commands[i].value = &value;
            commands[i].delta = 1;
            if (i < 8) assert(mulib_gui_undo_push(&u, make_command(&commands[i])));
        }
        old = u;
        before = live;
        commands[8].fail_apply = 1;
        assert(!mulib_gui_undo_push(&u, make_command(&commands[8])));
        assert(!memcmp(&u, &old, sizeof u) && live == before && value == 8);
        commands[8].fail_apply = 0;
        calls = 0;
        fail_at = 1;
        assert(!mulib_gui_undo_push(&u, make_command(&commands[8])));
        fail_at = 0;
        assert(!memcmp(&u, &old, sizeof u) && live == before && value == 8);
        assert(mulib_gui_undo_push(&u, make_command(&commands[8])));
        assert(value == 9);
        mulib_gui_undo_free(&u);
        for (size_t i = 0; i < 9; i++) assert(commands[i].released == 1);
    }
    mulib_gui_undo_free(NULL);
    assert(!live);
}

int main(void)
{
    test_utf8();
    test_buffer();
    test_cursor_selection();
    test_history_limits();
    test_editor();
    test_editor_model();
    test_editor_failures();
    test_richtext();
    test_command_undo();
    assert(!live);
    puts("gfx text: UTF-8, boundaries, byte caps and failure atomicity passed");
    return 0;
}
