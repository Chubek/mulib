/* MuLIB CAPASM: validated XMacIR to ISO C90, with optional host tool stages.
 * Define MULIB_IMPL_CAPASM in one translation unit. Link XMacIR once.
 */
#ifndef MULIB_CAPASM_H
#define MULIB_CAPASM_H
#include "mulib-xmacir.h"

#define MULIB_CAPASM_MAX_KERNELS 64

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MULIB_CAPASM_REWRITE,    /* canonical XMacIR -> XMacIR */
    MULIB_CAPASM_PREPROCESS, /* C source -> preprocessed C source */
    MULIB_CAPASM_COMPILE,    /* C (possibly preprocessed) -> assembly */
    MULIB_CAPASM_ASSEMBLE,   /* assembly -> object */
    MULIB_CAPASM_LINK        /* object -> linked artifact */
} mulib_capasm_stage_t;

typedef struct {
    const char *name;        /* nonempty diagnostic/pass name */
    const char *executable;  /* nonempty, selected by the trusted host */
    const char *const *argv; /* argc literal arguments, excluding argv[0] */
    size_t argc;
} mulib_capasm_tool_t;

/* Input is borrowed only for the call. On nonzero return, transfer a fresh
 * malloc-compatible *output and its byte length; no NUL is required. Output
 * is initially NULL; leave it NULL on failure (zero return). An optional
 * failure diagnostic must point to a static message. The callback is trusted
 * to perform the selected operation, including any file handling. It must
 * treat argument and input data as data, not shell commands, and must not
 * modify the supplied program, options, or input. */
typedef int (*mulib_capasm_run_fn)(
    void *user, mulib_capasm_stage_t stage, const mulib_capasm_tool_t *tool,
    const char *input, size_t input_length, char **output,
    size_t *output_length, mulib_xmacir_error_t *error);

typedef struct {
    const mulib_capasm_tool_t *passes; /* pass_count ordered rewrites (e.g. m4) */
    size_t pass_count;
    const mulib_capasm_tool_t *preprocessor;
    const mulib_capasm_tool_t *compiler;
    const mulib_capasm_tool_t *assembler;
    const mulib_capasm_tool_t *linker;
    mulib_capasm_run_fn run;
    void *user;
    /* Optional MuLIB header basenames: e.g. "money" for "mulib-money.h".
     * At most MULIB_CAPASM_MAX_KERNELS names; duplicate names are emitted
     * once in first-occurrence order. No implementation switches are set. */
    const char *const *kernels;
    size_t kernel_count;
} mulib_capasm_options_t;

/* Return nonzero on success. NULL options selects pure C90 emission. Non-NULL
 * options select rewrites then explicitly configured build stages in enum
 * order. An assembler requires a compiler; a linker requires an assembler.
 * Each rewrite is parsed and checked to retain ABI names, exact BLOB bytes
 * or the same U32 value. Other stages' output is trusted, not validated.
 *
 * On success, caller owns *output (free); *output_length excludes its trailing
 * NUL. This is C source if no build stages run, otherwise the last stage's
 * bytes (which may contain NULs). On failure neither result slot is changed.
 * error is optional and is cleared on success. Failure does not undo prior
 * callback side effects. No tool or process is selected or run by default.
 * Generated source is one translation unit per program; the combined link
 * must supply a matching external BLOB bridge. The host must choose external
 * names that neither the C library reserves nor the remaining linked
 * objects use, taking the target's identifier significance into account.
 * Selected MuLIB headers are emitted before the generated definition and
 * before preprocessing. They can require C11 or other kernel-specific
 * settings; only output without extra includes is promised to be C90.
 * Names are literal canonical basenames, not paths; the caller supplies
 * the include path, implementation TUs and any kernel dependencies.
 * The CAPASM and XMacIR headers cannot be selected directly.
 * Object sizes remain subject to target translation and storage limits.
 */
