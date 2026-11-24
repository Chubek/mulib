/* MuLIB graphical extension: glx-font. C11, header-only. */
#ifndef MULIB_GLX_FONT_H
#define MULIB_GLX_FONT_H
#include "mulib-textbuffer.h"
#ifndef MULIB_GLX_FONT_MAX_BYTES
#define MULIB_GLX_FONT_MAX_BYTES ((size_t)64 * 1024 * 1024)
#endif
#ifndef MULIB_GLX_FONT_MAX_MAPPINGS
#define MULIB_GLX_FONT_MAX_MAPPINGS ((size_t)1048576)
#endif
typedef struct {
    uint32_t codepoint, glyph;
} mulib_glx_font_mapping_t;
/* Owning object: initialize to zero, do not copy, and free before reuse.
 * The original fields retain their meanings. Unicode maps are owned too. */
typedef struct {
    unsigned char *bits;
    uint32_t count, width, height, bytes;
    double scale;
    mulib_glx_font_mapping_t *mapping;
    size_t mapping_count;
    int unicode;
} mulib_glx_font_t;

static inline uint32_t mgx_le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Validate all records, including sequence mappings which the scalar lookup
 * API deliberately does not shape. A sequence must contain at least two
 * Unicode scalars. Each glyph has exactly one 0xff terminator. */
static inline int mgx_font_psf2_table_(const unsigned char *p, size_t n,
    uint32_t glyphs, mulib_glx_font_mapping_t *map, size_t *entries)
{
    size_t i = 0, total = 0;
    for (uint32_t glyph = 0; glyph < glyphs; ++glyph) {
        size_t sequence_length = 0;
        int sequence = 0;
        for (;;) {
            uint32_t cp;
            size_t used;
            if (i == n)
                return 0;
            if (p[i] == 0xff) {
                ++i;
                if (sequence && sequence_length < 2)
                    return 0;
                break;
            }
            if (p[i] == 0xfe) {
                if (sequence && sequence_length < 2)
                    return 0;
                sequence = 1;
                sequence_length = 0;
                ++i;
                continue;
            }
            if (!mgx_utf8_decode((const char *)p + i, n - i, &cp, &used))
                return 0;
            i += used;
            if (sequence) {
                /* Saturation avoids overflow for arbitrarily long sequences. */
                if (sequence_length < 2)
                    ++sequence_length;
            } else {
                if (total >= MULIB_GLX_FONT_MAX_MAPPINGS ||
                    total >= SIZE_MAX / sizeof *map)
                    return 0;
                if (map)
                    map[total] = (mulib_glx_font_mapping_t){cp, glyph};
                ++total;
            }
        }
    }
    if (i != n)
        return 0;
    *entries = total;
    return 1;
}

static inline int mgx_font_mapping_compare_(const void *a, const void *b)
{
    const mulib_glx_font_mapping_t *x = a, *y = b;
    if (x->codepoint != y->codepoint)
        return x->codepoint < y->codepoint ? -1 : 1;
    return x->glyph < y->glyph ? -1 : x->glyph != y->glyph;
}

/* Copies the bitmap and the validated Unicode mapping; data may be released
 * immediately after success. Failure leaves the empty destination unchanged. */
static inline int mulib_glx_font_psf2(mulib_glx_font_t *f,
                                     const void *data, size_t n)
{
    const unsigned char *p = data;
    mulib_glx_font_t result = {0};
    size_t bitmap_bytes, entries = 0, table;
    uint32_t off, count, bytes, height, width, flags;
    if (!f || f->bits || f->mapping || !p || n < 32 ||
        n > MULIB_GLX_FONT_MAX_BYTES || mgx_le32(p) != 0x864ab572 ||
        mgx_le32(p + 4) != 0)
        return 0;
    off = mgx_le32(p + 8);
    flags = mgx_le32(p + 12);
    count = mgx_le32(p + 16);
    bytes = mgx_le32(p + 20);
    height = mgx_le32(p + 24);
    width = mgx_le32(p + 28);
    if (off < 32 || off > n || (flags & ~1u) || !count || !width ||
        !height || width > 256 || height > 256 ||
        bytes < ((width + 7) / 8) * height || !bytes ||
        count > (n - off) / bytes)
        return 0;
    bitmap_bytes = (size_t)count * bytes;
    table = (size_t)off + bitmap_bytes;
    if (flags && (!mgx_font_psf2_table_(p + table, n - table, count,
                                       NULL, &entries) ||
                  entries > (MULIB_GLX_FONT_MAX_BYTES - bitmap_bytes) /
                            sizeof *result.mapping))
        return 0;
    result.bits = (unsigned char *)malloc(bitmap_bytes);
    if (!result.bits)
        return 0;
    if (entries) {
        size_t unique = 0;
        result.mapping = (mulib_glx_font_mapping_t *)
            malloc(entries * sizeof *result.mapping);
        if (!result.mapping) {
            free(result.bits);
            return 0;
        }
        if (!mgx_font_psf2_table_(p + table, n - table, count,
                                 result.mapping, &entries)) {
            free(result.mapping);
            free(result.bits);
            return 0;
        }
        qsort(result.mapping, entries, sizeof *result.mapping,
              mgx_font_mapping_compare_);
        /* Duplicate aliases resolve deterministically to the first glyph. */
        for (size_t i = 0; i < entries; ++i)
            if (!unique || result.mapping[i].codepoint !=
                           result.mapping[unique - 1].codepoint)
                result.mapping[unique++] = result.mapping[i];
        result.mapping_count = unique;
    }
    memcpy(result.bits, p + off, bitmap_bytes);
    result.count = count;
    result.width = width;
    result.height = height;
    result.bytes = bytes;
    result.scale = 1;
    result.unicode = flags != 0;
    *f = result;
    return 1;
}

