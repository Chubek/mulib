/* MuLIB browser staging addition: web-textcodec
 * Implemented subset: UTF-16 to UTF-8 USVString encoding and bounded streaming
 * UTF-8 decoding with replacement, fatal and BOM options.
 * Remaining: JS objects, encoding labels and other encodings.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_TEXTCODEC_H
#define MULIB_WEB_TEXTCODEC_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-utf16.h"

typedef enum {
    MULIB_WEB_TEXTCODEC_OK = 1,
    MULIB_WEB_TEXTCODEC_INVALID = 0,
    MULIB_WEB_TEXTCODEC_MALFORMED = -1,
    MULIB_WEB_TEXTCODEC_LIMIT = -2,
    MULIB_WEB_TEXTCODEC_NO_MEMORY = -3
} mulib_web_textcodec_status;

/* Caller-owned state; initialize or reset with decoder_init. */
typedef struct {
    size_t remaining;
    uint32_t scalar;
    unsigned int needed, lower, upper;
    int fatal, ignore_bom, seen, finished;
} mulib_web_textcodec_decoder;

/* Limit is cumulative emitted bytes, after optional BOM removal. Zero
 * allows no output bytes. ignore_bom is the Web option: true preserves BOM. */
static inline void
mulib_web_textcodec_decoder_init(mulib_web_textcodec_decoder *d,
                                 size_t limit, int fatal, int ignore_bom)
{
    if (d)
        *d = (mulib_web_textcodec_decoder){
            limit, 0, 0, 0x80, 0xbf, !!fatal, !!ignore_bom, 0, 0
        };
}

/* Internal: same transition pass for sizing and emitting. */
static inline mulib_web_textcodec_status
mulib_web_textcodec_run_(mulib_web_textcodec_decoder *d, muba_span s,
                         int finish, size_t *len, muba_buf *dest)
{
    size_t i = 0;
    while (i < s.len || (finish && d->needed)) {
        uint32_t c;
        int malformed = 0;
        if (i == s.len) {
            /* EOF within a legal sequence prefix counts as one error. */
            d->needed = 0;
            malformed = 1;
        } else if (d->needed) {
            unsigned int byte = s.data[i];
            if (byte < d->lower || byte > d->upper) {
                /* Reprocess the disallowed byte after emitting replacement. */
                d->needed = 0;
                malformed = 1;
            } else {
                ++i;
                d->lower = 0x80;
                d->upper = 0xbf;
                d->scalar = (d->scalar << 6) | (byte & 0x3f);
                if (--d->needed)
                    continue;
            }
        } else {
            unsigned int byte = s.data[i++];
            if (byte <= 0x7f) {
                d->scalar = byte;
            } else if (byte >= 0xc2 && byte <= 0xf4) {
                /* Tighten the first continuation range against overlongs,
                 * surrogates and scalars above U+10FFFF. */
                d->lower = byte == 0xe0 ? 0xa0 : byte == 0xf0 ? 0x90 : 0x80;
                d->upper = byte == 0xed ? 0x9f : byte == 0xf4 ? 0x8f : 0xbf;
                d->needed = byte < 0xe0 ? 1u : byte < 0xf0 ? 2u : 3u;
                d->scalar = byte & (0x7fu >> (d->needed + 1));
                continue;
            } else {
                malformed = 1;
            }
        }
        if (malformed && d->fatal)
            return MULIB_WEB_TEXTCODEC_MALFORMED;
        c = malformed ? 0xfffd : d->scalar;
        if (!d->seen) {
            d->seen = 1;
            if (!d->ignore_bom && c == 0xfeff)
                continue;
        }

        size_t width = c < 0x80 ? 1u : c < 0x800 ? 2u : c < 0x10000 ? 3u : 4u;
        if (width > d->remaining)
            return MULIB_WEB_TEXTCODEC_LIMIT;
        d->remaining -= width;
        *len += width; /* bounded by the remaining budget at entry */
        if (dest && !muba_utf8_put(dest, c, 0))
            return MULIB_WEB_TEXTCODEC_INVALID;
    }
    d->finished = !!finish;
    return MULIB_WEB_TEXTCODEC_OK;
}

/* Feed one chunk, optionally marking EOF. On success replace out with this
 * chunk's UTF-8 output; on failure leave both decoder and out unchanged.
 * Successful EOF requires reinitialization before feeding more input. */
static inline mulib_web_textcodec_status
mulib_web_textcodec_decoder_feed(mulib_web_textcodec_decoder *d, muba_span s,
                                int finish, muba_buf *out)
{
    if (!d || !out || d->finished || !muba_span_ok(s))
        return MULIB_WEB_TEXTCODEC_INVALID;
    mulib_web_textcodec_decoder next = *d;
    size_t len = 0;
    mulib_web_textcodec_status status =
        mulib_web_textcodec_run_(&next, s, finish, &len, NULL);
    if (status != MULIB_WEB_TEXTCODEC_OK)
        return status;

    /* Preflight avoids output allocation for malformed or over-limit chunks
     * and enforces an exact allocation even when the budget is very small. */
    muba_buf staged = {0};
    if (len) {
        staged.data = (unsigned char *)malloc(len);
        if (!staged.data)
            return MULIB_WEB_TEXTCODEC_NO_MEMORY;
        staged.cap = len;
        next = *d;
        len = 0;
        status = mulib_web_textcodec_run_(&next, s, finish, &len, &staged);
        if (status != MULIB_WEB_TEXTCODEC_OK) {
            muba_free(&staged);
            return status;
        }
    }
    muba_commit(out, &staged);
    *d = next;
    return MULIB_WEB_TEXTCODEC_OK;
}

static inline int
mulib_web_textcodec_encode(const uint16_t *s, size_t n, muba_buf *out)
{
    return mulib_utf16_encode(s, n, 0, out);
}

/* One-shot replacement decoding; retain the legacy BOM-preserving default. */
static inline int
mulib_web_textcodec_decode(muba_span s, muba_buf *out)
{
    mulib_web_textcodec_decoder d;
    mulib_web_textcodec_decoder_init(&d, SIZE_MAX, 0, 1);
    return mulib_web_textcodec_decoder_feed(&d, s, 1, out) == MULIB_WEB_TEXTCODEC_OK;
}
#endif /* MULIB_WEB_TEXTCODEC_H */