int mulib_capasm_emit(const mulib_xmacir_program_t *program,
                      const mulib_capasm_options_t *options,
                      char **output, size_t *output_length,
                      mulib_xmacir_error_t *error);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_CAPASM_H */

#if defined(MULIB_IMPL_CAPASM) && !defined(MULIB_CAPASM_IMPLEMENTED)
#define MULIB_CAPASM_IMPLEMENTED
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} mulib_capasm_buffer_;

static int mulib_capasm_fail_(mulib_xmacir_error_t *error, size_t offset,
                             const char *message)
{
    if (error) {
        error->offset = offset;
        error->message = message;
    }
    return 0;
}

static int mulib_capasm_append_(mulib_capasm_buffer_ *b, const char *text,
                               size_t length)
{
    size_t needed, capacity;
    char *data;
    if (length >= (size_t)-1 - b->length) return 0;
    needed = b->length + length + 1;
    if (needed > b->capacity) {
        capacity = b->capacity ? b->capacity : 128;
        while (capacity < needed) {
            if (capacity > (size_t)-1 / 2) {
                capacity = needed;
                break;
            }
            capacity *= 2;
        }
        data = (char *)realloc(b->data, capacity);
        if (!data) return 0;
        b->data = data;
        b->capacity = capacity;
    }
    memcpy(b->data + b->length, text, length);
    b->length += length;
    b->data[b->length] = '\0';
    return 1;
}

static int mulib_capasm_put_(mulib_capasm_buffer_ *b, const char *text)
{
    return mulib_capasm_append_(b, text, strlen(text));
}

static int mulib_capasm_size_(mulib_capasm_buffer_ *b, size_t value)
{
    static const char digits[] = "0123456789";
    char text[sizeof(size_t) * CHAR_BIT];
    size_t pos = sizeof(text);
    do {
        text[--pos] = digits[value % 10];
        value /= 10;
    } while (value);
    return mulib_capasm_append_(b, text + pos, sizeof(text) - pos);
}

static const char mulib_capasm_hex_[] = "0123456789abcdef";

static int mulib_capasm_hexint_(mulib_capasm_buffer_ *b, unsigned long value)
{
    char text[] = "0x00000000UL";
    size_t pos = 10;
    while (pos > 2) {
        text[--pos] = mulib_capasm_hex_[value & 15UL];
        value >>= 4;
    }
    return mulib_capasm_put_(b, text);
}

/* ISO C90 guarantees only six case-insensitive significant characters for
 * external identifiers. Use an explicit alphabet, independent of locale. */
static unsigned char mulib_capasm_lower_(unsigned char c)
{
    static const char upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char lower[] = "abcdefghijklmnopqrstuvwxyz";
    const char *pos = c ? strchr(upper, c) : NULL;
    return pos ? (unsigned char)lower[pos - upper] : c;
}

static int mulib_capasm_external_match_(const char *left, const char *right)
{
    size_t i;
    for (i = 0; i < 6; ++i) {
        if (mulib_capasm_lower_((unsigned char)left[i]) !=
            mulib_capasm_lower_((unsigned char)right[i])) return 0;
        if (!left[i]) return 1;
    }
    return 1;
}

static int mulib_capasm_stddef_macro_(const char *name)
{
    return !strcmp(name, "NULL") || !strcmp(name, "offsetof");
}

static int mulib_capasm_stddef_ordinary_(const char *name)
{
    return mulib_capasm_stddef_macro_(name) || !strcmp(name, "ptrdiff_t") ||
           !strcmp(name, "size_t") || !strcmp(name, "wchar_t");
}

