/**
 * @file mulib-imtui-mune.h
 * @brief Frame-scoped Mune bindings for terminal immediate-mode UI.
 * Define MULIB_IMPL_IMTUI_MUNE in one translation unit. Implement
 * IMTUI_CORE, IMTUI_DRAW, IMTUI_WIDGETS, IMTUI_LAYOUT, IMTUI_SYNTAX and
 * MUNE with their dependencies. This is not a Lua sandbox: Mune's normal
 * standard libraries (including I/O) are still installed.
 */
#ifndef MULIB_IMTUI_MUNE_H
#define MULIB_IMTUI_MUNE_H
#include "mulib-imtui-layout.h"
#include "mulib-imtui-syntax.h"
#include "mulib-imtui-widgets.h"
#include "mulib-mune.h"
#ifdef __cplusplus
extern "C" {
#endif

/** Zero-initialize before bind. Do not modify or move while the VM exists.
 * Destroy the VM before releasing this storage. The context is borrowed only
 * during synchronous run/call and need not outlive them. Do not nest bridge
 * calls or share a VM/context concurrently. */
typedef struct mulib_imtui_mune {
    mulib_mune_state_t *state;
    mulib_imtui_t *frame;
    size_t bytes_left;
    size_t work_left;
} mulib_imtui_mune_t;

/** Install the native table `imtui_native` in a state, once, outside execution.
 * The table is published only when complete. An existing non-nil global or
 * an already-bound bridge is an error. Ordinary Mune execution can load
 * library/imtui before the first frame but cannot call the native functions.
 *
 * Native functions (all require active bridge execution):
 * - bounds() -> x, y, w, h
 * - row(x,y,w,h,gap,count,index) and column(...) -> x, y, w, h (one-based)
 * - text(x,y,bytes[,foreground,background,attributes])
 * - button(key,x,y,w,h,label[,enabled]) -> clicked
 * - checkbox(key,x,y,w,h,label,value[,enabled]) -> value, changed
 * - code(x,y,w,h,language,bytes[,first_line]); first_line is one-based.
 *   Language is "plain", "c", "cpp", "python", "json" or "shell". This
 *   builds a temporary syntax cache; C callers can retain one across frames
 *   with mulib_imtui_syntax_draw.
 * Coordinates and styling numbers must be integers. Dimensions/gaps are
 * nonnegative; keys are nonempty strings of at most 256 bytes. Use unique
 * stable keys per frame; no 64-bit widget ID is converted to a Lua number.
 * Colors follow mulib_term_style_t encoding; nil style fields use defaults.
 * Values and enabled flags are booleans; nil enabled means true.
 * Text/labels must be UTF-8, up to 4096 bytes; code accepts up to 64 KiB of
 * arbitrary bytes. Combined input to drawing/widgets is limited to 128 KiB
 * per run/call; a separate budget of 8 * MULIB_IMTUI_MAX_CELLS counts four
 * units per clipped widget cell and min(clipped width, line size) units per
 * source byte on each line of a code preview. Invalid arguments report a
 * Mune runtime error; exceeding the budgets reports a Mune limit error. */
mulib_mune_status_t mulib_imtui_mune_bind(mulib_mune_state_t *, mulib_imtui_mune_t *);

/** Execute inside a host-owned open frame. No GUI references remain live in
 * the bridge on return, even on error. Prior drawing/Lua side effects are not
 * rolled back. Result ownership is the same as in Mune: *result must be NULL. */
mulib_mune_status_t mulib_imtui_mune_run(mulib_imtui_mune_t *, mulib_imtui_t *,
    const char *source, size_t length, mulib_mune_result_t **result, mulib_mune_error_t *);

/** Like run, but call a retained Lua function without reparsing frame source. */
mulib_mune_status_t mulib_imtui_mune_call(mulib_imtui_mune_t *, mulib_imtui_t *,
    mulib_mune_value_t function, const mulib_mune_value_t *args, size_t count,
    mulib_mune_result_t **result, mulib_mune_error_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMTUI_MUNE) && !defined(MULIB_IMTUI_MUNE_IMPLEMENTED)
#define MULIB_IMTUI_MUNE_IMPLEMENTED
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

static int mulib_imtui_mune_int_(mulib_mune_value_t v, int *out) {
    if (v.type != MULIB_MUNE_INTEGER || v.as.integer < INT_MIN || v.as.integer > INT_MAX)
        return 0;
    *out = (int)v.as.integer;
    return 1;
}

static int mulib_imtui_mune_rect_(const mulib_mune_value_t *a, mulib_imtui_rect_t *r) {
    return mulib_imtui_mune_int_(a[0], &r->x) && mulib_imtui_mune_int_(a[1], &r->y) &&
        mulib_imtui_mune_int_(a[2], &r->w) && mulib_imtui_mune_int_(a[3], &r->h) &&
        r->w >= 0 && r->h >= 0 && r->x <= INT_MAX - r->w && r->y <= INT_MAX - r->h;
}

static mulib_imtui_t *mulib_imtui_mune_frame_(mulib_mune_state_t *s, void *user) {
    mulib_imtui_mune_t *b = (mulib_imtui_mune_t *)user;
    return b->state == s && b->frame && b->frame->in_frame ? b->frame : NULL;
}

static mulib_mune_status_t mulib_imtui_mune_push_rect_(mulib_mune_result_t *out,
                                                     mulib_imtui_rect_t r) {
    return mulib_mune_result_push(out, mulib_mune_integer(r.x)) &&
           mulib_mune_result_push(out, mulib_mune_integer(r.y)) &&
           mulib_mune_result_push(out, mulib_mune_integer(r.w)) &&
           mulib_mune_result_push(out, mulib_mune_integer(r.h)) ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}

static mulib_mune_status_t mulib_imtui_mune_bounds_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user) {
    mulib_imtui_t *ctx = mulib_imtui_mune_frame_(s, user);
    (void)a;
    if (!ctx || n) return MULIB_MUNE_RUNTIME;
    return mulib_imtui_mune_push_rect_(out, mulib_imtui_bounds(ctx));
}

