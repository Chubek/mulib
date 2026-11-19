/* Native PNG integration: no runtime libraries, device access or vendor assets. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t allocation_budget = SIZE_MAX, allocations_live;

static int
allocation_fails(void)
{
    if (allocation_budget == SIZE_MAX)
        return 0;
    if (!allocation_budget)
        return 1;
    --allocation_budget;
    return 0;
}

static void *
test_malloc(size_t n)
{
    void *p = allocation_fails() ? NULL : malloc(n);
    if (p)
        ++allocations_live;
    return p;
}

static void *
test_calloc(size_t n, size_t bytes)
{
    void *p = allocation_fails() ? NULL : calloc(n, bytes);
    if (p)
        ++allocations_live;
    return p;
}

static void *
test_realloc(void *p, size_t n)
{
    int empty = p == NULL;
    if (allocation_fails())
        return NULL;
    void *q = realloc(p, n);
    if (q && empty)
        ++allocations_live;
    return q;
}

static void
test_free(void *p)
{
    if (p) {
        assert(allocations_live);
        --allocations_live;
    }
    free(p);
}

#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_DEFLATE
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_IMAGEPNG
#define MULIB_GLX_IMAGEDECODE_ENABLE_PNG
#include "mulib-glx-imagedecode.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void
put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void
chunk(mulib_typecore_buf_t *b, const char tag[4], const void *p, size_t n)
{
    assert(n <= UINT32_MAX);
    assert(mulib_typecore_u32be(b, (uint32_t)n));
    size_t at = b->size;
    assert(mulib_typecore_append(b, tag, 4));
    assert(mulib_typecore_append(b, p, n));
    assert(mulib_typecore_u32be(b, mulib_typecore_crc32(b->data + at, n + 4)));
}

static size_t
find_chunk(const mulib_typecore_buf_t *b, const char tag[4], unsigned occurrence)
{
    for (size_t at = 8; at + 12 <= b->size;) {
        size_t n = mulib_imagepng_be32_(b->data + at);
        assert(n <= b->size - at - 12);
        if (!memcmp(b->data + at + 4, tag, 4) && !occurrence--)
            return at;
        at += n + 12;
    }
    return SIZE_MAX;
}

static void
fix_crc(mulib_typecore_buf_t *b, size_t at)
{
    assert(at != SIZE_MAX);
    size_t n = mulib_imagepng_be32_(b->data + at);
    assert(n <= b->size - at - 12);
    put32(b->data + at + 8 + n, mulib_typecore_crc32(b->data + at + 4, n + 4));
}

static mulib_typecore_buf_t
copy_fixture(const mulib_typecore_buf_t *b)
{
    mulib_typecore_buf_t copy = {0};
    assert(mulib_typecore_append(&copy, b->data, b->size));
    return copy;
}

static void
insert_chunk(mulib_typecore_buf_t *b, size_t at, const char tag[4],
              const void *p, size_t n)
{
    mulib_typecore_buf_t result = {0};
    assert(at <= b->size);
    assert(mulib_typecore_append(&result, b->data, at));
    chunk(&result, tag, p, n);
    assert(mulib_typecore_append(&result, b->data + at, b->size - at));
    mulib_typecore_free(b);
    *b = result;
}

static void
remove_chunk(mulib_typecore_buf_t *b, size_t at)
{
    assert(at != SIZE_MAX && at + 12 <= b->size);
    size_t n = mulib_imagepng_be32_(b->data + at) + 12;
    assert(n <= b->size - at);
    memmove(b->data + at, b->data + at + n, b->size - at - n);
    b->size -= n;
}

static mulib_typecore_buf_t
zlib_stream(const unsigned char *raw, size_t n, int stored)
{
    mulib_typecore_buf_t z = {0};
    assert(mulib_typecore_byte(&z, 0x78));
    assert(mulib_typecore_byte(&z, 0x01));
    if (stored) {
        assert(n <= 65535);
        unsigned char header[5] = {
            1, (unsigned char)n, (unsigned char)(n >> 8),
            (unsigned char)~n, (unsigned char)(~n >> 8)
        };
        assert(mulib_typecore_append(&z, header, sizeof header));
        assert(mulib_typecore_append(&z, raw, n));
    } else {
        mulib_buffer_t compressed = {0};
        assert(mulib_deflate_encode(raw, n, &compressed) == MULIB_DEFLATE_OK);
        assert(mulib_typecore_append(&z, compressed.data, compressed.length));
        mulib_buffer_free(&compressed);
    }
    assert(mulib_typecore_u32be(&z, mulib_typecore_adler32(raw, n)));
    return z;
}

static mulib_typecore_buf_t
png_from_zlib(uint32_t width, uint32_t height, unsigned type,
               const unsigned char *z, size_t zn, int transparency, int split)
{
    static const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    static const unsigned char palette[] = {12, 34, 56, 200, 150, 100, 0, 255, 80};
    static const unsigned char palpha[] = {0, 64};
    static const unsigned char gtrns[] = {0, 0};
    static const unsigned char rgbtrns[] = {0, 0, 0, 70, 0, 140};
    mulib_typecore_buf_t png = {0};
    unsigned char ihdr[13] = {0};
    put32(ihdr, width);
    put32(ihdr + 4, height);
    ihdr[8] = 8;
    ihdr[9] = (unsigned char)type;
    assert(mulib_typecore_append(&png, signature, sizeof signature));
    chunk(&png, "IHDR", ihdr, sizeof ihdr);
    if (type == 3)
        chunk(&png, "PLTE", palette, sizeof palette);
    if (transparency && type == 3)
        chunk(&png, "tRNS", palpha, sizeof palpha);
    else if (transparency && type == 0)
        chunk(&png, "tRNS", gtrns, sizeof gtrns);
    else if (transparency && type == 2)
        chunk(&png, "tRNS", rgbtrns, sizeof rgbtrns);
    if (split) {
        chunk(&png, "IDAT", NULL, 0);
        for (size_t i = 0; i < zn; ++i)
            chunk(&png, "IDAT", z + i, 1);
        chunk(&png, "IDAT", NULL, 0);
    } else {
        chunk(&png, "IDAT", z, zn);
    }
    chunk(&png, "IEND", NULL, 0);
    return png;
}

/* Independently encode each PNG filter, including modular-byte arithmetic. */
static unsigned
predictor(unsigned filter, unsigned a, unsigned b, unsigned c)
{
    if (filter == 1)
        return a;
    if (filter == 2)
        return b;
    if (filter == 3)
        return (a + b) >> 1;
    if (filter == 4) {
        int da = abs((int)b - (int)c), db = abs((int)a - (int)c);
        int dc = abs((int)a + (int)b - 2 * (int)c);
        if (da <= db && da <= dc)
            return a;
        if (db <= dc)
            return b;
        return c;
    }
    return 0;
}