static int mulib_capasm_c90_names_(const mulib_xmacir_program_t *p,
                                  mulib_xmacir_error_t *error)
{
    if (mulib_capasm_external_match_(p->entry, "main"))
        return mulib_capasm_fail_(error, 0, "entry conflicts with C main");
    if (p->mode == MULIB_XMACIR_BLOB) {
        if (mulib_capasm_external_match_(p->bridge, "main") ||
            mulib_capasm_external_match_(p->entry, p->bridge))
            return mulib_capasm_fail_(error, 0, "C90 external name collision");
        if (mulib_capasm_stddef_ordinary_(p->entry) ||
            mulib_capasm_stddef_ordinary_(p->bridge) ||
            mulib_capasm_stddef_macro_(p->context_tag) ||
            mulib_capasm_stddef_macro_(p->error_tag))
            return mulib_capasm_fail_(error, 0, "name conflicts with <stddef.h>");
        if (!strncmp(p->context_tag, p->error_tag, 31) &&
            strcmp(p->context_tag, p->error_tag))
            return mulib_capasm_fail_(error, 0, "C90 structure tag collision");
    }
    return 1;
}

/* Validation guarantees every instruction has its operands, the stack ends
 * with one value, and the last instruction is RET. */
static size_t mulib_capasm_peak_(const mulib_xmacir_program_t *p, int *has_swap)
{
    size_t i, depth = 0, peak = 0;
    *has_swap = 0;
    for (i = 0; i < p->count; ++i) {
        switch (p->instructions[i].opcode) {
        case MULIB_XMACIR_PUSH:
        case MULIB_XMACIR_DUP:
            if (++depth > peak) peak = depth;
            break;
        case MULIB_XMACIR_SWAP:
            *has_swap = 1;
            break;
        case MULIB_XMACIR_RET:
            break;
        default:
            --depth;
            break;
        }
    }
    return peak;
}

static int mulib_capasm_eval_(const mulib_xmacir_program_t *p,
                             unsigned long *value, mulib_xmacir_error_t *error)
{
    size_t i, depth = 0;
    unsigned long stack[MULIB_XMACIR_MAX_STACK], left, right, result;
    for (i = 0; i < p->count; ++i) {
        switch (p->instructions[i].opcode) {
        case MULIB_XMACIR_PUSH:
            stack[depth++] = p->instructions[i].value;
            continue;
        case MULIB_XMACIR_DUP:
            stack[depth] = stack[depth - 1];
            ++depth;
            continue;
        case MULIB_XMACIR_DROP:
            --depth;
            continue;
        case MULIB_XMACIR_SWAP:
            left = stack[depth - 2];
            stack[depth - 2] = stack[depth - 1];
            stack[depth - 1] = left;
            continue;
        case MULIB_XMACIR_RET:
            *value = stack[0];
            return 1;
        default:
            break;
        }
        left = stack[depth - 2];
        right = stack[--depth];
        switch (p->instructions[i].opcode) {
        case MULIB_XMACIR_ADD: result = left + right; break;
        case MULIB_XMACIR_SUB: result = left - right; break;
        case MULIB_XMACIR_MUL: result = left * right; break;
        case MULIB_XMACIR_AND: result = left & right; break;
        case MULIB_XMACIR_OR:  result = left | right; break;
        case MULIB_XMACIR_XOR: result = left ^ right; break;
        default:
            return mulib_capasm_fail_(error, i, "unsupported U32 opcode");
        }
        stack[depth - 1] = result & 0xffffffffUL;
    }
    return mulib_capasm_fail_(error, 0, "missing U32 return");
}

static int mulib_capasm_stack_(mulib_capasm_buffer_ *b, size_t index)
{
    return mulib_capasm_put_(b, "xm_stack[") && mulib_capasm_size_(b, index) &&
           mulib_capasm_put_(b, "]");
}