static mulib_mune_status_t mulib_imtui_mune_split_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user, int row) {
    mulib_imtui_rect_t r, part;
    int gap, count, index;
    if (!mulib_imtui_mune_frame_(s, user) || n != 7 || !mulib_imtui_mune_rect_(a, &r) ||
        !mulib_imtui_mune_int_(a[4], &gap) || gap < 0 ||
        !mulib_imtui_mune_int_(a[5], &count) || count < 1 ||
        !mulib_imtui_mune_int_(a[6], &index) || index < 1 || index > count ||
        (uint64_t)count != (size_t)count)
        return MULIB_MUNE_RUNTIME;
    if (!(row ? mulib_imtui_layout_row(r, gap, (size_t)count, (size_t)index - 1, &part)
              : mulib_imtui_layout_column(r, gap, (size_t)count, (size_t)index - 1, &part)))
        return MULIB_MUNE_RUNTIME;
    return mulib_imtui_mune_push_rect_(out, part);
}

static mulib_mune_status_t mulib_imtui_mune_row_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user) {
    return mulib_imtui_mune_split_(s, a, n, out, user, 1);
}

static mulib_mune_status_t mulib_imtui_mune_column_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user) {
    return mulib_imtui_mune_split_(s, a, n, out, user, 0);
}

static int mulib_imtui_mune_color_(mulib_mune_value_t v, uint32_t *color) {
    if (v.type == MULIB_MUNE_NIL) return 1;
    if (v.type != MULIB_MUNE_INTEGER || v.as.integer < 0 || v.as.integer > UINT32_MAX)
        return 0;
    *color = (uint32_t)v.as.integer;
    return *color <= 255 || (*color >> 24) == 1 || *color == MULIB_TERM_COLOR_DEFAULT;
}

static int mulib_imtui_mune_charge_(void *user, size_t n) {
    mulib_imtui_mune_t *b = (mulib_imtui_mune_t *)user;
    if (n > b->bytes_left) return 0;
    b->bytes_left -= n;
    return 1;
}