static mulib_typecore_buf_t
make_pixels(unsigned type, unsigned filter_offset, int split,
             uint32_t expected[15])
{
    unsigned channels = type == 2 ? 3 : type == 4 ? 2 : type == 6 ? 4 : 1;
    unsigned char samples[60], raw[65];
    static const uint32_t palette[] = {0x000c2238, 0x40c89664, 0xff00ff50};
    for (unsigned y = 0; y < 5; ++y) {
        unsigned filter = (y + filter_offset) % 5;
        raw[y * (3 * channels + 1)] = (unsigned char)filter;
        for (unsigned x = 0; x < 3; ++x) {
            unsigned char *s = samples + (y * 3 + x) * channels;
            for (unsigned c = 0; c < channels; ++c)
                s[c] = (unsigned char)(y * 97 + x * 113 + c * 70);
            if (type == 3) {
                s[0] = (unsigned char)((x + y) % 3);
                expected[y * 3 + x] = palette[s[0]];
            } else if (type == 0 || type == 4) {
                unsigned alpha = type == 4 ? s[1] : s[0] ? 255 : 0;
                expected[y * 3 + x] = (uint32_t)alpha << 24
                    | (uint32_t)s[0] * UINT32_C(0x010101);
            } else {
                unsigned alpha = type == 6 ? s[3]
                    : s[0] == 0 && s[1] == 70 && s[2] == 140 ? 0 : 255;
                expected[y * 3 + x] = (uint32_t)alpha << 24
                    | (uint32_t)s[0] << 16 | (uint32_t)s[1] << 8 | s[2];
            }
        }
        for (unsigned x = 0; x < 3 * channels; ++x) {
            unsigned index = y * 3 * channels + x;
            unsigned a = x >= channels ? samples[index - channels] : 0;
            unsigned b = y ? samples[index - 3 * channels] : 0;
            unsigned c = y && x >= channels ? samples[index - 4 * channels] : 0;
            raw[y * (3 * channels + 1) + x + 1] =
                (unsigned char)(samples[index] - predictor(filter, a, b, c));
        }
    }
    mulib_typecore_buf_t z = zlib_stream(raw, 5 * (3 * channels + 1),
                                        filter_offset & 1u);
    mulib_typecore_buf_t png = png_from_zlib(3, 5, type, z.data, z.size, 1, split);
    mulib_typecore_free(&z);
    return png;
}