static inline void mulib_glx_font_free(mulib_glx_font_t *f)
{
    if (f) {
        free(f->bits);
        free(f->mapping);
        memset(f, 0, sizeof *f);
    }
}

static inline int mgx_font_valid_(const mulib_glx_font_t *f)
{
    return f && f->bits && f->count && f->width && f->width <= 256 &&
           f->height && f->height <= 256 &&
           f->bytes >= ((f->width + 7) / 8) * f->height &&
           f->count <= SIZE_MAX / f->bytes &&
           (!f->mapping_count || f->mapping);
}

/* Exact scalar lookup, without fallback. Returns 1 and sets glyph on a hit;
 * otherwise returns 0 without modifying glyph. No-table fonts use identity
 * indexing, not a claimed encoding conversion. */
static inline int mulib_glx_font_lookup(const mulib_glx_font_t *f,
                                        uint32_t cp, uint32_t *glyph)
{
    size_t lo = 0, hi;
    if (!glyph || !mgx_font_valid_(f) || cp > 0x10ffff ||
        (cp >= 0xd800 && cp <= 0xdfff))
        return 0;
    if (!f->unicode) {
        if (cp >= f->count)
            return 0;
        *glyph = cp;
        return 1;
    }
    hi = f->mapping_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (f->mapping[mid].codepoint < cp)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == f->mapping_count || f->mapping[lo].codepoint != cp ||
        f->mapping[lo].glyph >= f->count)
        return 0;
    *glyph = f->mapping[lo].glyph;
    return 1;
}

/* Missing scalars use U+FFFD, then '?', then glyph zero; UINT32_MAX denotes
 * an invalid font. Sequence constituents never become aliases by accident. */
static inline uint32_t mulib_glx_font_glyph(const mulib_glx_font_t *f,
                                           uint32_t cp)
{
    uint32_t glyph;
    if (!mgx_font_valid_(f))
        return UINT32_MAX;
    if (mulib_glx_font_lookup(f, cp, &glyph) ||
        mulib_glx_font_lookup(f, 0xfffd, &glyph) ||
        mulib_glx_font_lookup(f, '?', &glyph))
        return glyph;
    return 0;
}

static inline int mulib_glx_font_glyph_bit(const mulib_glx_font_t *f,
    uint32_t glyph, unsigned x, unsigned y)
{
    size_t offset;
    if (!mgx_font_valid_(f) || glyph >= f->count ||
        x >= f->width || y >= f->height)
        return 0;
    offset = (size_t)glyph * f->bytes +
             (size_t)y * ((f->width + 7) / 8) + x / 8;
    return (f->bits[offset] >> (7 - x % 8)) & 1;
}

