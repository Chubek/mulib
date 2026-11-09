#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_LINEBREAK
#include "mulib-linebreak.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const char text[] = "aaa bb cc ddddd";
    const char *names[] = {"greedy", "character", "minimum raggedness", "balanced",
                           "lookahead", "Knuth-Plass", "Unicode"};
    mulib_linebreak_options_t options;
    mulib_linebreak_result_t result = {NULL, 0, 0};
    mulib_uniproc_t *unicode = mulib_uniproc_open(NULL, 0);
    int algorithm;
    mulib_linebreak_options_init(&options, 6);
    options.unicode = unicode;
    for (algorithm = MULIB_LINEBREAK_GREEDY; algorithm <= MULIB_LINEBREAK_UNICODE; ++algorithm) {
        size_t i;
        int rc;
        if (algorithm == MULIB_LINEBREAK_UNICODE && !unicode) {
            puts("Unicode: ICU unavailable"); continue;
        }
        rc = mulib_linebreak(text, sizeof text - 1, (mulib_linebreak_algorithm_t)algorithm, &options, &result);
        if (rc == MULIB_LINEBREAK_INFEASIBLE) {
            printf("%s: no feasible justified layout\n", names[algorithm]); continue;
        }
        assert(rc == MULIB_LINEBREAK_OK);
        printf("%s:\n", names[algorithm]);
        for (i = 0; i < result.count; ++i) {
            const mulib_linebreak_line_t *line = &result.lines[i];
            printf("  %.*s%s\n", (int)(line->end - line->start), text + line->start,
                   (line->flags & MULIB_LINEBREAK_HYPHEN) ? "-" : "");
        }
    }
    assert(!mulib_linebreak("aa bb cc", 8, MULIB_LINEBREAK_KNUTH_PLASS, &options, &result));
    assert(result.count == 2 && result.lines[0].adjustment == 2);
    mulib_linebreak_result_free(&result);
    mulib_uniproc_close(unicode);
    return 0;
}
