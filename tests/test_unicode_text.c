#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNINAME
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_BIDI
#include "mulib-uniname.h"
#include "mulib-unibreak.h"
#include "mulib-bidi.h"
#include <assert.h>

int main(void) {
    mulib_uniproc_t *u;
    uint16_t *utf16 = NULL, invalid[] = {0xd800};
    size_t units, bytes, *map = NULL, count;
    uint32_t cp, lo, up;
    unsigned combining;
    int category;
    char *s = NULL;
    mulib_unibreak_boundary_t *b = NULL;
    mulib_bidi_result_t bidi = {0};
    const char emoji[] = "A\xf0\x9f\x98\x80";
    assert(!mulib_uniproc_utf16(emoji, sizeof emoji - 1, &utf16, &units, &map));
    assert(units == 3 && map[0] == 0 && map[1] == 1 && map[2] == 1 && map[3] == 5);
    assert(!mulib_uniproc_utf8(utf16, units, &s, &bytes));
    assert(bytes == 5 && !memcmp(s, emoji, 5));
    free(utf16); free(map); free(s); s = NULL;
    assert(mulib_uniproc_utf8(invalid, 1, &s, &bytes) == -1 && !s);
    assert(mulib_uniproc_utf16("\xc0\x80", 2, &utf16, &units, NULL) == -1);
    assert(mulib_uniproc_utf16(NULL, SIZE_MAX, &utf16, &units, NULL) == -1);
    assert(!mulib_uniproc_utf16(NULL, 0, &utf16, &units, NULL) && units == 0);
    free(utf16);
    assert(!mulib_uniproc_open("/nonexistent/mulib-icu", 0));
    u = mulib_uniproc_open(NULL, 0);
    if (!u) { assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS")); puts("SKIP: ICU runtime unavailable"); return 0; }
    s = mulib_uniproc_normalize(u, MULIB_UNIPROC_NFC, "e\xcc\x81", 3, &bytes);
    assert(s && bytes == 2 && !memcmp(s, "\xc3\xa9", 2)); free(s);
    s = mulib_uniproc_normalize(u, MULIB_UNIPROC_NFKD, "\xef\xac\x83", 3, &bytes);
    assert(s && !strcmp(s, "ffi")); free(s);
    s = mulib_uniproc_normalize(u, MULIB_UNIPROC_NFC, "a\0b", 3, &bytes);
    assert(s && bytes == 3 && !memcmp(s, "a\0b", 3)); free(s);
    s = mulib_uniproc_normalize(u, MULIB_UNIPROC_NFD, NULL, 0, &bytes);
    assert(s && bytes == 0); free(s);
    assert(!mulib_uniproc_normalize(u, MULIB_UNIPROC_NFC, "\xed\xa0\x80", 3, NULL));
    s = mulib_uniproc_case_map(u, MULIB_UNIPROC_FOLD, NULL, "Stra\xc3\x9f" "e", 7, &bytes);
    assert(s && !strcmp(s, "strasse")); free(s);
    s = mulib_uniproc_case_map(u, MULIB_UNIPROC_LOWER, "tr", "I", 1, &bytes);
    assert(s && bytes == 2 && !strcmp(s, "\xc4\xb1")); free(s);
    s = mulib_uniproc_case_map(u, MULIB_UNIPROC_UPPER, NULL, "a", 1, NULL);
    assert(s && !strcmp(s, "A")); free(s);
    assert(!mulib_uniproc_properties(u, 0x301, &category, &combining, NULL, NULL) && combining == 230);
    assert(!mulib_uniproc_properties(u, 'a', &category, NULL, &lo, &up) && lo == 'a' && up == 'A');
    assert(mulib_uniproc_properties(u, 0x110000, NULL, NULL, NULL, NULL) == -1);
    s = mulib_uniname_name(u, 0x1f600, 0);
    assert(s && !strcmp(s, "GRINNING FACE"));
    assert(!mulib_uniname_lookup(u, s, 0, &cp) && cp == 0x1f600); free(s);
    assert(!mulib_uniname_lookup(u, "cjk unified ideograph-4e00", 0, &cp) && cp == 0x4e00);
    assert(!mulib_uniname_lookup(u, "HANGUL SYLLABLE GA", 0, &cp) && cp == 0xac00);
    cp = 42; assert(mulib_uniname_lookup(u, "NOT A UNICODE NAME", 0, &cp) == -1 && cp == 42);
    assert(!mulib_uniname_name(u, 0x110000, 0));
    assert(!mulib_unibreak_find(u, MULIB_UNIBREAK_GRAPHEME, NULL, "e\xcc\x81", 3, &b, &count));
    assert(count == 2 && b[0].offset == 0 && b[1].offset == 3); free(b);
    {
        const char family[] = "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7";
        assert(!mulib_unibreak_find(u, MULIB_UNIBREAK_GRAPHEME, NULL, family, sizeof family-1, &b, &count));
        assert(count == 2 && b[1].offset == sizeof family-1); free(b);
    }
    assert(!mulib_unibreak_find(u, MULIB_UNIBREAK_LINE, NULL, "ab cd\r\nef", 9, &b, &count));
    assert(count == 4 && b[1].offset == 3 && b[2].offset == 7 && b[2].status >= 100 && b[3].offset == 9); free(b);
    assert(!mulib_unibreak_find(u, MULIB_UNIBREAK_WORD, "en", NULL, 0, &b, &count));
    assert(count == 1 && b[0].offset == 0); free(b);
    assert(mulib_unibreak_find(u, MULIB_UNIBREAK_LINE, NULL, "\xff", 1, &b, &count) == -1);
    assert(!mulib_bidi_resolve(u, "abc \xd7\x90\xd7\x91", 8, -1, &bidi));
    assert(bidi.count == 2 && bidi.paragraph_level == 0);
    assert(bidi.runs[0].start == 0 && bidi.runs[0].length == 4 && bidi.runs[0].level == 0);
    assert(bidi.runs[1].start == 4 && bidi.runs[1].length == 4 && bidi.runs[1].level == 1);
    mulib_bidi_destroy(&bidi);
    assert(!mulib_bidi_resolve(u, "\xd7\x90\xd7\x91 12", 7, -1, &bidi));
    assert(bidi.paragraph_level == 1 && bidi.count == 2 && bidi.runs[0].start == 5 && bidi.runs[0].level == 2);
    mulib_bidi_destroy(&bidi);
    assert(!mulib_bidi_resolve(u, NULL, 0, -2, &bidi) && bidi.count == 0 && bidi.paragraph_level == 1);
    mulib_bidi_destroy(&bidi);
    assert(mulib_bidi_resolve(u, "\xff", 1, 0, &bidi) == -1);
    {
        /* Deterministic combinations of strong/weak/neutral characters,
         * isolates, paired brackets and supplementary scalars. */
        const uint32_t scalars[] = {'a', ' ', '1', '(', ')', 0x5d0, 0x628, 0x301, 0x2067, 0x2069, 0x1f600};
        unsigned state = 123;
        size_t trial, i, j;
        for (trial = 0; trial < 150; ++trial) {
            char source[128];
            size_t length = 0, total = 0;
            for (i = 0; i < 20; ++i) {
                state = state*1664525u+1013904223u;
                length += mulib_utf8_encode(scalars[state % (sizeof scalars/sizeof *scalars)],
                                           (unsigned char *)source+length, sizeof source-length);
            }
            assert(!mulib_bidi_resolve(u, source, length, -1, &bidi));
            for (i = 0; i < bidi.count; ++i) {
                size_t begin = bidi.runs[i].start, end = begin+bidi.runs[i].length;
                assert(bidi.runs[i].length && end <= length && bidi.runs[i].level <= 126);
                assert(((unsigned char)source[begin] & 0xc0) != 0x80);
                assert(end == length || ((unsigned char)source[end] & 0xc0) != 0x80);
                for (j = 0; j < i; ++j)
                    assert(end <= bidi.runs[j].start || begin >= bidi.runs[j].start+bidi.runs[j].length);
                total += bidi.runs[i].length;
            }
            assert(total == length); mulib_bidi_destroy(&bidi);
            assert(!mulib_unibreak_find(u, MULIB_UNIBREAK_GRAPHEME, NULL, source, length, &b, &count));
            assert(count && b[0].offset == 0 && b[count-1].offset == length);
            for (i = 1; i < count; ++i) {
                assert(b[i].offset > b[i-1].offset && b[i].offset <= length);
                assert(b[i].offset == length || ((unsigned char)source[b[i].offset] & 0xc0) != 0x80);
            }
            free(b);
            for (i = 0; i < 4; ++i) {
                size_t normalized_length, second_length;
                char *normalized = mulib_uniproc_normalize(u, (mulib_uniproc_form_t)i, source, length, &normalized_length);
                char *second;
                assert(normalized);
                second = mulib_uniproc_normalize(u, (mulib_uniproc_form_t)i, normalized, normalized_length, &second_length);
                assert(second && second_length == normalized_length && !memcmp(normalized, second, second_length));
                free(normalized); free(second);
            }
        }
    }
    {
        mulib_uniproc_t *other = mulib_uniproc_open(NULL, 0);
        assert(other);
        mulib_uniproc_close(u);
        s = mulib_uniproc_normalize(other, MULIB_UNIPROC_NFC, "e\xcc\x81", 3, NULL);
        assert(s && !strcmp(s, "\xc3\xa9")); free(s);
        mulib_uniproc_close(other);
    }
    return 0;
}
