#define MULIB_IMPL_XMACIR
#define MULIB_IMPL_CAPASM
#include "mulib-capasm.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char scalar_ir[] =
    "XM_U32(calc)\n"
    "XM_PUSH(4294967295)\nXM_PUSH(2)\nXM_ADD()\n"
    "XM_PUSH(3)\nXM_SWAP()\nXM_SUB()\n"
    "XM_PUSH(4294967295)\nXM_MUL()\n"
    "XM_PUSH(255)\nXM_AND()\nXM_PUSH(256)\nXM_OR()\nXM_PUSH(1)\nXM_XOR()\n"
    "XM_DUP()\nXM_DROP()\nXM_RET()\n";

static const char blob_ir[] =
    "XM_BLOB(load,xm_bytes,ctx,err)\n"
    "XM_BYTES(h00017f80ffaabbccddee112233)\nXM_END()\n";

static const char empty_ir[] = "XM_BLOB(empty,xm_bytes,ctx,err)\nXM_BYTES(h)\nXM_END()\n";
static const char rewrite_ir[] =
    "XM_U32(calc)\nXM_PUSH(510)\nXM_PUSH(1)\nXM_ADD()\nXM_RET()\n";
static const char folded_ir[] = "XM_U32(calc)\nXM_PUSH(511)\nXM_RET()\n";

static void parse(const char *text, mulib_xmacir_program_t *program)
{
    mulib_xmacir_error_t error = {0, NULL};
    if (!mulib_xmacir_parse(text, strlen(text), program, &error)) {
        fprintf(stderr, "XMacIR at %zu: %s\n", error.offset,
                error.message ? error.message : "(no diagnostic)");
        abort();
    }
}

static char *emit(const mulib_xmacir_program_t *program,
                  const mulib_capasm_options_t *options, size_t *length)
{
    char *out = NULL;
    mulib_xmacir_error_t error = {55, "old error"};
    assert(mulib_capasm_emit(program, options, &out, length, &error));
    assert(!error.message && !error.offset && out && out[*length] == '\0');
    return out;
}

static void reject(const mulib_xmacir_program_t *program,
                   const mulib_capasm_options_t *options, const char *diagnostic)
{
    mulib_xmacir_error_t error = {0, NULL};
    char sentinel;
    char *out = &sentinel;
    size_t length = 47;
    assert(!mulib_capasm_emit(program, options, &out, &length, &error));
    assert(out == &sentinel && length == 47 && error.message);
    if (diagnostic) assert(strstr(error.message, diagnostic));
}

static void test_pure(void)
{
    mulib_xmacir_program_t p = {0};
    const mulib_capasm_options_t no_tools = {0};
    char *c, *same;
    size_t length, n;
    parse(scalar_ir, &p);
    c = emit(&p, NULL, &length);
    assert(strlen(c) == length && strstr(c, "CAPASM") && strstr(c, "calc(void)"));
    assert(strstr(c, "0xffffffffUL") && strstr(c, "unsigned long xm_temp;"));
    same = emit(&p, &no_tools, &n);
    assert(n == length && !memcmp(c, same, length));
    free(same);
    free(c);
    assert(!mulib_capasm_emit(&p, NULL, NULL, &length, NULL));
    assert(!mulib_capasm_emit(&p, NULL, &c, NULL, NULL));
    mulib_xmacir_free(&p);
    reject(NULL, NULL, "null");
    reject(&p, NULL, NULL);

    parse(blob_ir, &p);
    c = emit(&p, NULL, &length);
    assert(strlen(c) == length && strstr(c, "#include <stddef.h>"));
    assert(strstr(c, "extern int xm_bytes(struct ctx *,\n    struct err *,"));
    assert(strstr(c, "int load(struct ctx *xm_context,"));
    assert(strstr(c, "static const unsigned char xm_data[]"));
    assert(strstr(c, "0x11, 0x22,\n        0x33,\n"));
    assert(strstr(c, "xm_bytes(xm_context, xm_error, xm_data, sizeof xm_data)"));
    free(c);
#if UCHAR_MAX > 255
    p.bytes[0] = 256;
    reject(&p, NULL, "octet");
    p.bytes[0] = 0;
#endif
    strcpy(p.entry, "xm_BYTe_but_not_identical");
    reject(&p, NULL, "collision");
    strcpy(p.entry, "load");
    strcpy(p.error_tag, "NULL");
    reject(&p, NULL, NULL);
    strcpy(p.error_tag, "err");
    strcpy(p.bridge, "size_t");
    reject(&p, NULL, NULL);
    mulib_xmacir_free(&p);

    parse(empty_ir, &p);
    c = emit(&p, NULL, &length);
    assert(strstr(c, "xm_data[] = {\n        0\n    };"));
    assert(strstr(c, ", xm_data, (size_t)0);"));
    free(c);
    mulib_xmacir_free(&p);
}

