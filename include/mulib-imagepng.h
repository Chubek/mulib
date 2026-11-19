/** @file mulib-imagepng.h
 * @brief RGB PNG encoding and bounded, noninterlaced 8-bit PNG decoding.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_IMAGEPNG once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-imagepng.md for scope and limitations.
 */
#ifndef MULIB_IMAGEPNG_H
#define MULIB_IMAGEPNG_H
#include "mulib-typecore.h"
#include "mulib-deflate.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_imagepng_encode_rgb(const unsigned char *pixels,uint32_t width,uint32_t height,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif

/* The additive decoder is static inline. Calling it requires the existing
 * TYPECORE, DEFLATE and BUFFER implementations in exactly one translation unit.
 * The encoder's original implementation/dependency contract is unchanged. */
typedef struct {
    size_t max_input_bytes, max_pixels, max_output_bytes, max_work_bytes;
    uint32_t max_width, max_height;
    size_t max_chunks;
} mulib_imagepng_limits_t;

typedef struct {
    unsigned char *pixels; /* Owned, straight RGBA8; no color-space conversion. */
    uint32_t width, height;
    size_t stride;
} mulib_imagepng_rgba_t;

#ifndef MULIB_IMAGEPNG_MALLOC
#define MULIB_IMAGEPNG_MALLOC malloc
#endif
#ifndef MULIB_IMAGEPNG_FREE
#define MULIB_IMAGEPNG_FREE free
#endif

static inline mulib_imagepng_limits_t
mulib_imagepng_limits_default(void)
{
    mulib_imagepng_limits_t limits = {
        (size_t)128 * 1024 * 1024, (size_t)16 * 1024 * 1024,
        (size_t)64 * 1024 * 1024, (size_t)512 * 1024 * 1024,
        16384, 16384, 65536
    };
    return limits;
}

static inline void
mulib_imagepng_rgba_free(mulib_imagepng_rgba_t *image)
{
    if (image) {
        MULIB_IMAGEPNG_FREE(image->pixels);
        memset(image, 0, sizeof *image);
    }
}

static inline uint32_t
mulib_imagepng_be32_(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16
        | (uint32_t)p[2] << 8 | p[3];
}

static inline int
mulib_imagepng_tag_(const unsigned char *tag)
{
    for (unsigned i = 0; i < 4; ++i)
        if (!((tag[i] >= 'A' && tag[i] <= 'Z')
              || (tag[i] >= 'a' && tag[i] <= 'z')))
            return 0;
    return !(tag[2] & 32u); /* The PNG reserved bit must be zero. */
}

/* Upper bound on the existing byte-buffer allocator's capacity for n bytes. */
static inline int
mulib_imagepng_capacity_(size_t n, size_t *capacity)
{
    size_t cap = 64;
    while (cap < n) {
        if (cap > SIZE_MAX / 2)
            return 0;
        cap *= 2;
    }
    *capacity = cap;
    return 1;
}

