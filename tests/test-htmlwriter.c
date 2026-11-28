/* Standalone API contract test for mulib-htmlwriter. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_HTMLWRITER
#include "mulib-htmlwriter.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    static const char *const malformed[] = {
        "x\x80",                 /* stray continuation */
        "x\xc0\xaf",             /* overlong ASCII with illegal lead */
        "x\xe0\x80\xaf",         /* overlong ASCII with legal lead */
        "x\xe2\x82",             /* truncated three-byte sequence */
        "x\xe2\x28\xa1",         /* bad continuation */
        "x\xed\xa0\x80",         /* surrogate */
        "x\xf4\x90\x80\x80"      /* beyond U+10FFFF */
    };
    mulib_typecore_buf_t b = {0};
    assert(mulib_htmlwriter_document("", "", &b));
    assert(mulib_typecore_ends(&b));
    assert(strstr((char *)b.data, "<title></title></head><body><p></p>"));
    mulib_typecore_free(&b);

    assert(mulib_typecore_text(&b, "kept"));
    assert(mulib_typecore_ends(&b));
    unsigned char *data = b.data;
    size_t capacity = b.capacity;
    for (size_t i = 0; i < sizeof malformed / sizeof malformed[0]; ++i) {
        assert(!mulib_htmlwriter_document(malformed[i], "body", &b));
        assert(b.data == data && b.size == 4 && b.capacity == capacity);
        assert(!memcmp(b.data, "kept", 5));
        assert(!mulib_htmlwriter_document("title", malformed[i], &b));
        assert(b.data == data && b.size == 4 && b.capacity == capacity);
        assert(!memcmp(b.data, "kept", 5));
    }

    assert(mulib_htmlwriter_document("a<\xc2\xa2", "x&\xe2\x82\xac\xf0\x9f\x98\x80", &b));
    assert(mulib_typecore_ends(&b));
    assert(!memcmp(b.data, "kept<!doctype html>", sizeof "kept<!doctype html>" - 1));
    assert(strstr((char *)b.data, "<title>a&lt;\xc2\xa2</title>"));
    assert(strstr((char *)b.data, "<p>x&amp;\xe2\x82\xac\xf0\x9f\x98\x80</p>"));
    mulib_typecore_free(&b);
    return 0;
}
