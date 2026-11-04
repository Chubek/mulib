#define MULIB_IMPL_XLL
#define MULIB_IMPL_LAYOUT
#define MULIB_IMPL_GLYPH
#define MULIB_IMPL_LIGATURE
#define MULIB_IMPL_PGLAYOUT
#define MULIB_IMPL_FONTDRAW
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_BIDI
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_STYLESHEET
#define MULIB_IMPL_CSS
#define MULIB_IMPL_XML
#define MULIB_IMPL_YAML
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-xll.h"
#include <assert.h>

/* With arguments this example is a translator: xll input.xll output.c prefix.
 * Parse/generate before opening the output, so invalid input does not truncate it. */
int main(int argc, char **argv) {
    mulib_xll_t *program;
    mulib_xll_error_t error;
    mulib_strbuf_t input = {0};
    char *output;
    size_t length;
    if (argc == 1) {
        const char *source = "xll 1 { layout greeting { text \"Hello سلام world\"; font \"Noto Sans\" 18;"
            " width 400; wrap word; align start; origin 20 40; } }";
        mulib_xll_block_t block = mulib_xll_block_default();
        program = mulib_xll_parse(source, strlen(source), &error); assert(program);
        block.name = "footer"; block.text = "Built through the C API"; block.length = strlen(block.text);
        block.options.width = 400; block.options.alignment = MULIB_LAYOUT_CENTER;
        assert(!mulib_xll_add(program, &block, &error));
        output = mulib_xll_emit_c(program, "example", &length, &error);
        assert(output && length && strstr(output, "mulib_layout_create") && mulib_xll_count(program) == 2);
        free(output); mulib_xll_destroy(program); return 0;
    }
    if (argc != 4) { fprintf(stderr, "usage: %s input.xll output.c prefix\n", argv[0]); return 2; }
    {
        FILE *file = fopen(argv[1], "rb");
        char buffer[4096]; size_t n; int failed = 0;
        if (!file) { perror(argv[1]); return 1; }
        while ((n = fread(buffer, 1, sizeof buffer, file)) != 0) {
            if (n > 128u*1024u*1024u-input.length || !mulib_strbuf_append_n(&input, buffer, n)) { failed = 1; break; }
        }
        if (ferror(file)) failed = 1;
        if (fclose(file)) failed = 1;
        if (failed) { fprintf(stderr, "input read, allocation or size limit failure\n"); mulib_strbuf_free(&input); return 1; }
    }
    program = mulib_xll_parse(input.data, input.length, &error); mulib_strbuf_free(&input);
    if (!program) { fprintf(stderr, "%s:%zu:%zu: %s\n", argv[1], error.line, error.column, error.message); return 1; }
    output = mulib_xll_emit_c(program, argv[3], &length, &error); mulib_xll_destroy(program);
    if (!output) { fprintf(stderr, "%s\n", error.message); return 1; }
    {
        FILE *file = fopen(argv[2], "wb"); int failed;
        if (!file) { perror(argv[2]); free(output); return 1; }
        failed = fwrite(output, 1, length, file) != length;
        if (fclose(file)) failed = 1;
        free(output);
        if (failed) { fprintf(stderr, "output write failure\n"); return 1; }
    }
    return 0;
}