static int mulib_capasm_u32_(const mulib_xmacir_program_t *p,
                            mulib_capasm_buffer_ *b)
{
    size_t i, depth = 0, peak;
    int has_swap;
    const char *op;
    peak = mulib_capasm_peak_(p, &has_swap);
    if (!mulib_capasm_put_(b, "unsigned long ") ||
        !mulib_capasm_put_(b, p->entry) ||
        !mulib_capasm_put_(b, "(void)\n{\n    unsigned long xm_stack[") ||
        !mulib_capasm_size_(b, peak) ||
        !mulib_capasm_put_(b, "];\n") ||
        (has_swap && !mulib_capasm_put_(b, "    unsigned long xm_temp;\n")))
        return 0;
    for (i = 0; i < p->count; ++i) {
        switch (p->instructions[i].opcode) {
        case MULIB_XMACIR_PUSH:
            if (!mulib_capasm_put_(b, "    ") || !mulib_capasm_stack_(b, depth) ||
                !mulib_capasm_put_(b, " = ") ||
                !mulib_capasm_hexint_(b, p->instructions[i].value) ||
                !mulib_capasm_put_(b, ";\n")) return 0;
            ++depth;
            continue;
        case MULIB_XMACIR_DUP:
            if (!mulib_capasm_put_(b, "    ") || !mulib_capasm_stack_(b, depth) ||
                !mulib_capasm_put_(b, " = ") || !mulib_capasm_stack_(b, depth - 1) ||
                !mulib_capasm_put_(b, ";\n")) return 0;
            ++depth;
            continue;
        case MULIB_XMACIR_DROP:
            --depth;
            continue;
        case MULIB_XMACIR_SWAP:
            if (!mulib_capasm_put_(b, "    xm_temp = ") ||
                !mulib_capasm_stack_(b, depth - 2) ||
                !mulib_capasm_put_(b, ";\n    ") ||
                !mulib_capasm_stack_(b, depth - 2) ||
                !mulib_capasm_put_(b, " = ") || !mulib_capasm_stack_(b, depth - 1) ||
                !mulib_capasm_put_(b, ";\n    ") ||
                !mulib_capasm_stack_(b, depth - 1) ||
                !mulib_capasm_put_(b, " = xm_temp;\n")) return 0;
            continue;
        case MULIB_XMACIR_RET:
            return mulib_capasm_put_(b, "    return xm_stack[0];\n}\n");
        case MULIB_XMACIR_ADD: op = " + "; break;
        case MULIB_XMACIR_SUB: op = " - "; break;
        case MULIB_XMACIR_MUL: op = " * "; break;
        case MULIB_XMACIR_AND: op = " & "; break;
        case MULIB_XMACIR_OR:  op = " | "; break;
        case MULIB_XMACIR_XOR: op = " ^ "; break;
        default: return 0;
        }
        --depth;
        if (!mulib_capasm_put_(b, "    ") || !mulib_capasm_stack_(b, depth - 1) ||
            !mulib_capasm_put_(b, " = (") || !mulib_capasm_stack_(b, depth - 1) ||
            !mulib_capasm_put_(b, op) || !mulib_capasm_stack_(b, depth) ||
            !mulib_capasm_put_(b, ") & 0xffffffffUL;\n")) return 0;
    }
    return 0;
}

