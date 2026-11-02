#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_STYLESHEET
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_BIDI
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_PGLAYOUT
#include "mulib-pglayout.h"
#include "mulib-sysfonts.h"
#include <assert.h>

int main(void) {
    mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
    mulib_sysfonts_t *system = mulib_sysfonts_open(NULL);
    mulib_fontshape_t *fonts[3] = {0};
    const char *queries[] = {"sans:lang=en", "sans:lang=ar", "sans:lang=hi"};
    size_t i, j;
    const char rtl[] = "\xd7\x90\xd7\x91 abc def";
    char script[5];
    int ignorable;
    mulib_bidi_paragraph_t *paragraph;
    mulib_bidi_result_t line = {0}, isolated = {0};
    mulib_pglayout_fontset_t set = {u, fonts, 3, "und", -1};
    mulib_pglayout_shaped_line_t shaped = {0};
    mulib_pglayout_item_t *text;
    if (!u || !system) {
        assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS"));
        puts("SKIP: ICU/Fontconfig unavailable");
        mulib_uniproc_close(u); mulib_sysfonts_close(system); return 0;
    }
    assert(!mulib_uniproc_script(u, 0x628, script, &ignorable) && !strcmp(script, "Arab") && !ignorable);
    assert(!mulib_uniproc_script(u, 0x200d, script, &ignorable) && ignorable);
    paragraph = mulib_bidi_paragraph_open(u, rtl, sizeof rtl-1, -1); assert(paragraph);
    assert(!mulib_bidi_paragraph_line(paragraph, 5, 7, &line) && line.paragraph_level == 1);
    assert(!mulib_bidi_resolve(u, rtl+5, 7, -1, &isolated) && isolated.paragraph_level == 0);
    assert(line.count && line.runs[0].start >= 5);
    mulib_bidi_destroy(&line); mulib_bidi_destroy(&isolated);
    assert(mulib_bidi_paragraph_line(paragraph, 1, 1, &line) == -1 && !line.runs);
    assert(mulib_bidi_paragraph_line(paragraph, SIZE_MAX, 1, &line) == -1);
    mulib_bidi_paragraph_close(paragraph);
    {
        const char multiple[] = "abc\n\xd7\x90\xd7\x91 xyz";
        paragraph = mulib_bidi_paragraph_open(u, multiple, sizeof multiple-1, -1); assert(paragraph);
        assert(!mulib_bidi_paragraph_line(paragraph, 0, 3, &line) && line.paragraph_level == 0);
        mulib_bidi_destroy(&line);
        assert(!mulib_bidi_paragraph_line(paragraph, 4, sizeof multiple-1-4, &line) && line.paragraph_level == 1);
        mulib_bidi_destroy(&line);
        assert(mulib_bidi_paragraph_line(paragraph, 0, sizeof multiple-1, &line) == -1);
        mulib_bidi_paragraph_close(paragraph);
    }
    {
        const char supplementary[] = "a\xf0\x9f\x98\x80" "b";
        paragraph = mulib_bidi_paragraph_open(u, supplementary, sizeof supplementary-1, 0); assert(paragraph);
        assert(!mulib_bidi_paragraph_line(paragraph, 1, 4, &line) && line.count == 1 && line.runs[0].length == 4);
        mulib_bidi_destroy(&line);
        assert(mulib_bidi_paragraph_line(paragraph, 2, 3, &line) == -1);
        assert(!mulib_bidi_paragraph_line(paragraph, sizeof supplementary-1, 0, &line) && !line.count);
        mulib_bidi_destroy(&line); mulib_bidi_paragraph_close(paragraph);
    }
    for (i = 0; i < 3; ++i) {
        mulib_sysfonts_font_t match = {0};
        assert(!mulib_sysfonts_match(system, queries[i], &match));
        fonts[i] = mulib_fontshape_open(match.path, (unsigned)match.index, 16*64, NULL);
        mulib_sysfonts_font_destroy(&match);
        if (!fonts[i]) {
            assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS")); puts("SKIP: HarfBuzz unavailable");
            for (j = 0; j < i; ++j) mulib_fontshape_close(fonts[j]);
            mulib_sysfonts_close(system); mulib_uniproc_close(u); return 0;
        }
    }
    assert(mulib_fontshape_covers(fonts[0], 0x110000) == -1);
    assert(!mulib_pglayout_shape_line(&set, "office", 6, 0, 6, &shaped));
    assert(shaped.count == 1 && !strcmp(shaped.runs[0].script, "Latn") && shaped.runs[0].font_index == 0 && shaped.advance > 0);
    mulib_pglayout_shaped_line_destroy(&shaped);
    if (mulib_fontshape_covers(fonts[1], 0x5d0) == 1) {
        assert(!mulib_pglayout_shape_line(&set, rtl, sizeof rtl-1, 5, 7, &shaped) && shaped.paragraph_level == 1);
        for (i = 0; i < shaped.count; ++i) for (j = 0; j < shaped.runs[i].glyphs.count; ++j)
            assert(shaped.runs[i].glyphs.glyphs[j].cluster >= 5 && shaped.runs[i].glyphs.glyphs[j].cluster < 12);
        mulib_pglayout_shaped_line_destroy(&shaped);
    }
    if (mulib_fontshape_covers(fonts[1], 0x633) == 1 && mulib_fontshape_covers(fonts[2], 0x915) == 1) {
        const char mixed[] = "abc \xd8\xb3\xd9\x84\xd8\xa7\xd9\x85 \xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7";
        int arabic = 0, devanagari = 0;
        assert(!mulib_pglayout_shape_line(&set, mixed, sizeof mixed-1, 0, sizeof mixed-1, &shaped));
        for (i = 0; i < shaped.count; ++i) {
            if (!strcmp(shaped.runs[i].script, "Arab") && (shaped.runs[i].level & 1)) arabic = 1;
            if (!strcmp(shaped.runs[i].script, "Deva")) devanagari = 1;
        }
        assert(arabic && devanagari); mulib_pglayout_shaped_line_destroy(&shaped);
        text = mulib_pglayout_new(MULIB_PGLAYOUT_TEXT); assert(text);
        assert(!mulib_pglayout_content(text, mixed));
        assert(!mulib_pglayout_compute_shaped(text, NULL, &set, 16, 40));
        {
            size_t lines;
            const mulib_pglayout_line_t *geometry = mulib_pglayout_lines(text, &lines);
            assert(geometry && lines >= 2);
            for (i = 0; i < lines; ++i) {
                const mulib_pglayout_shaped_line_t *cached = mulib_pglayout_shaped_line(text, i);
                assert(cached && cached->advance == geometry[i].box.width);
            }
            assert(mulib_pglayout_compute_shaped(text, NULL, &set, 17, 80) == -1);
            assert(mulib_pglayout_shaped_line(text, 0)); /* failed compute retained cache */
        }
        assert(!mulib_pglayout_compute(text, NULL, u, 80, mulib_pglayout_font_measure,
                                      &(mulib_pglayout_font_t){u, fonts[0], NULL, "und"}));
        assert(!mulib_pglayout_shaped_line(text, 0));
        mulib_pglayout_destroy(text);
    } else puts("SKIP: multilingual font coverage unavailable");
    assert(mulib_pglayout_shape_line(&set, "e\xcc\x81", 3, 1, 2, &shaped) == -1 && !shaped.runs);
    assert(mulib_pglayout_shape_line(&set, "\xff", 1, 0, 1, &shaped) == -1);
    set.count = SIZE_MAX;
    assert(mulib_pglayout_shape_line(&set, "a", 1, 0, 1, &shaped) == -1);
    set.count = 3;
    assert(!mulib_pglayout_shape_line(&set, NULL, 0, 0, 0, &shaped) && !shaped.count);
    mulib_pglayout_shaped_line_destroy(&shaped);
    for (i = 0; i < 3; ++i) mulib_fontshape_close(fonts[i]);
    mulib_sysfonts_close(system); mulib_uniproc_close(u); return 0;
}
