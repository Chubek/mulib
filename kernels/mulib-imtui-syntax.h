/**
 * @file mulib-imtui-syntax.h
 * @brief Read-only syntax preview for the immediate-mode terminal canvas.
 * Define MULIB_IMPL_IMTUI_SYNTAX in one translation unit. Requires IMTUI_CORE,
 * TERMSYN and their dependencies (including EDBUF and UTF8).
 */
#ifndef MULIB_IMTUI_SYNTAX_H
#define MULIB_IMTUI_SYNTAX_H
#include "mulib-imtui-core.h"
#include "mulib-termsyn.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Update the caller's syntax cache and draw source into rect in an open
 * frame. first_line is zero-based and may be beyond EOF (nothing drawn).
 * Reuse the cache with the same buffer across frames; destroy the cache
 * before the associated buffer. NULL styles selects the termsyn default.
 *
 * Only source cells are written; there is no wrapping, clearing or input.
 * LF and CRLF delimit lines. Tabs reach four-cell stops measured from rect.x.
 * Each Unicode scalar uses one cell, with malformed bytes and other control
 * characters replaced by U+FFFD. A custom lexer's span crossing a scalar
 * boundary colors the scalar according to its first byte.
 * Returns MULIB_TERMSYN_OK or a negative mulib_termsyn_result. */
int mulib_imtui_syntax_draw(mulib_imtui_t *, mulib_imtui_rect_t,
    const mulib_edbuf_t *, mulib_termsyn_t *, size_t first_line,
    const mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT]);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMTUI_SYNTAX) && !defined(MULIB_IMTUI_SYNTAX_IMPLEMENTED)
#define MULIB_IMTUI_SYNTAX_IMPLEMENTED
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_imtui_syntax_draw(mulib_imtui_t *ctx, mulib_imtui_rect_t rect,
    const mulib_edbuf_t *buffer, mulib_termsyn_t *syntax, size_t first_line,
    const mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT]) {
    unsigned char bytes[512];
    mulib_term_style_t defaults[MULIB_TERMSYN_TOKEN_COUNT];
    mulib_termsyn_span_t span = {0, 0, MULIB_TERMSYN_TEXT};
    mulib_imtui_rect_t clip;
    size_t start, length, at, size, skip;
    int64_t x, y, left, right, bottom;
    int status;
    if (!ctx || !ctx->in_frame || !buffer || !syntax ||
        rect.w < 0 || rect.h < 0 || rect.x > INT_MAX - rect.w || rect.y > INT_MAX - rect.h)
        return MULIB_TERMSYN_EINVAL;
    clip = mulib_imtui_clip(ctx);
    left = rect.x > clip.x ? rect.x : clip.x;
    right = (int64_t)rect.x + rect.w;
    if (right > (int64_t)clip.x + clip.w) right = (int64_t)clip.x + clip.w;
    y = rect.y > clip.y ? rect.y : clip.y;
    bottom = (int64_t)rect.y + rect.h;
    if (bottom > (int64_t)clip.y + clip.h) bottom = (int64_t)clip.y + clip.h;
    if (left >= right || y >= bottom) return MULIB_TERMSYN_OK;
    skip = (size_t)(y - rect.y);
    if (skip > SIZE_MAX - first_line ||
        first_line + skip >= mulib_edbuf_line_count(buffer)) return MULIB_TERMSYN_OK;
    if (!styles) {
        mulib_termsyn_default_theme(defaults);
        styles = defaults;
    }
    for (size_t i = 0; i < MULIB_TERMSYN_TOKEN_COUNT; ++i) {
        mulib_term_cell_t cell = {' ', styles[i]};
        if (!mulib_term_cell_valid(cell)) return MULIB_TERMSYN_EINVAL;
    }
    status = mulib_termsyn_update(syntax, buffer);
    if (status) return status;
    if (mulib_edbuf_line_start(buffer, first_line + skip, &at) != MULIB_EDBUF_OK)
        return MULIB_TERMSYN_EINVAL;
    start = at;
    length = 0;
    size = mulib_edbuf_size(buffer);
    x = rect.x;
    while (at < size && y < bottom) {
        const unsigned char *p;
        size_t avail, used;
        uint32_t cp;
        if (start + length - at < 4 && start + length < size) {
            start = at;
            length = size - at < sizeof bytes ? size - at : sizeof bytes;
            if (mulib_edbuf_read(buffer, start, bytes, length) != MULIB_EDBUF_OK)
                return MULIB_TERMSYN_EINVAL;
        }
        p = bytes + (at - start);
        avail = start + length - at;
        if (x >= right) {
            /* Skip the rest of a clipped line without decoding its text. */
            const unsigned char *lf = (const unsigned char *)memchr(p, '\n', avail);
            if (!lf) { at += avail; continue; }
            at += (size_t)(lf - p);
            p = lf;
            avail = 1;
        }
        if (*p == '\n') { at++; x = rect.x; y++; continue; }
        if (*p == '\r' && avail >= 2 && p[1] == '\n') { at++; continue; }
        if (!mulib_utf8_decode(p, avail, &cp, &used)) { cp = 0xfffd; used = 1; }
        if (cp == '\t' || x >= left) {
            mulib_term_cell_t cell;
            int64_t end = cp == '\t' ? x + 4 - (x - rect.x) % 4 : x + 1;
            if (end > right) end = right;
            if (x < left) x = left < end ? left : end;
            if (x < end) {
                if (at >= span.offset + span.length) {
                    status = mulib_termsyn_at(syntax, at, &span);
                    if (status) return status;
                }
                cell.style = styles[span.token];
                cell.codepoint = cp == '\t' ? ' ' : cp < 32 || (cp >= 127 && cp < 160) ? 0xfffd : cp;
                while (x < end) {
                    if (!mulib_imtui_put(ctx, (int)x, (int)y, cell)) return MULIB_TERMSYN_EINVAL;
                    ++x;
                }
            }
        } else {
            ++x;
        }
        at += used;
    }
    return MULIB_TERMSYN_OK;
}
#ifdef __cplusplus
}
#endif
#endif