static void
rejected(const void *p, size_t n)
{
    mgx_surface s = {0};
    mulib_imagepng_rgba_t rgba = {0};
    size_t before = allocations_live;
    assert(!mulib_glx_imagedecode(p, n, &s));
    assert(!s.pixels && !s.w && !s.h);
    assert(!mulib_imagepng_decode_rgba(p, n, &rgba, NULL));
    assert(!rgba.pixels && !rgba.width && !rgba.height && !rgba.stride);
    assert(allocations_live == before);
}

static void
test_color_types_and_filters(void)
{
    static const unsigned types[] = {0, 2, 3, 4, 6};
    for (size_t t = 0; t < sizeof types / sizeof *types; ++t) {
        for (unsigned offset = 0; offset < 5; ++offset) {
            uint32_t expected[15];
            mulib_typecore_buf_t png = make_pixels(types[t], offset, 1, expected);
            mgx_surface s = {0};
            mulib_imagepng_rgba_t rgba = {0};
            assert(mulib_glx_imagedecode(png.data, png.size, &s));
            assert(s.w == 3 && s.h == 5);
            for (size_t i = 0; i < 15; ++i)
                assert(s.pixels[i] == expected[i]);
            assert(mulib_imagepng_decode_rgba(png.data, png.size, &rgba, NULL));
            assert(rgba.width == 3 && rgba.height == 5 && rgba.stride == 12);
            for (size_t i = 0; i < 15; ++i) {
                uint32_t color = (uint32_t)rgba.pixels[i * 4 + 3] << 24
                    | (uint32_t)rgba.pixels[i * 4] << 16
                    | (uint32_t)rgba.pixels[i * 4 + 1] << 8 | rgba.pixels[i * 4 + 2];
                assert(color == expected[i]);
            }
            assert(!mulib_glx_imagedecode(png.data, png.size, &s));
            assert(!mulib_imagepng_decode_rgba(png.data, png.size, &rgba, NULL));
            mulib_typecore_free(&png);
            /* Pixel ownership is independent of the original encoded bytes. */
            assert(s.pixels[14] == expected[14]);
            mulib_imagepng_rgba_free(&rgba);
            mulib_imagepng_rgba_free(&rgba);
            mgx_surface_free(&s);
        }
    }
    mulib_imagepng_rgba_free(NULL);
}

