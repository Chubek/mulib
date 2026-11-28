/* Offline UTF-8 scalar, chunk boundary, budget and ownership regressions. */
#include <assert.h>
#include <stdlib.h>

static int fail_alloc;
static size_t alloc_count, last_size;

static void *web_textcodec_malloc(size_t n)
{
    ++alloc_count;
    last_size = n;
    return fail_alloc ? NULL : malloc(n);
}

#define malloc web_textcodec_malloc
#include "mulib-web-textcodec.h"
#undef malloc

#define SPAN(p, n) ((muba_span){(const unsigned char *)(p), (n)})

static void matches(const muba_buf *out, const void *data, size_t len)
{
    assert(out->len == len);
    assert(!len || memcmp(out->data, data, len) == 0);
}

/* Try every partition of each short fixture, optionally finishing with an
 * empty chunk. Output timing can differ but the concatenation must not. */
static void partitions(muba_span in, muba_span expected, int ignore_bom)
{
    assert(in.len > 0 && in.len <= 12);
    for (unsigned mask = 0; mask < (1u << in.len); ++mask) {
        mulib_web_textcodec_decoder d;
        mulib_web_textcodec_decoder_init(&d, expected.len, 0, ignore_bom);
        muba_buf out = {0}, combined = {0};
        size_t start = 0;
        for (size_t i = 0; i < in.len; ++i) {
            if (i + 1 != in.len && !(mask & (1u << i)))
                continue;
            int finish = i + 1 == in.len && (mask & (1u << i));
            assert(mulib_web_textcodec_decoder_feed(
                       &d, SPAN(in.data + start, i + 1 - start), finish, &out)
                   == MULIB_WEB_TEXTCODEC_OK);
            assert(muba_append(&combined, SPAN(out.data, out.len)));
            start = i + 1;
        }
        if (!d.finished) {
            assert(mulib_web_textcodec_decoder_feed(&d, SPAN(NULL, 0), 1, &out)
                   == MULIB_WEB_TEXTCODEC_OK);
            assert(muba_append(&combined, SPAN(out.data, out.len)));
        }
        assert(d.finished && d.remaining == 0);
        matches(&combined, expected.data, expected.len);
        muba_free(&combined);
        muba_free(&out);
    }
}

static void test_partitions(void)
{
    static const unsigned char scalars[] = {
        0, 0x7f, 0xc2, 0x80, 0xe0, 0xa0, 0x80, 0xf4, 0x8f, 0xbf, 0xbf
    };
    static const char bom[] = "\xef\xbb\xbf";
    static const char replacement[] = "\xef\xbf\xbd";
    partitions(SPAN(scalars, sizeof scalars), SPAN(scalars, sizeof scalars), 0);
    partitions(muba_str("\xf0\x90\x80"), muba_str(replacement), 0);
    partitions(muba_str("\xe2\x82" "X"), muba_str("\xef\xbf\xbd" "X"), 0);
    partitions(muba_str("\xf0\x90\xe2\x82\xac"), muba_str("\xef\xbf\xbd\xe2\x82\xac"), 0);
    partitions(muba_str("\xe0\x9f\x80"), muba_str("\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd"), 0);
    partitions(muba_str("\xed\xa0\x80"), muba_str("\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd"), 0);
    partitions(muba_str("\xf4\x90\xbf\xbf"),
               muba_str("\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd"), 0);
    partitions(muba_str("\xc0\xaf\xff"), muba_str("\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd"), 0);
    partitions(muba_str(bom), SPAN(NULL, 0), 0);
    partitions(muba_str(bom), muba_str(bom), 1);
    partitions(muba_str("\xef\xbb\xbf\xef\xbb\xbf"), muba_str(bom), 0);
    partitions(muba_str("\xff\xef\xbb\xbf"), muba_str("\xef\xbf\xbd\xef\xbb\xbf"), 0);
}

