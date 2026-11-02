/* Optional official Unicode corpus runner; no bundled third-party data.
 * Usage: unicode_conformance normalization|grapheme|bidi FILE */
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_BIDI
#include "mulib-unibreak.h"
#include "mulib-bidi.h"
#include <errno.h>
#ifdef MULIB_TEXT_CONFORMANCE_NATIVE
#include <unicode/ubidi.h>
static size_t native_disagreements;
static int compare_native(mulib_uniproc_t *u, const char *text, size_t n, int base,
                           const mulib_bidi_result_t *result) {
    uint16_t *utf16 = NULL;
    size_t units, *map = NULL;
    UBiDi *native = ubidi_open();
    UErrorCode error = U_ZERO_ERROR;
    int32_t count, i;
    int status = -1;
    (void)u;
    if (!native || mulib_uniproc_utf16(text, n, &utf16, &units, &map)) goto done;
    ubidi_setPara(native, utf16, (int32_t)units, (UBiDiLevel)(base == -1 ? UBIDI_DEFAULT_LTR : base), NULL, &error);
    if (U_FAILURE(error)) goto done;
    count = ubidi_countRuns(native, &error);
    if (U_FAILURE(error) || count < 0 || (size_t)count != result->count || ubidi_getParaLevel(native) != result->paragraph_level) goto done;
    for (i = 0; i < count; ++i) {
        int32_t start, length;
        (void)ubidi_getVisualRun(native, i, &start, &length);
        if (result->runs[i].start != map[start] || result->runs[i].length != map[start+length]-map[start] ||
            result->runs[i].level != ubidi_getLevelAt(native, start)) goto done;
    }
    status = 0;
done:
    if (native) ubidi_close(native);
    free(utf16); free(map); return status;
}
#endif