static void
test_dynamic_and_encoder(void)
{
    /* Python zlib.compress(..., 9), synthetic grayscale rows: ((x+y)%7)*31.
     * This independent fixture begins with a dynamic-Huffman block. */
    static const unsigned char dynamic[] = {
        0x78,0xda,0xed,0xd1,0x31,0x01,0x00,0x30,0x08,0xc4,0xc0,0x77,0x82,
        0x13,0x9c,0xe0,0x04,0x27,0x38,0x41,0x5d,0x1d,0x74,0xe8,0x0f,0x5d,
        0x32,0x65,0xca,0x74,0x52,0x64,0xf5,0xec,0x73,0xdc,0x3f,0x64,0xfe,
        0x29,0xf3,0x2f,0x99,0x7f,0xcb,0xfc,0x47,0xfb,0x9b,0x01,0x46,0x18,
        0x61,0x84,0x11,0x46,0x18,0x61,0x84,0x11,0x46,0x18,0x61,0x84,0x11,
        0x46,0x18,0x6f,0x39,0xf5,0xe7,0xcf,0xef
    };
    mulib_typecore_buf_t png = png_from_zlib(64, 64, 0, dynamic, sizeof dynamic, 0, 1);
    mgx_surface s = {0};
    assert(((dynamic[2] >> 1) & 3u) == 2);
    assert(mulib_glx_imagedecode(png.data, png.size, &s));
    for (unsigned y = 0; y < 64; ++y)
        for (unsigned x = 0; x < 64; ++x)
            assert(s.pixels[y * 64 + x] == (UINT32_C(0xff000000)
                | ((x + y) % 7) * 31 * UINT32_C(0x010101)));
    mgx_surface_free(&s);
    mulib_typecore_free(&png);

    /* More than 65535 filtered bytes forces the encoder's stored stream to
     * cross a DEFLATE block boundary, independently of PNG row boundaries. */
    unsigned char rgb[257 * 100 * 3];
    for (size_t i = 0; i < sizeof rgb; ++i)
        rgb[i] = (unsigned char)(i * 73);
    assert(mulib_imagepng_encode_rgb(rgb, 257, 100, &png));
    assert(mulib_glx_imagedecode(png.data, png.size, &s));
    for (size_t i = 0; i < 257 * 100; ++i)
        assert(s.pixels[i] == (UINT32_C(0xff000000) | (uint32_t)rgb[i * 3] << 16
            | (uint32_t)rgb[i * 3 + 1] << 8 | rgb[i * 3 + 2]));
    mgx_surface_free(&s);
    mulib_typecore_free(&png);
}