static mulib_imtui_rect_t mulib_imtui_mune_clip_(mulib_imtui_rect_t r, mulib_imtui_rect_t clip) {
    mulib_imtui_rect_t out = {0, 0, 0, 0};
    int left = r.x > clip.x ? r.x : clip.x, top = r.y > clip.y ? r.y : clip.y;
    int right = r.x + r.w, bottom = r.y + r.h;
    if (right > clip.x + clip.w) right = clip.x + clip.w;
    if (bottom > clip.y + clip.h) bottom = clip.y + clip.h;
    if (left < right && top < bottom) {
        out.x = left;
        out.y = top;
        out.w = right - left;
        out.h = bottom - top;
    }
    return out;
}

static int mulib_imtui_mune_work_(mulib_imtui_mune_t *b, size_t count, size_t unit) {
    if (unit && count > b->work_left / unit) return 0;
    b->work_left -= count * unit;
    return 1;
}

static mulib_mune_status_t mulib_imtui_mune_text_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user) {
    mulib_imtui_t *ctx = mulib_imtui_mune_frame_(s, user);
    mulib_term_style_t style = mulib_term_style_default();
    const char *text;
    size_t len;
    int x, y, attr = 0;
    (void)out;
    if (!ctx || n < 3 || n > 6 || !mulib_imtui_mune_int_(a[0], &x) ||
        !mulib_imtui_mune_int_(a[1], &y) ||
        !(text = mulib_mune_value_string(a[2], &len)) || len > 4096 ||
        (n >= 4 && !mulib_imtui_mune_color_(a[3], &style.foreground)) ||
        (n >= 5 && !mulib_imtui_mune_color_(a[4], &style.background)) ||
        (n >= 6 && a[5].type != MULIB_MUNE_NIL &&
         (!mulib_imtui_mune_int_(a[5], &attr) || attr < 0 || attr > 255)))
        return MULIB_MUNE_RUNTIME;
    if (!mulib_imtui_mune_charge_(user, len)) return MULIB_MUNE_LIMIT;
    if (!mulib_utf8_validate((const unsigned char *)text, len)) return MULIB_MUNE_RUNTIME;
    style.attributes = (unsigned)attr;
    return mulib_imtui_text(ctx, x, y, text, len, style) ? MULIB_MUNE_OK : MULIB_MUNE_RUNTIME;
}

static mulib_mune_status_t mulib_imtui_mune_widget_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user, int check) {
    mulib_imtui_t *ctx = mulib_imtui_mune_frame_(s, user);
    size_t key_len, len, required = check ? 7 : 6;
    const char *key, *label;
    mulib_imtui_rect_t r, visible;
    int enabled = 1, value = 0, clicked = 0;
    if (!ctx || n < required || n > required + 1 ||
        !(key = mulib_mune_value_string(a[0], &key_len)) || !key_len || key_len > 256 ||
        !mulib_imtui_mune_rect_(a + 1, &r) ||
        !(label = mulib_mune_value_string(a[5], &len)) || len > 4096 ||
        (check && a[6].type != MULIB_MUNE_BOOLEAN))
        return MULIB_MUNE_RUNTIME;
    if (n > required && a[required].type != MULIB_MUNE_NIL) {
        if (a[required].type != MULIB_MUNE_BOOLEAN) return MULIB_MUNE_RUNTIME;
        enabled = a[required].as.boolean;
    }
    if (!mulib_imtui_mune_charge_(user, len + key_len)) return MULIB_MUNE_LIMIT;
    if (!mulib_utf8_validate((const unsigned char *)label, len)) return MULIB_MUNE_RUNTIME;
    if (check) value = a[6].as.boolean;
    visible = mulib_imtui_mune_clip_(r, mulib_imtui_clip(ctx));
    if (visible.w) {
        mulib_imtui_id_t id;
        if (!mulib_imtui_mune_work_((mulib_imtui_mune_t *)user, (size_t)visible.w * visible.h, 4))
            return MULIB_MUNE_LIMIT;
        id = mulib_imtui_id(ctx, key, key_len);
        if (!id) return MULIB_MUNE_RUNTIME;
        clicked = check ? mulib_imtui_checkbox(ctx, id, r, label, len, &value, enabled)
                        : mulib_imtui_button(ctx, id, r, label, len, enabled);
    }
    if (check && !mulib_mune_result_push(out, mulib_mune_boolean(value))) return MULIB_MUNE_MEMORY;
    return mulib_mune_result_push(out, mulib_mune_boolean(clicked)) ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}