typedef struct {
    mulib_capasm_stage_t stage;
    const mulib_capasm_tool_t *tool;
    const char *input;
    size_t input_length;
    const char *result;
    size_t result_length;
} expected_call;

typedef struct {
    const expected_call *calls;
    size_t total;
    size_t pos;
} mock_host;

static int mock_run(void *user, mulib_capasm_stage_t stage,
                     const mulib_capasm_tool_t *tool, const char *input,
                     size_t input_length, char **output, size_t *output_length,
                     mulib_xmacir_error_t *error)
{
    mock_host *host = (mock_host *)user;
    const expected_call *call;
    assert(host->pos < host->total);
    call = host->calls + host->pos++;
    assert(stage == call->stage && tool == call->tool);
    assert(*output == NULL && *output_length == 0 && !error->message);
    assert(input_length == call->input_length && !memcmp(input, call->input, input_length));
    if (!strcmp(tool->name, "simplify")) {
        assert(!strcmp(tool->executable, "/host/path with space/m4"));
        assert(tool->argc == 3 && !strcmp(tool->argv[0], "-Dthing=with spaces"));
        assert(!strcmp(tool->argv[1], "; nope") && !strcmp(tool->argv[2], ""));
    }
    if (!call->result) {
        error->offset = 29;
        error->message = "tool deliberately failed";
        return 0;
    }
    *output = (char *)malloc(call->result_length ? call->result_length : 1);
    assert(*output);
    memcpy(*output, call->result, call->result_length);
    *output_length = call->result_length;
    return 1;
}

static char *canonical(const char *text, size_t *length)
{
    mulib_xmacir_program_t p = {0};
    char *out = NULL;
    parse(text, &p);
    assert(mulib_xmacir_format(&p, &out, length, NULL));
    mulib_xmacir_free(&p);
    return out;
}

static void test_pipeline(void)
{
    static const char *const literal_args[] = {"-Dthing=with spaces", "; nope", ""};
    static const mulib_capasm_tool_t passes[] = {
        {"simplify", "/host/path with space/m4", literal_args, 3},
        {"fold", "./literal-tool", NULL, 0}
    };
    static const mulib_capasm_tool_t pp = {"cpp", "selected-cpp", NULL, 0};
    static const mulib_capasm_tool_t cc = {"cc", "selected-compiler", NULL, 0};
    static const mulib_capasm_tool_t as = {"as", "selected-as", NULL, 0};
    static const mulib_capasm_tool_t ld = {"ld", "selected-ld", NULL, 0};
    static const char object[] = {'o', '\0', 'b', 'j'};
    static const char artifact[] = {'\0', 'E', 'L', 'F', '\0'};
    mock_host host = {NULL, 6, 0};
    static const char *const kernels[] = {"money"};
    const mulib_capasm_options_t options = {
        .passes = passes, .pass_count = 2, .preprocessor = &pp,
        .compiler = &cc, .assembler = &as, .linker = &ld,
        .run = mock_run, .user = &host, .kernels = kernels, .kernel_count = 1
    };
    const mulib_capasm_options_t headers_only = {.kernels = kernels, .kernel_count = 1};
    mulib_xmacir_program_t program = {0}, folded = {0};
    char *c, *in1, *in2, *output;
    size_t n, n1, n2, length;
    parse(scalar_ir, &program);
    parse(folded_ir, &folded);
    c = emit(&folded, &headers_only, &n);
    in1 = canonical(scalar_ir, &n1);
    in2 = canonical(rewrite_ir, &n2);
    {
        const expected_call calls[] = {
            {MULIB_CAPASM_REWRITE, passes, in1, n1, rewrite_ir, sizeof(rewrite_ir) - 1},
            {MULIB_CAPASM_REWRITE, passes + 1, in2, n2, folded_ir, sizeof(folded_ir) - 1},
            {MULIB_CAPASM_PREPROCESS, &pp, c, n, "C", 1},
            {MULIB_CAPASM_COMPILE, &cc, "C", 1, "ASM", 3},
            {MULIB_CAPASM_ASSEMBLE, &as, "ASM", 3, object, sizeof(object)},
            {MULIB_CAPASM_LINK, &ld, object, sizeof(object), artifact, sizeof(artifact)}
        };
        host.calls = calls;
        output = emit(&program, &options, &length);
        assert(host.pos == host.total && length == sizeof(artifact));
        assert(!memcmp(output, artifact, length) && output[length] == '\0');
        assert(program.count > folded.count && !strcmp(program.entry, "calc"));
        free(output);
    }
    free(c);
    free(in1);
    free(in2);
    mulib_xmacir_free(&program);
    mulib_xmacir_free(&folded);
}