static int mulib_capasm_blob_(const mulib_xmacir_program_t *p,
                             mulib_capasm_buffer_ *b)
{
    size_t i;
    unsigned char value;
    char literal[] = "0x00,";
    /* Block-scope names may coincide with tags or the entry, but must not
     * shadow the bridge. The other short name in each pair is a fallback. */
    const char *ctx = strcmp(p->bridge, "xm_context") ? "xm_context" : "xm_ctx";
    const char *err = strcmp(p->bridge, "xm_error") ? "xm_error" : "xm_err";
    const char *data = strcmp(p->bridge, "xm_bytes") ? "xm_bytes" : "xm_data";
    if (!mulib_capasm_put_(b, "#include <stddef.h>\n\nstruct ") ||
        !mulib_capasm_put_(b, p->context_tag) || !mulib_capasm_put_(b, ";\nstruct ") ||
        !mulib_capasm_put_(b, p->error_tag) || !mulib_capasm_put_(b, ";\nextern int ") ||
        !mulib_capasm_put_(b, p->bridge) || !mulib_capasm_put_(b, "(struct ") ||
        !mulib_capasm_put_(b, p->context_tag) || !mulib_capasm_put_(b, " *,\n    struct ") ||
        !mulib_capasm_put_(b, p->error_tag) ||
        !mulib_capasm_put_(b, " *, const unsigned char *, size_t);\n\nint ") ||
        !mulib_capasm_put_(b, p->entry) || !mulib_capasm_put_(b, "(struct ") ||
        !mulib_capasm_put_(b, p->context_tag) || !mulib_capasm_put_(b, " *") ||
        !mulib_capasm_put_(b, ctx) || !mulib_capasm_put_(b, ",\n    struct ") ||
        !mulib_capasm_put_(b, p->error_tag) || !mulib_capasm_put_(b, " *") ||
        !mulib_capasm_put_(b, err) ||
        !mulib_capasm_put_(b, ")\n{\n    static const unsigned char ") ||
        !mulib_capasm_put_(b, data) || !mulib_capasm_put_(b, "[] = {\n"))
        return 0;
    for (i = 0; i < p->length; ++i) {
        /* XMacIR validation requires octets, even when CHAR_BIT > 8. */
        value = p->bytes[i];
        literal[2] = mulib_capasm_hex_[(value >> 4) & 15];
        literal[3] = mulib_capasm_hex_[value & 15];
        if (!mulib_capasm_put_(b, i % 12 ? " " : "        ") ||
            !mulib_capasm_put_(b, literal) ||
            (i % 12 == 11 && !mulib_capasm_put_(b, "\n"))) return 0;
    }
    if (!p->length && !mulib_capasm_put_(b, "        0\n")) return 0;
    if (p->length % 12 && !mulib_capasm_put_(b, "\n")) return 0;
    return mulib_capasm_put_(b, "    };\n    return ") &&
           mulib_capasm_put_(b, p->bridge) && mulib_capasm_put_(b, "(") &&
           mulib_capasm_put_(b, ctx) && mulib_capasm_put_(b, ", ") &&
           mulib_capasm_put_(b, err) && mulib_capasm_put_(b, ", ") &&
           mulib_capasm_put_(b, data) && mulib_capasm_put_(b, ", ") &&
           (p->length ? (mulib_capasm_put_(b, "sizeof ") &&
                          mulib_capasm_put_(b, data))
                      : mulib_capasm_put_(b, "(size_t)0")) &&
           mulib_capasm_put_(b, ");\n}\n");
}

static int mulib_capasm_tool_valid_(const mulib_capasm_tool_t *tool)
{
    size_t i;
    if (!tool->name || !*tool->name || !tool->executable || !*tool->executable ||
        (tool->argc && !tool->argv) ||
        tool->argc > (size_t)-1 / sizeof(*tool->argv)) return 0;
    for (i = 0; i < tool->argc; ++i)
        if (!tool->argv[i]) return 0;
    return 1;
}

static int mulib_capasm_kernel_name_(const char *name)
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t i;
    if (!name || !*name || !strchr(letters, *name)) return 0;
    for (i = 1; name[i]; ++i) {
        char c = name[i];
        if (i == 95 || (!strchr(letters, c) && c != '-' && c != '_' && c != '+'))
            return 0;
    }
    return strcmp(name, "capasm") && strcmp(name, "xmacir");
}