static mulib_mune_status_t mulib_imtui_mune_button_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user) {
    return mulib_imtui_mune_widget_(s, a, n, out, user, 0);
}

static mulib_mune_status_t mulib_imtui_mune_checkbox_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user) {
    return mulib_imtui_mune_widget_(s, a, n, out, user, 1);
}

static mulib_mune_status_t mulib_imtui_mune_code_(mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t n, mulib_mune_result_t *out, void *user) {
    static const struct { const char *name; mulib_termsyn_language_t lang; } languages[] = {
        {"plain", MULIB_TERMSYN_PLAIN}, {"c", MULIB_TERMSYN_C}, {"cpp", MULIB_TERMSYN_CPP},
        {"python", MULIB_TERMSYN_PYTHON}, {"json", MULIB_TERMSYN_JSON}, {"shell", MULIB_TERMSYN_SHELL}
    };
    mulib_imtui_t *ctx = mulib_imtui_mune_frame_(s, user);
    mulib_imtui_rect_t r;
    const char *source, *language;
    mulib_edbuf_t *buffer;
    mulib_termsyn_t *syntax;
    size_t name_len, len, i, at, width;
    int first = 1, status = MULIB_TERMSYN_ENOMEM;
    (void)out;
    if (!ctx || n < 6 || n > 7 || !mulib_imtui_mune_rect_(a, &r) ||
        !(language = mulib_mune_value_string(a[4], &name_len)) ||
        !(source = mulib_mune_value_string(a[5], &len)) ||
        (n == 7 && a[6].type != MULIB_MUNE_NIL &&
         (!mulib_imtui_mune_int_(a[6], &first) || first < 1 || (uint64_t)first != (size_t)first)))
        return MULIB_MUNE_RUNTIME;
    for (i = 0; i < sizeof languages / sizeof languages[0]; ++i)
        if (name_len == strlen(languages[i].name) &&
            !memcmp(language, languages[i].name, name_len)) break;
    if (i == sizeof languages / sizeof languages[0]) return MULIB_MUNE_RUNTIME;
    if (len > 64u * 1024u || !mulib_imtui_mune_charge_(user, len)) return MULIB_MUNE_LIMIT;
    width = (size_t)mulib_imtui_mune_clip_(r, mulib_imtui_clip(ctx)).w;
    if (!width || !len) return MULIB_MUNE_OK;
    /* termsyn_at can scan a line's spans. Bound the work conservatively
     * using bytes per line times the maximum lookups for its visible cells. */
    for (at = 0; at < len;) {
        const char *lf = (const char *)memchr(source + at, '\n', len - at);
        size_t line = lf ? (size_t)(lf - (source + at)) + 1 : len - at;
        if (!mulib_imtui_mune_work_((mulib_imtui_mune_t *)user, line, width < line ? width : line))
            return MULIB_MUNE_LIMIT;
        at += line;
    }
    buffer = mulib_edbuf_create(MULIB_EDBUF_GAP, source, len);
    if (!buffer) return MULIB_MUNE_MEMORY;
    syntax = mulib_termsyn_create(languages[i].lang);
    if (syntax) status = mulib_imtui_syntax_draw(ctx, r, buffer, syntax, (size_t)first - 1, NULL);
    mulib_termsyn_destroy(syntax);
    mulib_edbuf_destroy(buffer);
    if (status == MULIB_TERMSYN_ENOMEM) return MULIB_MUNE_MEMORY;
    return status == MULIB_TERMSYN_OK ? MULIB_MUNE_OK : MULIB_MUNE_RUNTIME;
}