static void test_rewrite_failure(const char *input, const char *result, const char *message)
{
    const mulib_capasm_tool_t tool = {"named", "tool", NULL, 0};
    mulib_xmacir_program_t p = {0};
    size_t length;
    char *text = canonical(input, &length);
    const expected_call call = {
        MULIB_CAPASM_REWRITE, &tool, text, length, result, result ? strlen(result) : 0
    };
    mock_host host = {&call, 1, 0};
    const mulib_capasm_options_t opts = {
        .passes = &tool, .pass_count = 1, .run = mock_run, .user = &host
    };
    parse(input, &p);
    reject(&p, &opts, message);
    assert(host.pos == 1);
    mulib_xmacir_free(&p);
    free(text);
}

static void test_blob_rewrite_and_build_failure(void)
{
    const mulib_capasm_tool_t pass = {"keep bytes", "tool", NULL, 0};
    const mulib_capasm_tool_t cpp = {"cpp", "preprocessor", NULL, 0};
    mulib_xmacir_program_t p = {0};
    mulib_xmacir_error_t error = {0, NULL};
    size_t n, c_len, length;
    char *text = canonical(blob_ir, &n);
    char *c, *output, sentinel;
    parse(blob_ir, &p);
    c = emit(&p, NULL, &c_len);
    {
        const expected_call calls[] = {
            {MULIB_CAPASM_REWRITE, &pass, text, n, text, n},
            {MULIB_CAPASM_PREPROCESS, &cpp, c, c_len, NULL, 0}
        };
        mock_host host = {calls, 1, 0};
        mulib_capasm_options_t opts = {
            .passes = &pass, .pass_count = 1, .run = mock_run, .user = &host
        };
        output = emit(&p, &opts, &length);
        assert(host.pos == 1 && length == c_len && !memcmp(c, output, length));
        free(output);

        host.pos = 0;
        host.total = 2;
        opts.preprocessor = &cpp;
        output = &sentinel;
        length = 37;
        assert(!mulib_capasm_emit(&p, &opts, &output, &length, &error));
        assert(host.pos == 2 && output == &sentinel && length == 37);
        assert(error.offset == 29 && !strcmp(error.message, "tool deliberately failed"));
    }
    free(c);
    free(text);
    mulib_xmacir_free(&p);
}