static int mulib_capasm_options_valid_(const mulib_capasm_options_t *opts,
                                      mulib_xmacir_error_t *error)
{
    size_t i;
    if (!opts) return 1;
    if (opts->kernel_count > MULIB_CAPASM_MAX_KERNELS ||
        (opts->kernel_count && !opts->kernels))
        return mulib_capasm_fail_(error, 0, "invalid MuLIB kernel list");
    for (i = 0; i < opts->kernel_count; ++i)
        if (!mulib_capasm_kernel_name_(opts->kernels[i]))
            return mulib_capasm_fail_(error, i, "invalid or excluded MuLIB kernel name");
    if ((opts->pass_count && !opts->passes) ||
        opts->pass_count > (size_t)-1 / sizeof(*opts->passes))
        return mulib_capasm_fail_(error, 0, "invalid rewrite pass list");
    if ((opts->pass_count || opts->preprocessor || opts->compiler ||
         opts->assembler || opts->linker) && !opts->run)
        return mulib_capasm_fail_(error, 0, "selected tools require a host callback");
    if ((opts->assembler && !opts->compiler) || (opts->linker && !opts->assembler))
        return mulib_capasm_fail_(error, 0, "assembler needs compiler and linker needs assembler");
    if ((opts->preprocessor && !mulib_capasm_tool_valid_(opts->preprocessor)) ||
        (opts->compiler && !mulib_capasm_tool_valid_(opts->compiler)) ||
        (opts->assembler && !mulib_capasm_tool_valid_(opts->assembler)) ||
        (opts->linker && !mulib_capasm_tool_valid_(opts->linker)))
        return mulib_capasm_fail_(error, 0, "invalid build stage tool configuration");
    for (i = 0; i < opts->pass_count; ++i)
        if (!mulib_capasm_tool_valid_(opts->passes + i))
            return mulib_capasm_fail_(error, i, "invalid rewrite tool configuration");
    return 1;
}

static int mulib_capasm_includes_(mulib_capasm_buffer_ *source,
                                 const mulib_capasm_options_t *options)
{
    size_t i, earlier;
    if (!options || !options->kernel_count)
        return mulib_capasm_put_(source, "/* CAPASM: generated ISO C90. */\n");
    if (!mulib_capasm_put_(source, "/* CAPASM: generated C with selected MuLIB headers. */\n"))
        return 0;
    for (i = 0; i < options->kernel_count; ++i) {
        const char *name = options->kernels[i];
        for (earlier = 0; earlier < i; ++earlier)
            if (!strcmp(options->kernels[earlier], name)) break;
        if (earlier == i &&
            !(mulib_capasm_put_(source, "#include \"mulib-") &&
              mulib_capasm_put_(source, name) &&
              mulib_capasm_put_(source, ".h\"\n"))) return 0;
    }
    return mulib_capasm_put_(source, "\n");
}

static int mulib_capasm_invoke_(const mulib_capasm_options_t *opts,
                               mulib_capasm_stage_t stage,
                               const mulib_capasm_tool_t *tool,
                               const char *input, size_t input_length,
                               char **result, size_t *length,
                               mulib_xmacir_error_t *error)
{
    mulib_xmacir_error_t tool_error = {0, NULL};
    char *next = NULL;
    size_t n = 0;
    int ok = opts->run(opts->user, stage, tool, input, input_length,
                       &next, &n, &tool_error);
    if (next == input) {
        return mulib_capasm_fail_(error, 0, "tool output aliases its input");
    }
    if (!ok || !next || n == (size_t)-1) {
        free(next);
        if (!ok && tool_error.message)
            return mulib_capasm_fail_(error, tool_error.offset, tool_error.message);
        return mulib_capasm_fail_(error, 0, ok ? "invalid tool output" : "host tool failed");
    }
    *result = next;
    *length = n;
    return 1;
}

static int mulib_capasm_same_ir_(const mulib_xmacir_program_t *original,
                                const mulib_xmacir_program_t *next,
                                unsigned long value,
                                mulib_xmacir_error_t *error)
{
    unsigned long current;
    if (original->mode != next->mode || strcmp(original->entry, next->entry) ||
        strcmp(original->bridge, next->bridge) ||
        strcmp(original->context_tag, next->context_tag) ||
        strcmp(original->error_tag, next->error_tag))
        return mulib_capasm_fail_(error, 0, "rewrite changed the XMacIR ABI");
    if (original->mode == MULIB_XMACIR_BLOB) {
        if (original->length != next->length ||
            (original->length && memcmp(original->bytes, next->bytes, original->length)))
            return mulib_capasm_fail_(error, 0, "rewrite changed the BLOB payload");
    } else {
        if (!mulib_capasm_eval_(next, &current, error)) return 0;
        if (current != value)
            return mulib_capasm_fail_(error, 0, "rewrite changed the U32 value");
    }
    return 1;
}