static void
test_structure_and_limits(void)
{
    uint32_t expected[15];
    mulib_typecore_buf_t png = make_pixels(6, 0, 0, expected), bad = {0};
    size_t ihdr = find_chunk(&png, "IHDR", 0);
    for (size_t n = 0; n < png.size; ++n)
        rejected(png.data, n);
    rejected(NULL, png.size);
    for (unsigned field = 0; field < 13; ++field) {
        static const unsigned char values[] = {255,0,0,0,255,0,0,0,16,5,1,1,1};
        bad = copy_fixture(&png);
        bad.data[ihdr + 8 + field] = values[field];
        fix_crc(&bad, ihdr);
        /* Low dimension bytes 0 make a zero dimension; high bytes exceed caps. */
        if (field != 1 && field != 2 && field != 5 && field != 6)
            rejected(bad.data, bad.size);
        mulib_typecore_free(&bad);
    }
    bad = copy_fixture(&png);
    assert(mulib_typecore_byte(&bad, 0)); /* No trailing bytes after IEND. */
    rejected(bad.data, bad.size);
    mulib_typecore_free(&bad);
    bad = copy_fixture(&png);
    bad.data[ihdr + 8] ^= 1; /* CRC, not just dimension validation. */
    rejected(bad.data, bad.size);
    mulib_typecore_free(&bad);

    const char *tags[] = {"ABCD", "abca", "acTL", "fcTL", "fdAT", "IEND"};
    for (size_t i = 0; i < sizeof tags / sizeof *tags; ++i) {
        bad = copy_fixture(&png);
        insert_chunk(&bad, 33, tags[i], NULL, 0);
        rejected(bad.data, bad.size);
        mulib_typecore_free(&bad);
    }
    bad = copy_fixture(&png);
    insert_chunk(&bad, 33, "IHDR", png.data + 16, 13);
    rejected(bad.data, bad.size);
    mulib_typecore_free(&bad);
    bad = copy_fixture(&png);
    remove_chunk(&bad, find_chunk(&bad, "IDAT", 0));
    rejected(bad.data, bad.size);
    mulib_typecore_free(&bad);
    bad = copy_fixture(&png);
    size_t end = find_chunk(&bad, "IEND", 0);
    insert_chunk(&bad, end, "teST", "ignored", 7); /* Legal ancillary chunk. */
    mgx_surface s = {0};
    assert(mulib_glx_imagedecode(bad.data, bad.size, &s));
    mgx_surface_free(&s);
    insert_chunk(&bad, find_chunk(&bad, "IEND", 0), "IDAT", NULL, 0);
    rejected(bad.data, bad.size); /* IDAT must not resume after ancillary data. */
    mulib_typecore_free(&bad);

    for (unsigned field = 0; field < 5; ++field) {
        mulib_glx_image_limits_t l = {png.size, 15, 60, 3, 5};
        switch (field) {
        case 0: --l.max_input_bytes; break;
        case 1: --l.max_pixels; break;
        case 2: --l.max_output_bytes; break;
        case 3: --l.max_width; break;
        default: --l.max_height; break;
        }
        assert(!mulib_glx_imagedecode_limited(png.data, png.size, &s, &l));
        assert(!s.pixels);
    }
    mulib_glx_image_limits_t l = {png.size, 15, 60, 3, 5};
    assert(mulib_glx_imagedecode_limited(png.data, png.size, &s, &l));
    mgx_surface_free(&s);
    for (unsigned field = 0; field < 7; ++field) {
        mulib_imagepng_limits_t limits = mulib_imagepng_limits_default();
        mulib_imagepng_rgba_t rgba = {0};
        switch (field) {
        case 0: limits.max_input_bytes = 0; break;
        case 1: limits.max_pixels = 0; break;
        case 2: limits.max_output_bytes = 0; break;
        case 3: limits.max_work_bytes = 0; break;
        case 4: limits.max_width = 0; break;
        case 5: limits.max_height = 0; break;
        default: limits.max_chunks = 0; break;
        }
        assert(!mulib_imagepng_decode_rgba(png.data, png.size, &rgba, &limits));
        assert(!rgba.pixels);
    }
    mulib_imagepng_limits_t limits = mulib_imagepng_limits_default();
    mulib_imagepng_rgba_t rgba = {0};
    size_t zn = mulib_imagepng_be32_(png.data + find_chunk(&png, "IDAT", 0));
    /* Raw length is 65, so the existing allocator reserves 128 bytes twice. */
    limits.max_work_bytes = 60 + zn + 256;
    limits.max_chunks = 3;
    assert(mulib_imagepng_decode_rgba(png.data, png.size, &rgba, &limits));
    mulib_imagepng_rgba_free(&rgba);
    --limits.max_work_bytes;
    assert(!mulib_imagepng_decode_rgba(png.data, png.size, &rgba, &limits));
    ++limits.max_work_bytes;
    --limits.max_chunks;
    assert(!mulib_imagepng_decode_rgba(png.data, png.size, &rgba, &limits));
    mulib_typecore_free(&png);
}