static void test_failures(void)
{
    mulib_web_textcodec_decoder d;
    muba_buf out = {0};
    assert(muba_append(&out, muba_str("old")));
    mulib_web_textcodec_decoder_init(&d, 4, 1, 0);
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("\xe2\x82"), 0, &out)
           == MULIB_WEB_TEXTCODEC_OK);
    matches(&out, NULL, 0);
    assert(muba_append(&out, muba_str("old")));
    unsigned char *old = out.data;
    size_t before = alloc_count;
    assert(mulib_web_textcodec_decoder_feed(&d, SPAN(NULL, 0), 1, &out)
           == MULIB_WEB_TEXTCODEC_MALFORMED);
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("x"), 0, &out)
           == MULIB_WEB_TEXTCODEC_MALFORMED);
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("\xac" "ab"), 1, &out)
           == MULIB_WEB_TEXTCODEC_LIMIT);
    assert(mulib_web_textcodec_decoder_feed(&d, SPAN(NULL, 1), 0, &out)
           == MULIB_WEB_TEXTCODEC_INVALID);
    assert(mulib_web_textcodec_decoder_feed(NULL, SPAN(NULL, 0), 0, &out)
           == MULIB_WEB_TEXTCODEC_INVALID);
    assert(mulib_web_textcodec_decoder_feed(&d, SPAN(NULL, 0), 0, NULL)
           == MULIB_WEB_TEXTCODEC_INVALID);
    assert(alloc_count == before);
    fail_alloc = 1;
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("\xac"), 1, &out)
           == MULIB_WEB_TEXTCODEC_NO_MEMORY);
    fail_alloc = 0;
    assert(alloc_count == before + 1 && last_size == 3);
    assert(out.data == old);
    matches(&out, "old", 3);
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("\xac"), 0, &out)
           == MULIB_WEB_TEXTCODEC_OK);
    matches(&out, "\xe2\x82\xac", 3);
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("xy"), 1, &out)
           == MULIB_WEB_TEXTCODEC_LIMIT);
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("x"), 1, &out)
           == MULIB_WEB_TEXTCODEC_OK);
    assert(out.cap == 1 && last_size == 1);
    assert(mulib_web_textcodec_decoder_feed(&d, SPAN(NULL, 0), 1, &out)
           == MULIB_WEB_TEXTCODEC_INVALID);
    matches(&out, "x", 1);

    mulib_web_textcodec_decoder_init(&d, 0, 0, 0);
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("\xff"), 1, &out)
           == MULIB_WEB_TEXTCODEC_LIMIT);
    matches(&out, "x", 1);
    assert(mulib_web_textcodec_decoder_feed(&d, muba_str("\xef\xbb\xbf"), 1, &out)
           == MULIB_WEB_TEXTCODEC_OK);
    matches(&out, NULL, 0);
    muba_free(&out);
}

static void test_oneshot(void)
{
    muba_buf out = {0};
    const uint16_t units[] = {0xd800, 0, 0xd83d, 0xde00, 0xdc00};
    const char bytes[] = "\xef\xbf\xbd\0\xf0\x9f\x98\x80\xef\xbf\xbd";
    assert(mulib_web_textcodec_encode(units, sizeof units / sizeof *units, &out));
    matches(&out, bytes, sizeof bytes - 1);
    assert(mulib_web_textcodec_decode(SPAN(out.data, out.len), &out));
    matches(&out, bytes, sizeof bytes - 1);
    assert(mulib_web_textcodec_decode(muba_str("\xef\xbb\xbf\xe2\x82"), &out));
    matches(&out, "\xef\xbb\xbf\xef\xbf\xbd", 6);
    assert(!mulib_web_textcodec_decode(SPAN(NULL, 1), &out));
    matches(&out, "\xef\xbb\xbf\xef\xbf\xbd", 6);
    assert(mulib_web_textcodec_decode(SPAN(NULL, 0), &out));
    matches(&out, NULL, 0);
    muba_free(&out);
}

int main(void)
{
    test_partitions();
    test_failures();
    test_oneshot();
    return 0;
}