mulib_mune_status_t mulib_imtui_mune_bind(mulib_mune_state_t *s, mulib_imtui_mune_t *b) {
    static const struct { const char *name; mulib_mune_native_fn fn; } fields[] = {
        {"bounds", mulib_imtui_mune_bounds_}, {"row", mulib_imtui_mune_row_},
        {"column", mulib_imtui_mune_column_}, {"text", mulib_imtui_mune_text_},
        {"button", mulib_imtui_mune_button_}, {"checkbox", mulib_imtui_mune_checkbox_},
        {"code", mulib_imtui_mune_code_}
    };
    mulib_mune_result_t *table_root;
    mulib_mune_value_t table, key, fn;
    mulib_mune_status_t status;
    size_t i;
    if (!s || !b || b->state ||
        mulib_mune_get_global(s, "imtui_native", &table) != MULIB_MUNE_OK ||
        table.type != MULIB_MUNE_NIL) return MULIB_MUNE_INVALID;
    table_root = mulib_mune_result_create(s);
    if (!table_root) return MULIB_MUNE_MEMORY;
    status = mulib_mune_make_table(s, &table);
    if (status == MULIB_MUNE_OK && !mulib_mune_result_push(table_root, table)) status = MULIB_MUNE_MEMORY;
    for (i = 0; status == MULIB_MUNE_OK && i < sizeof fields / sizeof fields[0]; ++i) {
        status = mulib_mune_make_string(s, fields[i].name, strlen(fields[i].name), &key);
        if (status == MULIB_MUNE_OK) status = mulib_mune_make_function(s, fields[i].fn, b, &fn);
        if (status == MULIB_MUNE_OK) status = mulib_mune_raw_set(s, table, key, fn);
    }
    if (status == MULIB_MUNE_OK) status = mulib_mune_set_global(s, "imtui_native", table);
    mulib_mune_result_destroy(table_root);
    if (status == MULIB_MUNE_OK) b->state = s;
    return status;
}

static int mulib_imtui_mune_enter_(mulib_imtui_mune_t *b, mulib_imtui_t *ctx,
    mulib_mune_result_t **out, mulib_mune_error_t *error) {
    if (!b || !b->state || b->frame || !ctx || !ctx->in_frame || !out || *out) {
        if (error) {
            error->code = MULIB_MUNE_INVALID;
            error->offset = 0;
            error->message = "IMTUI Mune requires a bound bridge and an open frame";
        }
        return 0;
    }
    b->bytes_left = 128u * 1024u;
    b->work_left = 8u * MULIB_IMTUI_MAX_CELLS;
    b->frame = ctx;
    return 1;
}

static mulib_mune_status_t mulib_imtui_mune_leave_(mulib_imtui_mune_t *b, mulib_mune_status_t status) {
    b->frame = NULL;
    b->bytes_left = 0;
    b->work_left = 0;
    return status;
}

mulib_mune_status_t mulib_imtui_mune_run(mulib_imtui_mune_t *b, mulib_imtui_t *ctx,
    const char *source, size_t length, mulib_mune_result_t **out, mulib_mune_error_t *error) {
    if (!mulib_imtui_mune_enter_(b, ctx, out, error)) return MULIB_MUNE_INVALID;
    return mulib_imtui_mune_leave_(b, mulib_mune_run_source(b->state, source, length, out, error));
}

mulib_mune_status_t mulib_imtui_mune_call(mulib_imtui_mune_t *b, mulib_imtui_t *ctx,
    mulib_mune_value_t fn, const mulib_mune_value_t *args, size_t count,
    mulib_mune_result_t **out, mulib_mune_error_t *error) {
    if (!mulib_imtui_mune_enter_(b, ctx, out, error)) return MULIB_MUNE_INVALID;
    return mulib_imtui_mune_leave_(b, mulib_mune_call(b->state, fn, args, count, out, error));
}
#ifdef __cplusplus
}
#endif
#endif