static void
test_palette_and_compression_errors(void)
{
    uint32_t expected[15];
    mulib_typecore_buf_t png = make_pixels(3, 0, 0, expected);
    for (unsigned mode = 0; mode < 7; ++mode) {
        mulib_typecore_buf_t bad = copy_fixture(&png);
        size_t palette = find_chunk(&bad, "PLTE", 0);
        size_t alpha = find_chunk(&bad, "tRNS", 0);
        if (mode == 0)
            remove_chunk(&bad, palette);
        else if (mode == 1)
            insert_chunk(&bad, palette, "PLTE", bad.data + palette + 8, 9);
        else if (mode == 2)
            insert_chunk(&bad, alpha, "tRNS", bad.data + alpha + 8, 2);
        else if (mode == 3) {
            remove_chunk(&bad, alpha);
            insert_chunk(&bad, find_chunk(&bad, "IEND", 0), "tRNS", "\0", 1);
        } else if (mode == 4) {
            remove_chunk(&bad, palette);
            insert_chunk(&bad, 33, "PLTE", "abcd", 4);
        } else if (mode == 5) {
            unsigned char too_many[771] = {0};
            remove_chunk(&bad, palette);
            insert_chunk(&bad, 33, "PLTE", too_many, sizeof too_many);
        } else {
            remove_chunk(&bad, alpha);
            insert_chunk(&bad, find_chunk(&bad, "IDAT", 0), "tRNS", "abcd", 4);
        }
        rejected(bad.data, bad.size);
        mulib_typecore_free(&bad);
    }
    mulib_typecore_free(&png);

    const unsigned char valid_raw[] = {0, 12, 34, 56, 128};
    for (unsigned mode = 0; mode < 9; ++mode) {
        unsigned char raw[sizeof valid_raw];
        memcpy(raw, valid_raw, sizeof raw);
        if (mode == 0)
            raw[0] = 5;
        mulib_typecore_buf_t z = zlib_stream(raw, sizeof raw, 0);
        if (mode == 1)
            z.data[0] = 0x79; /* Unsupported compression method. */
        else if (mode == 2)
            z.data[1] ^= 1; /* FCHECK. */
        else if (mode == 3)
            z.data[1] = 0x20; /* Valid FCHECK but FDICT forbidden in PNG. */
        else if (mode == 4)
            z.data[2] = 7; /* Reserved DEFLATE block type. */
        else if (mode == 5)
            z.data[z.size - 1] ^= 1; /* Adler mismatch, with correct chunk CRC. */
        else if (mode == 6) {
            assert(mulib_typecore_byte(&z, 0));
            memmove(z.data + z.size - 4, z.data + z.size - 5, 4);
            z.data[z.size - 5] = 0; /* Garbage after the raw DEFLATE stream. */
        }
        png = png_from_zlib(mode == 7 ? 2 : 1, mode == 8 ? 2 : 1, 6,
                            z.data, z.size, 0, 0);
        rejected(png.data, png.size);
        mulib_typecore_free(&png);
        mulib_typecore_free(&z);
    }
    /* A palette index can be structurally valid compressed input yet invalid
     * after unfiltering. This failure must free the partially decoded output. */
    const unsigned char invalid_index[] = {0, 3};
    mulib_typecore_buf_t z = zlib_stream(invalid_index, sizeof invalid_index, 1);
    png = png_from_zlib(1, 1, 3, z.data, z.size, 0, 0);
    rejected(png.data, png.size);
    mulib_typecore_free(&png);
    mulib_typecore_free(&z);
}

static void
test_allocation_failures_and_mutations(void)
{
    uint32_t expected[15], state = UINT32_C(0x572319ab);
    mulib_typecore_buf_t png = make_pixels(6, 4, 0, expected);
    size_t before = allocations_live;
    int success = 0;
    for (size_t budget = 0; budget < 64; ++budget) {
        mgx_surface s = {0};
        allocation_budget = budget;
        success = mulib_glx_imagedecode(png.data, png.size, &s);
        allocation_budget = SIZE_MAX;
        if (success) {
            assert(s.pixels[14] == expected[14]);
            mgx_surface_free(&s);
        } else {
            assert(!s.pixels && !s.w && !s.h);
        }
        assert(allocations_live == before);
        if (success)
            break;
    }
    assert(success);
    for (unsigned i = 0; i < 4000; ++i) {
        mulib_typecore_buf_t bad = copy_fixture(&png);
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        bad.data[state % bad.size] ^= (unsigned char)(1u << ((state >> 16) % 8));
        mgx_surface s = {0};
        if (mulib_glx_imagedecode(bad.data, bad.size, &s))
            mgx_surface_free(&s);
        else
            assert(!s.pixels);
        mulib_typecore_free(&bad);
        assert(allocations_live == before);
    }
    mulib_typecore_free(&png);
}

int
main(void)
{
    test_color_types_and_filters();
    test_dynamic_and_encoder();
    test_structure_and_limits();
    test_palette_and_compression_errors();
    test_allocation_failures_and_mutations();
    assert(!allocations_live);
    puts("gfx native PNG: colors, filters, deflate, checksums, limits and failures OK");
    return 0;
}