#define TEXT_CORPUS_LIMIT 4096
static int scalar_list(char *s, char *text, size_t *length, size_t *offsets, size_t *count) {
    size_t n = 0, k = 0;
    while (*s) {
        unsigned long cp;
        char *end;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
        if (!*s) break;
        errno = 0; cp = strtoul(s, &end, 16);
        if (end == s || errno || cp > 0x10ffff || k == TEXT_CORPUS_LIMIT) return -1;
        offsets[k++] = n;
        {
            size_t used = mulib_utf8_encode((uint32_t)cp, (unsigned char *)text+n, TEXT_CORPUS_LIMIT*4-n);
            if (!used) return -1;
            n += used;
        }
        s = end;
    }
    text[n] = 0; offsets[k] = n; *length = n; *count = k; return 0;
}
static int normalization(mulib_uniproc_t *u, char *line) {
    char text[5][TEXT_CORPUS_LIMIT*4+1];
    size_t lengths[5], offsets[TEXT_CORPUS_LIMIT+1], count, i, form;
    char *s = line;
    for (i = 0; i < 5; ++i) {
        char *end = strchr(s, ';');
        if (!end) return -1;
        *end = 0;
        if (scalar_list(s, text[i], &lengths[i], offsets, &count)) return -1;
        s = end+1;
    }
    for (form = 0; form < 4; ++form) for (i = 0; i < 5; ++i) {
        size_t target = form == 0 ? (i < 3 ? 1 : 3) : form == 1 ? (i < 3 ? 2 : 4) : form == 2 ? 3 : 4;
        size_t length;
        char *normalized = mulib_uniproc_normalize(u, (mulib_uniproc_form_t)form, text[i], lengths[i], &length);
        int fail = !normalized || length != lengths[target] || memcmp(normalized, text[target], length);
        free(normalized); if (fail) return -1;
    }
    return 0;
}
static int grapheme(mulib_uniproc_t *u, char *s) {
    char text[TEXT_CORPUS_LIMIT*4+1];
    size_t offsets[TEXT_CORPUS_LIMIT+1], count = 0, n = 0, actual, i;
    mulib_unibreak_boundary_t *boundaries = NULL;
    int fail;
    while (*s) {
        char *end;
        unsigned long cp;
        size_t used;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
        if (!*s) break;
        if ((unsigned char)s[0] == 0xc3 && (unsigned char)s[1] == 0xb7) {
            if (count > TEXT_CORPUS_LIMIT) return -1;
            offsets[count++] = n; s += 2; continue;
        }
        if ((unsigned char)s[0] == 0xc3 && (unsigned char)s[1] == 0x97) { s += 2; continue; }
        errno = 0; cp = strtoul(s, &end, 16);
        if (end == s || errno || cp > 0x10ffff) return -1;
        used = mulib_utf8_encode((uint32_t)cp, (unsigned char *)text+n, TEXT_CORPUS_LIMIT*4-n);
        if (!used) return -1;
        n += used; s = end;
    }
    if (mulib_unibreak_find(u, MULIB_UNIBREAK_GRAPHEME, NULL, text, n, &boundaries, &actual)) return -1;
    fail = actual != count;
    if (!fail) for (i = 0; i < count; ++i) if (offsets[i] != boundaries[i].offset) { fail = 1; break; }
    free(boundaries); return fail ? -1 : 0;
}
static int bidi(mulib_uniproc_t *u, char *line) {
    char text[TEXT_CORPUS_LIMIT*4+1], *fields[5], *s = line, *end;
    size_t offsets[TEXT_CORPUS_LIMIT+1], count, n, i, j, k = 0, expected_count = 0;
    int expected[TEXT_CORPUS_LIMIT], actual[TEXT_CORPUS_LIMIT], order[TEXT_CORPUS_LIMIT];
    long base, paragraph_level;
    mulib_bidi_result_t result = {0};
    for (i = 0; i < 5; ++i) {
        fields[i] = s; end = strchr(s, ';');
        if (i < 4 && !end) return -1;
        if (end) { *end = 0; s = end+1; }
    }
    if (scalar_list(fields[0], text, &n, offsets, &count)) return -1;
    base = strtol(fields[1], NULL, 10); paragraph_level = strtol(fields[2], NULL, 10);
    s = fields[3];
    for (i = 0; i < count; ++i) {
        while (*s == ' ') ++s;
        if (*s == 'x') { expected[i] = -1; ++s; }
        else { long value = strtol(s, &end, 10); if (end == s || value < 0 || value > 126) return -1; expected[i] = (int)value; s = end; }
        actual[i] = -2;
    }
    s = fields[4];
    while (*s) {
        long value;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
        if (!*s) break;
        value = strtol(s, &end, 10);
        if (end == s || value < 0 || (size_t)value >= count || expected_count == TEXT_CORPUS_LIMIT) return -1;
        order[expected_count++] = (int)value; s = end;
    }
    if (mulib_bidi_resolve(u, text, n, base == 2 ? -1 : (int)base, &result)) return -1;
#ifdef MULIB_TEXT_CONFORMANCE_NATIVE
    if (compare_native(u, text, n, base == 2 ? -1 : (int)base, &result)) ++native_disagreements;
#endif
    if (result.paragraph_level != (unsigned)paragraph_level) goto fail;
    for (i = 0; i < result.count; ++i) {
        size_t first = 0, last;
        while (first < count && offsets[first] < result.runs[i].start) ++first;
        last = first;
        while (last < count && offsets[last] < result.runs[i].start+result.runs[i].length) {
            actual[last] = (int)result.runs[i].level; ++last;
        }
        for (j = first; j < last; ++j) {
            size_t index = result.runs[i].level & 1 ? last-1-(j-first) : j;
            if (expected[index] < 0) continue;
            if (k >= expected_count || order[k++] != (int)index) goto fail;
        }
    }
    for (i = 0; i < count; ++i) if (expected[i] >= 0 && expected[i] != actual[i]) goto fail;
    if (k != expected_count) goto fail;
    mulib_bidi_destroy(&result); return 0;
fail:
    mulib_bidi_destroy(&result); return -1;
}
int main(int argc, char **argv) {
    char line[65536];
    FILE *file;
    size_t number = 0, tested = 0, failed = 0;
    mulib_uniproc_t *u;
    if (argc != 3 || (strcmp(argv[1], "normalization") && strcmp(argv[1], "grapheme") && strcmp(argv[1], "bidi"))) return 2;
    u = mulib_uniproc_open(NULL, 0); if (!u) return 2;
    {
        void *address;
        void (*version)(uint8_t *);
        uint8_t data[4];
        if (sizeof version != sizeof address || mulib_uniproc_symbol(u, "u_getUnicodeVersion", &address)) { mulib_uniproc_close(u); return 2; }
        memcpy(&version, &address, sizeof version); version(data);
        printf("ICU Unicode data: %u.%u.%u\n", data[0], data[1], data[2]);
    }
    file = fopen(argv[2], "rb"); if (!file) { mulib_uniproc_close(u); return 2; }
    while (fgets(line, sizeof line, file)) {
        char *s = line, *comment = strchr(line, '#');
        int result;
        ++number;
        if (!strchr(line, '\n') && !feof(file)) { ++failed; break; }
        if (comment) *comment = 0;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
        if (!*s || *s == '@') continue;
        result = !strcmp(argv[1], "normalization") ? normalization(u, s) : !strcmp(argv[1], "grapheme") ? grapheme(u, s) : bidi(u, s);
        ++tested;
        if (result) { if (failed < 10) fprintf(stderr, "%s:%zu: mismatch\n", argv[2], number); ++failed; }
    }
    if (ferror(file)) ++failed;
    fclose(file); mulib_uniproc_close(u);
    printf("%s: %zu cases, %zu failures\n", argv[1], tested, failed);
#ifdef MULIB_TEXT_CONFORMANCE_NATIVE
    if (!strcmp(argv[1], "bidi")) printf("bidi wrapper/native disagreements: %zu\n", native_disagreements);
    if (native_disagreements) return 1;
#endif
    return failed || !tested ? 1 : 0;
}