int mulib_capasm_emit(const mulib_xmacir_program_t *program,
                      const mulib_capasm_options_t *options,
                      char **output, size_t *output_length,
                      mulib_xmacir_error_t *error)
{
    static const mulib_capasm_stage_t stages[] = {
        MULIB_CAPASM_PREPROCESS, MULIB_CAPASM_COMPILE,
        MULIB_CAPASM_ASSEMBLE, MULIB_CAPASM_LINK
    };
    const mulib_capasm_tool_t *tools[4];
    mulib_capasm_buffer_ source = {NULL, 0, 0};
    mulib_xmacir_program_t rewritten = {0};
    const mulib_xmacir_program_t *current = program;
    char *canonical = NULL, *next = NULL, *terminal;
    size_t canonical_length, next_length, i;
    unsigned long scalar = 0;
    int built = 0, ok = 0;

    if (!program || !output || !output_length)
        return mulib_capasm_fail_(error, 0, "null CAPASM argument");
    if (!mulib_xmacir_validate(program, error) ||
        !mulib_capasm_c90_names_(program, error) ||
        !mulib_capasm_options_valid_(options, error)) return 0;

    if (options && options->pass_count) {
        if (program->mode == MULIB_XMACIR_U32 &&
            !mulib_capasm_eval_(program, &scalar, error)) goto done;
        for (i = 0; i < options->pass_count; ++i) {
            if (!mulib_xmacir_format(current, &canonical, &canonical_length, error) ||
                !mulib_capasm_invoke_(options, MULIB_CAPASM_REWRITE, options->passes + i,
                                     canonical, canonical_length, &next, &next_length,
                                     error)) goto done;
            free(canonical);
            canonical = NULL;
            mulib_xmacir_free(&rewritten);
            if (!mulib_xmacir_parse(next, next_length, &rewritten, error) ||
                !mulib_capasm_same_ir_(program, &rewritten, scalar, error)) goto done;
            free(next);
            next = NULL;
            current = &rewritten;
        }
    }

    if (!mulib_capasm_includes_(&source, options) ||
        !(current->mode == MULIB_XMACIR_U32 ? mulib_capasm_u32_(current, &source) :
                                            mulib_capasm_blob_(current, &source))) {
        mulib_capasm_fail_(error, 0, "cannot allocate generated C source");
        goto done;
    }
    if (options) {
        tools[0] = options->preprocessor;
        tools[1] = options->compiler;
        tools[2] = options->assembler;
        tools[3] = options->linker;
        for (i = 0; i < sizeof(stages) / sizeof(*stages); ++i) {
            if (!tools[i]) continue;
            if (!mulib_capasm_invoke_(options, stages[i], tools[i], source.data,
                                     source.length, &next, &next_length, error)) goto done;
            free(source.data);
            source.data = next;
            source.length = next_length;
            next = NULL;
            built = 1;
        }
    }
    if (built) {
        terminal = (char *)realloc(source.data, source.length + 1);
        if (!terminal) {
            mulib_capasm_fail_(error, 0, "cannot allocate tool output terminator");
            goto done;
        }
        source.data = terminal;
        source.data[source.length] = '\0';
    }
    *output = source.data;
    *output_length = source.length;
    source.data = NULL;
    if (error) {
        error->offset = 0;
        error->message = NULL;
    }
    ok = 1;

done:
    free(source.data);
    free(canonical);
    free(next);
    mulib_xmacir_free(&rewritten);
    return ok;
}
#endif /* MULIB_IMPL_CAPASM */