static inline unsigned
mulib_imagepng_paeth_(unsigned a, unsigned b, unsigned c)
{
    int p = (int)a + (int)b - (int)c;
    int pa = abs(p - (int)a), pb = abs(p - (int)b), pc = abs(p - (int)c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

/* Noninterlaced depth-8 types 0/2/3/4/6, all five filters, optional PLTE/tRNS.
 * Zlib uses a 32 KiB window; packed/16-bit samples, Adam7 and APNG are rejected.
 * Every chunk CRC and the zlib Adler-32 are verified. Unknown ancillary chunks
 * are ignored without interpretation, unknown critical chunks are rejected.
 * out must be zero/empty. No output changes on malformed input or allocation
 * failure. Limits cover variable-size allocations, excluding caller-owned input,
 * fixed stack storage and allocator bookkeeping. Fields are inclusive; zero
 * forbids work. */
static inline int
mulib_imagepng_decode_rgba(const void *data, size_t size,
                           mulib_imagepng_rgba_t *out,
                           const mulib_imagepng_limits_t *limits)
{
    static const unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    const unsigned char *p = (const unsigned char *)data;
    mulib_imagepng_limits_t defaults = mulib_imagepng_limits_default();
    mulib_imagepng_rgba_t image = {NULL, 0, 0, 0};
    mulib_buffer_t raw = {NULL, 0, 0};
    unsigned char palette[256][4], *compressed = NULL;
    size_t at = 8, chunks = 0, compressed_size = 0;
    size_t row_bytes = 0, raw_size = 0, output_size = 0, capacity;
    uint32_t width = 0, height = 0;
    unsigned type = 0, channels = 0, entries = 0;
    unsigned transparent[3] = {0, 0, 0};
    int ihdr = 0, plte = 0, trns = 0, idat = 0, idat_end = 0, iend = 0;
    int ok = 0;
    if (!limits)
        limits = &defaults;
    if (!p || !out || out->pixels || out->width || out->height || out->stride
        || size < 8 || size > limits->max_input_bytes
        || memcmp(p, signature, 8))
        return 0;
    for (unsigned i = 0; i < 256; ++i)
        palette[i][3] = 255;
    while (at < size) {
        if (size - at < 12 || chunks >= limits->max_chunks)
            return 0;
        ++chunks;
        size_t n = mulib_imagepng_be32_(p + at);
        const unsigned char *tag = p + at + 4, *payload = p + at + 8;
        if (n > UINT32_C(0x7fffffff) || n > size - at - 12
            || !mulib_imagepng_tag_(tag)
            || mulib_typecore_crc32(tag, n + 4)
                != mulib_imagepng_be32_(payload + n))
            return 0;
        if (!ihdr && memcmp(tag, "IHDR", 4))
            return 0;
        if (!memcmp(tag, "IHDR", 4)) {
            if (ihdr || n != 13)
                return 0;
            width = mulib_imagepng_be32_(payload);
            height = mulib_imagepng_be32_(payload + 4);
            type = payload[9];
            if (!width || !height || width > UINT32_C(0x7fffffff)
                || height > UINT32_C(0x7fffffff)
                || width > limits->max_width || height > limits->max_height
                || payload[8] != 8 || payload[10] || payload[11] || payload[12])
                return 0;
            switch (type) {
            case 0: case 3: channels = 1; break;
            case 2: channels = 3; break;
            case 4: channels = 2; break;
            case 6: channels = 4; break;
            default: return 0;
            }
            if ((size_t)width > SIZE_MAX / height
                || (size_t)width * height > limits->max_pixels
                || (size_t)width * height > limits->max_output_bytes / 4
                || (size_t)width > (SIZE_MAX - 1) / channels)
                return 0;
            output_size = (size_t)width * height * 4;
            row_bytes = (size_t)width * channels;
            if ((size_t)height > SIZE_MAX / (row_bytes + 1))
                return 0;
            raw_size = (row_bytes + 1) * height;
            ihdr = 1;
        } else if (!memcmp(tag, "PLTE", 4)) {
            if (plte || trns || idat || type == 0 || type == 4
                || !n || n > 768 || n % 3)
                return 0;
            entries = (unsigned)(n / 3);
            for (unsigned i = 0; i < entries; ++i)
                memcpy(palette[i], payload + (size_t)i * 3, 3);
            plte = 1;
        } else if (!memcmp(tag, "tRNS", 4)) {
            if (trns || idat)
                return 0;
            if (type == 3) {
                if (!plte || !n || n > entries)
                    return 0;
                for (size_t i = 0; i < n; ++i)
                    palette[i][3] = payload[i];
            } else if (type == 0 || type == 2) {
                unsigned count = type == 0 ? 1 : 3;
                if (n != count * 2)
                    return 0;
                for (unsigned i = 0; i < count; ++i) {
                    if (payload[i * 2])
                        return 0;
                    transparent[i] = payload[i * 2 + 1];
                }
            } else {
                return 0;
            }
            trns = 1;
        } else if (!memcmp(tag, "IDAT", 4)) {
            if (idat_end || (type == 3 && !plte)
                || n > SIZE_MAX - compressed_size)
                return 0;
            compressed_size += n;
            idat = 1;
        } else if (!memcmp(tag, "IEND", 4)) {
            if (n || !idat || size - at != 12)
                return 0;
            iend = 1;
        } else {
            if (!(tag[0] & 32u) || !memcmp(tag, "acTL", 4)
                || !memcmp(tag, "fcTL", 4) || !memcmp(tag, "fdAT", 4))
                return 0;
        }
        if (idat && memcmp(tag, "IDAT", 4))
            idat_end = 1;
        at += n + 12;
    }
    if (!iend || compressed_size < 6
        || !mulib_imagepng_capacity_(raw_size, &capacity)
        || output_size > limits->max_work_bytes
        || compressed_size > limits->max_work_bytes - output_size
        || capacity > (limits->max_work_bytes - output_size - compressed_size) / 2)
        return 0;
    compressed = (unsigned char *)MULIB_IMAGEPNG_MALLOC(compressed_size);
    if (!compressed)
        return 0;
    size_t copied = 0;
    for (at = 8; at < size;) {
        size_t n = mulib_imagepng_be32_(p + at);
        if (!memcmp(p + at + 4, "IDAT", 4)) {
            memcpy(compressed + copied, p + at + 8, n);
            copied += n;
        }
        at += n + 12;
    }
    /* Restrict CMF to a 32 KiB window: the existing raw decoder enforces that
     * maximum, but does not enforce a smaller advertised zlib window. */
    if (compressed[0] != 0x78 || (compressed[1] & 32u)
        || (((unsigned)compressed[0] << 8) | compressed[1]) % 31
        || mulib_deflate_decode(compressed + 2, compressed_size - 6,
                                raw_size, &raw) != MULIB_DEFLATE_OK
        || raw.length != raw_size
        || mulib_typecore_adler32(raw.data, raw.length)
            != mulib_imagepng_be32_(compressed + compressed_size - 4))
        goto done;
    MULIB_IMAGEPNG_FREE(compressed);
    compressed = NULL;
    image.pixels = (unsigned char *)MULIB_IMAGEPNG_MALLOC(output_size);
    if (!image.pixels)
        goto done;
    image.width = width;
    image.height = height;
    image.stride = (size_t)width * 4;
    for (uint32_t y = 0; y < height; ++y) {
        unsigned char *row = raw.data + (size_t)y * (row_bytes + 1) + 1;
        const unsigned char *previous = y ? row - row_bytes - 1 : NULL;
        unsigned filter = row[-1];
        if (filter > 4)
            goto done;
        for (size_t x = 0; x < row_bytes; ++x) {
            unsigned a = x >= channels ? row[x - channels] : 0;
            unsigned b = previous ? previous[x] : 0;
            unsigned c = previous && x >= channels ? previous[x - channels] : 0;
            unsigned predictor = filter == 1 ? a : filter == 2 ? b
                : filter == 3 ? (a + b) / 2
                : filter == 4 ? mulib_imagepng_paeth_(a, b, c) : 0;
            row[x] = (unsigned char)(row[x] + predictor);
        }
        for (uint32_t x = 0; x < width; ++x) {
            const unsigned char *src = row + (size_t)x * channels;
            unsigned char *dst = image.pixels + (size_t)y * image.stride
                + (size_t)x * 4;
            if (type == 3) {
                if (*src >= entries)
                    goto done;
                memcpy(dst, palette[*src], 4);
            } else if (type == 0 || type == 4) {
                dst[0] = dst[1] = dst[2] = src[0];
                dst[3] = type == 4 ? src[1]
                    : trns && src[0] == transparent[0] ? 0 : 255;
            } else {
                memcpy(dst, src, 3);
                dst[3] = type == 6 ? src[3]
                    : trns && src[0] == transparent[0]
                        && src[1] == transparent[1] && src[2] == transparent[2]
                    ? 0 : 255;
            }
        }
    }
    *out = image;
    memset(&image, 0, sizeof image);
    ok = 1;
done:
    MULIB_IMAGEPNG_FREE(compressed);
    mulib_buffer_free(&raw);
    mulib_imagepng_rgba_free(&image);
    return ok;
}
#endif /* MULIB_IMAGEPNG_H */
#if defined(MULIB_IMPL_IMAGEPNG) && !defined(MULIB_IMAGEPNG_H_IMPLEMENTED)
#define MULIB_IMAGEPNG_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_imagepng_chunk_(mulib_typecore_buf_t *b,const char tag[4],const void *p,size_t n){if(n>UINT32_MAX)return 0;mulib_typecore_buf_t t={0};int ok=mulib_typecore_append(&t,tag,4)&&mulib_typecore_append(&t,p,n)&&mulib_typecore_u32be(b,(uint32_t)n)&&mulib_typecore_append(b,t.data,t.size)&&mulib_typecore_u32be(b,mulib_typecore_crc32(t.data,t.size));mulib_typecore_free(&t);return ok;}
int mulib_imagepng_encode_rgb(const unsigned char *src,uint32_t width,uint32_t height,mulib_typecore_buf_t *out){if(!src||!out||!width||!height||width>100000||height>100000 )return 0;size_t stride=(size_t)width*3;if(height>SIZE_MAX/(stride+1))return 0;size_t rawlen=(stride+1)*(size_t)height;if(rawlen>128*1024*1024)return 0;
 mulib_typecore_buf_t raw={0},z={0},b={0};const unsigned char sig[]={137,80,78,71,13,10,26,10};unsigned char ihdr[13]={0};ihdr[0]=(unsigned char)(width>>24);ihdr[1]=(unsigned char)(width>>16);ihdr[2]=(unsigned char)(width>>8);ihdr[3]=(unsigned char)width;ihdr[4]=(unsigned char)(height>>24);ihdr[5]=(unsigned char)(height>>16);ihdr[6]=(unsigned char)(height>>8);ihdr[7]=(unsigned char)height;ihdr[8]=8;ihdr[9]=2;
 for(uint32_t row=0;row<height;row++){if(!mulib_typecore_byte(&raw,0)||!mulib_typecore_append(&raw,src+(size_t)row*stride,stride))goto fail;}
 if(!mulib_typecore_byte(&z,0x78)||!mulib_typecore_byte(&z,0x01))goto fail;
 for(size_t i=0;i<raw.size;){size_t n=raw.size-i;if(n>65535)n=65535;uint16_t k=(uint16_t)n;unsigned char head[5]={(unsigned char)(i+n==raw.size),(unsigned char)k,(unsigned char)(k>>8),(unsigned char)(~k),(unsigned char)((~k)>>8)};if(!mulib_typecore_append(&z,head,5)||!mulib_typecore_append(&z,raw.data+i,n))goto fail;i+=n;}
 if(!mulib_typecore_u32be(&z,mulib_typecore_adler32(raw.data,raw.size))||!mulib_typecore_append(&b,sig,8)||!mulib_imagepng_chunk_(&b,"IHDR",ihdr,13)||!mulib_imagepng_chunk_(&b,"IDAT",z.data,z.size)||!mulib_imagepng_chunk_(&b,"IEND",NULL,0)||!mulib_typecore_append(out,b.data,b.size))goto fail;
 mulib_typecore_free(&raw);mulib_typecore_free(&z);mulib_typecore_free(&b);return 1;
 fail:mulib_typecore_free(&raw);mulib_typecore_free(&z);mulib_typecore_free(&b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