static void test_kernels(void)
{
    static const char *const kernels[] = {"money", "b+tree", "money", "2dgeom"};
    static const char *const invalid[] = {
        "capasm", "xmacir", "Capasm", "mulib-capasm.h", "../money",
        "foreign/money", "money\"\n#include \"bad", "money\\", "", NULL
    };
    mulib_xmacir_program_t p = {0};
    mulib_capasm_options_t opts = {.kernels = kernels, .kernel_count = 4};
    size_t len;
    parse(scalar_ir, &p);
    char *c = emit(&p, &opts, &len);
    const char *header = "#include \"mulib-money.h\"\n";
    const char expected[] = "#include \"mulib-money.h\"\n"
        "#include \"mulib-b+tree.h\"\n#include \"mulib-2dgeom.h\"\n\n";
    char *first = strstr(c, header);
    assert(first && !strncmp(first, expected, sizeof expected - 1));
    assert(!strstr(first + strlen(header), header) && !strstr(c, "MULIB_IMPL_"));
    free(c);
    opts.kernel_count = 1;
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        opts.kernels = invalid + i;
        reject(&p, &opts, "kernel name");
    }
    opts.kernels = NULL;
    reject(&p, &opts, "kernel list");
    opts.kernels = kernels;
    opts.kernel_count = MULIB_CAPASM_MAX_KERNELS + 1;
    reject(&p, &opts, "kernel list");
    mulib_xmacir_free(&p);
}

static void test_options_failure(void)
{
    const char *const bad_argv[] = {NULL};
    mulib_capasm_tool_t tool = {"named", "tool", NULL, 0};
    mock_host host = {NULL, 0, 0};
    mulib_capasm_options_t opts = {0};
    mulib_xmacir_program_t p = {0};
    parse(scalar_ir, &p);
    opts.pass_count = 1;
    reject(&p, &opts, "pass list");
    opts.passes = &tool;
    reject(&p, &opts, "callback");
    opts.run = mock_run;
    opts.user = &host;
    opts.assembler = &tool;
    reject(&p, &opts, "assembler needs compiler");
    opts.assembler = NULL;
    opts.linker = &tool;
    reject(&p, &opts, "linker needs assembler");
    opts.linker = NULL;
    opts.preprocessor = &tool;
    tool.executable = "";
    reject(&p, &opts, "build stage");
    tool.executable = "tool";
    opts.preprocessor = NULL;
    tool.argc = 1;
    reject(&p, &opts, "rewrite tool");
    tool.argv = bad_argv;
    reject(&p, &opts, "rewrite tool");
    assert(host.pos == 0);
    mulib_xmacir_free(&p);

    test_rewrite_failure(scalar_ir, NULL, "deliberately failed");
    test_rewrite_failure(scalar_ir, "not XMacIR", NULL);
    test_rewrite_failure(scalar_ir, "XM_U32(other)\nXM_PUSH(511)\nXM_RET()\n", "changed the XMacIR ABI");
    test_rewrite_failure(scalar_ir, "XM_U32(calc)\nXM_PUSH(512)\nXM_RET()\n", "changed the U32 value");
    test_rewrite_failure(scalar_ir, blob_ir, "changed the XMacIR ABI");
    test_rewrite_failure(blob_ir, "XM_BLOB(load,xm_bytes,ctx,other)\nXM_BYTES(h00)\nXM_END()\n",
                         "changed the XMacIR ABI");
    test_rewrite_failure(blob_ir, "XM_BLOB(load,xm_bytes,ctx,err)\nXM_BYTES(h00)\nXM_END()\n",
                         "changed the BLOB payload");
    test_rewrite_failure(blob_ir, "XM_BLOB(load,xm_bytes,ctx,err)\n"
                         "XM_BYTES(hff017f80ffaabbccddee112233)\nXM_END()\n", "changed the BLOB payload");
}

/* Optional fixture modes write generated sources. Compile each as an
 * independent strict C90 translation unit, then link with a C90 client. */
int main(int argc, char **argv)
{
    if (argc == 2) {
        const char *text;
        mulib_xmacir_program_t p = {0};
        char *source;
        size_t length;
        if (!strcmp(argv[1], "u32")) text = scalar_ir;
        else if (!strcmp(argv[1], "blob")) text = blob_ir;
        else if (!strcmp(argv[1], "empty")) text = empty_ir;
        else return 1;
        parse(text, &p);
        source = emit(&p, NULL, &length);
        if (fwrite(source, 1, length, stdout) != length) {
            free(source);
            mulib_xmacir_free(&p);
            return 1;
        }
        free(source);
        mulib_xmacir_free(&p);
    } else {
        test_pure();
        test_kernels();
        test_pipeline();
        test_blob_rewrite_and_build_failure();
        test_options_failure();
    }
    return 0;
}