static inline int mulib_glx_font_bit(const mulib_glx_font_t *f,
                                    uint32_t cp, unsigned x, unsigned y)
{
    return mulib_glx_font_glyph_bit(f, mulib_glx_font_glyph(f, cp), x, y);
}
/* Small original fixed-width ASCII fallback. No external font file. */
static inline int mulib_glx_font_builtin(mulib_glx_font_t *f){
 static const unsigned char glyphs[95][7]={
  {0,0,0,0,0,0,0}, /* 32 */
  {4,4,4,4,4,0,4}, /* 33 */
  {10,10,0,0,0,0,0}, /* 34 */
  {10,10,31,10,31,10,10}, /* 35 */
  {4,15,20,14,5,30,4}, /* 36 */
  {17,2,4,8,16,17,0}, /* 37 */
  {12,18,20,8,21,18,13}, /* 38 */
  {4,4,8,0,0,0,0}, /* 39 */
  {2,4,8,8,8,4,2}, /* 40 */
  {8,4,2,2,2,4,8}, /* 41 */
  {0,21,14,31,14,21,0}, /* 42 */
  {0,4,4,31,4,4,0}, /* 43 */
  {0,0,0,0,0,4,8}, /* 44 */
  {0,0,0,31,0,0,0}, /* 45 */
  {0,0,0,0,0,0,4}, /* 46 */
  {1,1,2,4,8,16,16}, /* 47 */
  {14,17,19,21,25,17,14}, /* 48 */
  {4,12,4,4,4,4,14}, /* 49 */
  {14,17,1,2,4,8,31}, /* 50 */
  {30,1,1,14,1,1,30}, /* 51 */
  {2,6,10,18,31,2,2}, /* 52 */
  {31,16,16,30,1,1,30}, /* 53 */
  {14,16,16,30,17,17,14}, /* 54 */
  {31,1,2,4,8,8,8}, /* 55 */
  {14,17,17,14,17,17,14}, /* 56 */
  {14,17,17,15,1,1,14}, /* 57 */
  {0,4,0,0,4,0,0}, /* 58 */
  {0,4,0,0,4,4,8}, /* 59 */
  {1,2,4,8,4,2,1}, /* 60 */
  {0,0,31,0,31,0,0}, /* 61 */
  {16,8,4,2,4,8,16}, /* 62 */
  {14,17,1,2,4,0,4}, /* 63 */
  {14,17,23,21,23,16,14}, /* 64 */
  {14,17,17,31,17,17,17}, /* 65 */
  {30,17,17,30,17,17,30}, /* 66 */
  {14,17,16,16,16,17,14}, /* 67 */
  {30,17,17,17,17,17,30}, /* 68 */
  {31,16,16,30,16,16,31}, /* 69 */
  {31,16,16,30,16,16,16}, /* 70 */
  {14,17,16,23,17,17,15}, /* 71 */
  {17,17,17,31,17,17,17}, /* 72 */
  {14,4,4,4,4,4,14}, /* 73 */
  {7,2,2,2,18,18,12}, /* 74 */
  {17,18,20,24,20,18,17}, /* 75 */
  {16,16,16,16,16,16,31}, /* 76 */
  {17,27,21,21,17,17,17}, /* 77 */
  {17,25,21,19,17,17,17}, /* 78 */
  {14,17,17,17,17,17,14}, /* 79 */
  {30,17,17,30,16,16,16}, /* 80 */
  {14,17,17,17,21,18,13}, /* 81 */
  {30,17,17,30,20,18,17}, /* 82 */
  {15,16,16,14,1,1,30}, /* 83 */
  {31,4,4,4,4,4,4}, /* 84 */
  {17,17,17,17,17,17,14}, /* 85 */
  {17,17,17,17,17,10,4}, /* 86 */
  {17,17,17,21,21,27,17}, /* 87 */
  {17,17,10,4,10,17,17}, /* 88 */
  {17,17,10,4,4,4,4}, /* 89 */
  {31,1,2,4,8,16,31}, /* 90 */
  {14,8,8,8,8,8,14}, /* 91 */
  {16,16,8,4,2,1,1}, /* 92 */
  {14,2,2,2,2,2,14}, /* 93 */
  {4,10,17,0,0,0,0}, /* 94 */
  {0,0,0,0,0,0,31}, /* 95 */
  {8,4,0,0,0,0,0}, /* 96 */
  {0,0,14,1,15,17,15}, /* 97 */
  {16,16,22,25,17,17,30}, /* 98 */
  {0,0,14,16,16,17,14}, /* 99 */
  {1,1,13,19,17,17,15}, /* 100 */
  {0,0,14,17,31,16,14}, /* 101 */
  {6,9,8,28,8,8,8}, /* 102 */
  {0,0,15,17,15,1,14}, /* 103 */
  {16,16,22,25,17,17,17}, /* 104 */
  {4,0,12,4,4,4,14}, /* 105 */
  {2,0,6,2,2,18,12}, /* 106 */
  {16,16,18,20,24,20,18}, /* 107 */
  {12,4,4,4,4,4,14}, /* 108 */
  {0,0,26,21,21,21,21}, /* 109 */
  {0,0,22,25,17,17,17}, /* 110 */
  {0,0,14,17,17,17,14}, /* 111 */
  {0,0,30,17,30,16,16}, /* 112 */
  {0,0,15,17,15,1,1}, /* 113 */
  {0,0,22,25,16,16,16}, /* 114 */
  {0,0,15,16,14,1,30}, /* 115 */
  {8,8,28,8,8,9,6}, /* 116 */
  {0,0,17,17,17,19,13}, /* 117 */
  {0,0,17,17,17,10,4}, /* 118 */
  {0,0,17,17,21,21,10}, /* 119 */
  {0,0,17,10,4,10,17}, /* 120 */
  {0,0,17,17,15,1,14}, /* 121 */
  {0,0,31,2,4,8,31}, /* 122 */
  {3,4,4,8,4,4,3}, /* 123 */
  {4,4,4,4,4,4,4}, /* 124 */
  {24,4,4,2,4,4,24}, /* 125 */
  {0,0,9,22,0,0,0}, /* 126 */
 };if(!f||f->bits||f->mapping)return 0;unsigned char *p=calloc(128,8);if(!p)return 0;
 for(unsigned c=32;c<127;c++)for(unsigned y=0;y<7;y++)p[c*8+y]=(unsigned char)(glyphs[c-32][y]<<3);
 *f=(mulib_glx_font_t){.bits=p,.count=128,.width=6,.height=8,.bytes=8,.scale=1};return 1;
}
#endif
