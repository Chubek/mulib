#include "on1x_kernel_recipe.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <mulib-sha.h>

/* The LLSynth FFIHandler grammar is used by the host recipe compiler for
 * offline compilation and overflow proofs. Its generated C11 public API
 * does not expose the parse tree. Keep this runtime projection bounded,
 * allocation-free, and independent of the host compiler or generated tables. */
#define MAX_FIELDS 128u
#define MAX_DOCUMENT_NODES 16385u /* Includes the outer list. */

struct reader {
    const char *bytes;
    size_t pos, end, nodes;
    struct on1x_kernel_recipe_error *error;
};

struct word {
    const char *data;
    size_t size;
};

struct span {
    size_t start, end;
};

static int fail(struct reader *r, const char *message)
{
    if (r->error && !r->error->message) {
        r->error->offset = r->pos;
        r->error->message = message;
    }
    return 0;
}

static int whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char peek(struct reader *r)
{
    while (r->pos < r->end) {
        const char c = r->bytes[r->pos];
        if (whitespace(c)) {
            ++r->pos;
        } else if (c == ';' || c == '#') {
            do {
                ++r->pos;
            } while (r->pos < r->end && r->bytes[r->pos] != '\r' &&
                     r->bytes[r->pos] != '\n');
        } else {
            return c;
        }
    }
    return '\0';
}

static int delimiter(char c)
{
    return whitespace(c) || c == '(' || c == ')' || c == ';' || c == '#';
}

static int charge_node(struct reader *r)
{
    return ++r->nodes <= MAX_DOCUMENT_NODES || fail(r, "recipe document node limit exceeded");
}

static int paren(struct reader *r, char c)
{
    if (peek(r) != c)
        return fail(r, c == '(' ? "expected '('" : "expected ')'");
    if (c == '(' && !charge_node(r))
        return 0;
    ++r->pos;
    return 1;
}

static int read_word(struct reader *r, struct word *out)
{
    if (!peek(r) || delimiter(r->bytes[r->pos]))
        return fail(r, "expected recipe atom");
    if (!charge_node(r))
        return 0;
    const size_t start = r->pos;
    while (r->pos < r->end && !delimiter(r->bytes[r->pos])) {
        const unsigned char c = (unsigned char)r->bytes[r->pos];
        if (c <= 32 || c >= 127)
            return fail(r, "invalid recipe atom");
        ++r->pos;
    }
    *out = (struct word){r->bytes + start, r->pos - start};
    return 1;
}

static int equal(struct word w, const char *s)
{
    return w.size == strlen(s) && memcmp(w.data, s, w.size) == 0;
}

static void copy_word(char *out, struct word w)
{
    memcpy(out, w.data, w.size);
    out[w.size] = '\0';
}

static int digit(char c) { return c >= '0' && c <= '9'; }
static int lower(char c) { return c >= 'a' && c <= 'z'; }
static int letter(char c) { return lower(c) || (c >= 'A' && c <= 'Z') || c == '_'; }

static int identifier(struct word w)
{
    if (!w.size || w.size > 64 || !letter(w.data[0]))
        return 0;
    for (size_t i = 1; i < w.size; ++i)
        if (!letter(w.data[i]) && !digit(w.data[i]))
            return 0;
    return 1;
}

static int kernel_name(struct word w)
{
    if (!w.size || w.size > 128 || !lower(w.data[0]))
        return 0;
    for (size_t i = 1; i < w.size; ++i) {
        const char c = w.data[i];
        if (!lower(c) && !digit(c) &&
            (c != '-' || i + 1 == w.size || (!lower(w.data[i + 1]) && !digit(w.data[i + 1]))))
            return 0;
    }
    return 1;
}

static int selector(struct word w)
{
    if (equal(w, "*"))
        return 1;
    if (!w.size || w.size > 128 || (!letter(w.data[0]) && !digit(w.data[0])))
        return 0;
    for (size_t i = 1; i < w.size; ++i) {
        const char c = w.data[i];
        if (!letter(c) && !digit(c) && c != '.' && c != '-')
            return 0;
    }
    return 1;
}

static int sha256_spelling(struct word w)
{
    if (w.size != 64)
        return 0;
    for (size_t i = 0; i < w.size; ++i)
        if (!digit(w.data[i]) && (w.data[i] < 'a' || w.data[i] > 'f'))
            return 0;
    return 1;
}

static void encode_hex(char out[65], const unsigned char digest[32])
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i) {
        out[2 * i] = hex[digest[i] >> 4];
        out[2 * i + 1] = hex[digest[i] & 15];
    }
    out[64] = '\0';
}

int on1x_kernel_sha256(const void *bytes, size_t size, char out[65])
{
    unsigned char digest[32];
    if (!out || !mulib_sha256(bytes, size, digest))
        return 0;
    encode_hex(out, digest);
    return 1;
}

static int once(struct reader *r, unsigned *seen, unsigned flag)
{
    if (*seen & flag)
        return fail(r, "duplicate recipe field");
    *seen |= flag;
    return 1;
}

static int read_selector(struct reader *r)
{
    struct word w;
    return read_word(r, &w) && (selector(w) || fail(r, "invalid recipe selector"));
}

static int directive(struct reader *r, int hint)
{
    struct word action;
    if (!read_word(r, &action))
        return 0;
    if (hint ? !(equal(action, "kernel-hot") || equal(action, "canonicalized") ||
                 equal(action, "local-memory") || equal(action, "private-code") ||
                 equal(action, "cold-code") || equal(action, "no-inline"))
             : !(equal(action, "fold-identity") || equal(action, "memgraph-local") ||
                 equal(action, "meta-canonical")))
        return fail(r, "unknown recipe directive");
    unsigned seen = 0;
    while (peek(r) != ')') {
        struct word key;
        if (!paren(r, '(') || !read_word(r, &key))
            return 0;
        if (equal(key, "backend") || equal(key, "target")) {
            if (!once(r, &seen, equal(key, "backend") ? 1u : 2u) || !read_selector(r))
                return 0;
        } else if (equal(key, "speculative")) {
            if (!once(r, &seen, 4u))
                return 0;
        } else {
            return fail(r, "unknown recipe directive option");
        }
        if (!paren(r, ')'))
            return 0;
    }
    return 1;
}

/* Capture a bounded expression to defer type checking until params are known.
 * The byte spans live only during parsing and do not escape into the AST. */
static int capture_expr(struct reader *r, struct span *span)
{
    (void)peek(r);
    span->start = r->pos;
    size_t depth = 0;
    do {
        const char c = peek(r);
        if (c == '(') {
            if (++depth > ON1X_KERNEL_RECIPE_MAX_DEPTH + 1u)
                return fail(r, "recipe expression depth limit exceeded");
            if (!paren(r, '('))
                return 0;
        } else if (c == ')' && depth) {
            --depth;
            if (!paren(r, ')'))
                return 0;
        } else {
            struct word atom;
            if (!read_word(r, &atom))
                return 0;
        }
    } while (depth);
    span->end = r->pos;
    return 1;
}

static int integer(struct word w, int64_t *out)
{
    const size_t start = (size_t)(w.data[0] == '-');
    if (w.size <= start || w.size - start > 19 ||
        (w.size - start > 1 && w.data[start] == '0'))
        return 0;
    const uint64_t limit = (uint64_t)INT64_MAX + start;
    uint64_t value = 0;
    for (size_t i = start; i < w.size; ++i) {
        if (!digit(w.data[i]))
            return 0;
        const unsigned n = (unsigned)(w.data[i] - '0');
        if (value > (limit - n) / 10u)
            return 0;
        value = 10u * value + n;
    }
    *out = start && value == limit ? INT64_MIN : start ? -(int64_t)value : (int64_t)value;
    return 1;
}

enum kind { I64, BOOL };
struct operation {
    const char *name;
    enum on1x_kernel_recipe_op op;
    enum kind result, inputs[3];
    unsigned arity;
};

static const struct operation operations[] = {
    {"lt", ON1X_KERNEL_OP_LT, BOOL, {I64, I64}, 2},
    {"le", ON1X_KERNEL_OP_LE, BOOL, {I64, I64}, 2},
    {"gt", ON1X_KERNEL_OP_GT, BOOL, {I64, I64}, 2},
    {"ge", ON1X_KERNEL_OP_GE, BOOL, {I64, I64}, 2},
    {"eq", ON1X_KERNEL_OP_EQ, BOOL, {I64, I64}, 2},
    {"and", ON1X_KERNEL_OP_AND, BOOL, {BOOL, BOOL}, 2},
    {"or", ON1X_KERNEL_OP_OR, BOOL, {BOOL, BOOL}, 2},
    {"add", ON1X_KERNEL_OP_ADD, I64, {I64, I64}, 2},
    {"sub", ON1X_KERNEL_OP_SUB, I64, {I64, I64}, 2},
    {"if", ON1X_KERNEL_OP_IF, I64, {BOOL, I64, I64}, 3}
};

static int expression(struct reader *r, struct on1x_kernel_verified_export *out,
                      const struct word params[3], enum kind expected, int tail,
                      unsigned depth, unsigned *remaining, uint16_t *root)
{
    if (depth > ON1X_KERNEL_RECIPE_MAX_DEPTH || !*remaining)
        return fail(r, "recipe expression node or depth limit exceeded");
    --*remaining;
    struct on1x_kernel_recipe_node node = {0};
    struct word w;
    if (peek(r) == '(') {
        if (!paren(r, '(') || !read_word(r, &w))
            return 0;
        const struct operation *op = NULL;
        for (size_t i = 0; i < sizeof operations / sizeof operations[0]; ++i) {
            if (equal(w, operations[i].name)) {
                op = &operations[i];
                break;
            }
        }
        if (!op)
            return fail(r, "unknown recipe operator");
        if (op->result != expected || (op->op == ON1X_KERNEL_OP_IF && !tail))
            return fail(r, "recipe operator has wrong type or position");
        node.op = op->op;
        uint16_t *children[] = {&node.a, &node.b, &node.c};
        for (unsigned i = 0; i < op->arity; ++i)
            if (!expression(r, out, params, op->inputs[i], op->op == ON1X_KERNEL_OP_IF && i > 0,
                            depth + 1, remaining, children[i]))
                return 0;
        if (!paren(r, ')'))
            return 0;
    } else {
        if (!read_word(r, &w))
            return 0;
        if (expected == BOOL) {
            if (!equal(w, "true") && !equal(w, "false"))
                return fail(r, "expected boolean recipe operand");
            node.op = ON1X_KERNEL_OP_BOOL;
            node.immediate = equal(w, "true");
        } else {
            size_t index = 0;
            while (index < out->arity &&
                   (w.size != params[index].size || memcmp(w.data, params[index].data, w.size) != 0))
                ++index;
            if (index < out->arity) {
                node.op = ON1X_KERNEL_OP_PARAM;
                node.immediate = (int64_t)index;
            } else {
                node.op = ON1X_KERNEL_OP_INTEGER;
                if (!integer(w, &node.immediate))
                    return fail(r, "expected parameter or in-range decimal i64 literal");
            }
        }
    }
    *root = (uint16_t)out->node_count;
    out->nodes[out->node_count++] = node;
    return 1;
}

static int check_expr(struct reader *document, struct span span,
                      struct on1x_kernel_verified_export *out, const struct word params[3],
                      enum kind expected, uint16_t *root)
{
    struct reader r = {document->bytes, span.start, span.end, 0, document->error};
    unsigned remaining = ON1X_KERNEL_RECIPE_MAX_EXPR_NODES;
    return expression(&r, out, params, expected, expected == I64, 0, &remaining, root) &&
           (!peek(&r) || fail(&r, "trailing recipe expression input"));
}

static int hash_text(mulib_sha_ctx *ctx, const char *text)
{
    return mulib_sha_update(ctx, text, strlen(text));
}

static int hash_expr(mulib_sha_ctx *ctx, const struct on1x_kernel_recipe_node *nodes,
                     uint16_t root)
{
    const struct on1x_kernel_recipe_node *n = &nodes[root];
    switch (n->op) {
    case ON1X_KERNEL_OP_BOOL:
        return hash_text(ctx, n->immediate ? "true" : "false");
    case ON1X_KERNEL_OP_PARAM: {
        const char text[] = {'$', (char)('0' + n->immediate)};
        return mulib_sha_update(ctx, text, sizeof text);
    }
    case ON1X_KERNEL_OP_INTEGER: {
        char text[sizeof "-9223372036854775808"];
        const int size = snprintf(text, sizeof text, "%" PRId64, n->immediate);
        return size > 0 && (size_t)size < sizeof text &&
               mulib_sha_update(ctx, text, (size_t)size);
    }
    default:
        for (size_t i = 0; i < sizeof operations / sizeof operations[0]; ++i) {
            if (n->op == operations[i].op)
                return hash_text(ctx, "(") && hash_text(ctx, operations[i].name) &&
                       hash_text(ctx, " ") && hash_expr(ctx, nodes, n->a) &&
                       hash_text(ctx, " ") && hash_expr(ctx, nodes, n->b) &&
                       (operations[i].arity != 3 ||
                        (hash_text(ctx, " ") && hash_expr(ctx, nodes, n->c))) &&
                       hash_text(ctx, ")");
        }
        return 0;
    }
}

static int hash_behavior(struct on1x_kernel_verified_export *out)
{
    mulib_sha_ctx ctx;
    unsigned char digest[32];
    if (!mulib_sha_init(&ctx, MULIB_SHA256) ||
        !hash_text(&ctx, "(guard ") || !hash_expr(&ctx, out->nodes, out->guard) ||
        !hash_text(&ctx, ")\n(body ") || !hash_expr(&ctx, out->nodes, out->body) ||
        !hash_text(&ctx, ")\n") || !mulib_sha_final(&ctx, digest, sizeof digest))
        return 0;
    encode_hex(out->behavior_sha256, digest);
    return 1;
}

static int parameters(struct reader *r, struct word params[3], size_t *arity)
{
    *arity = 0;
    while (peek(r) != ')') {
        if (*arity == 3)
            return fail(r, "recipe requires two or three i64 parameters");
        struct word name;
        if (!read_word(r, &name))
            return 0;
        if (!identifier(name) || equal(name, "true") || equal(name, "false"))
            return fail(r, "invalid recipe parameter name");
        for (size_t i = 0; i < *arity; ++i)
            if (name.size == params[i].size && memcmp(name.data, params[i].data, name.size) == 0)
                return fail(r, "duplicate recipe parameter");
        params[(*arity)++] = name;
    }
    return *arity >= 2 || fail(r, "recipe requires two or three i64 parameters");
}

static int parse_export(struct reader *r, struct on1x_kernel_recipe *doc)
{
    if (doc->export_count == ON1X_KERNEL_RECIPE_MAX_EXPORTS)
        return fail(r, "recipe export limit exceeded");
    struct word member, symbol;
    if (!read_word(r, &member) || !read_word(r, &symbol))
        return 0;
    if (!identifier(member) || !identifier(symbol))
        return fail(r, "invalid recipe export member or C symbol");
    for (size_t i = 0; i < doc->export_count; ++i) {
        if (equal(member, doc->exports[i].member) || equal(symbol, doc->exports[i].symbol))
            return fail(r, "duplicate recipe export member or C symbol");
    }
    struct on1x_kernel_verified_export *out = &doc->exports[doc->export_count];
    copy_word(out->member, member);
    copy_word(out->symbol, symbol);
    struct word params[3];
    struct span guard = {0}, body = {0};
    unsigned seen = 0, fields = 0;
    enum { PARAMS = 1, GUARD = 2, BODY = 4, BACKEND = 8, TARGET = 16 };
    while (peek(r) != ')') {
        struct word key;
        if (++fields > MAX_FIELDS)
            return fail(r, "recipe export field limit exceeded");
        if (!paren(r, '(') || !read_word(r, &key))
            return 0;
        if (equal(key, "params")) {
            if (!once(r, &seen, PARAMS) || !parameters(r, params, &out->arity))
                return 0;
        } else if (equal(key, "guard") || equal(key, "body")) {
            const int is_guard = equal(key, "guard");
            if (!once(r, &seen, is_guard ? GUARD : BODY) ||
                !capture_expr(r, is_guard ? &guard : &body))
                return 0;
        } else if (equal(key, "backend") || equal(key, "target")) {
            if (!once(r, &seen, equal(key, "backend") ? BACKEND : TARGET) || !read_selector(r))
                return 0;
        } else if (equal(key, "hint") || equal(key, "rewrite")) {
            if (!directive(r, equal(key, "hint")))
                return 0;
        } else {
            return fail(r, "unknown recipe export field");
        }
        if (!paren(r, ')'))
            return 0;
    }
    if ((seen & (PARAMS | GUARD | BODY)) != (PARAMS | GUARD | BODY))
        return fail(r, "expected recipe parameters, guard and body");
    if (!check_expr(r, guard, out, params, BOOL, &out->guard) ||
        !check_expr(r, body, out, params, I64, &out->body))
        return 0;
    if (!hash_behavior(out))
        return fail(r, "cannot hash canonical recipe behavior");
    ++doc->export_count;
    return 1;
}

int on1x_kernel_recipe_parse(const void *bytes, size_t size,
                             struct on1x_kernel_recipe *out,
                             struct on1x_kernel_recipe_error *error)
{
    if (error)
        *error = (struct on1x_kernel_recipe_error){0, NULL};
    struct reader r = {bytes, 0, size, 0, error};
    if (!bytes || !out || !size || size > ON1X_KERNEL_RECIPE_MAX_BYTES)
        return fail(&r, "missing recipe or recipe byte limit exceeded");
    const char *nul = memchr(bytes, 0, size);
    if (nul) {
        r.pos = (size_t)(nul - r.bytes);
        return fail(&r, "NUL in recipe payload");
    }
    memset(out, 0, sizeof *out);
    struct word w;
    if (!paren(&r, '(') || !read_word(&r, &w) || !equal(w, "ffispec") ||
        !read_word(&r, &w) || !equal(w, "1"))
        return fail(&r, "expected (ffispec 1 ...)");

    unsigned seen = 0;
    enum { KERNEL = 1, HEADER = 2, SHA256 = 4, BACKEND = 8, TARGET = 16 };
    while (peek(&r) != ')') {
        struct word key;
        if (!paren(&r, '(') || !read_word(&r, &key))
            return 0;
        if (equal(key, "kernel")) {
            if (!once(&r, &seen, KERNEL) || !read_word(&r, &w))
                return 0;
            if (!kernel_name(w))
                return fail(&r, "invalid recipe kernel name");
            copy_word(out->kernel, w);
        } else if (equal(key, "header")) {
            if (!once(&r, &seen, HEADER) || !read_word(&r, &w))
                return 0;
            if (w.size >= sizeof out->header)
                return fail(&r, "invalid recipe source header");
            copy_word(out->header, w);
        } else if (equal(key, "sha256")) {
            if (!once(&r, &seen, SHA256) || !read_word(&r, &w))
                return 0;
            if (!sha256_spelling(w))
                return fail(&r, "invalid recipe source SHA-256 spelling");
            copy_word(out->source_sha256, w);
        } else if (equal(key, "backend") || equal(key, "target")) {
            if (!once(&r, &seen, equal(key, "backend") ? BACKEND : TARGET) || !read_selector(&r))
                return 0;
        } else if (equal(key, "export")) {
            if (!parse_export(&r, out))
                return 0;
        } else {
            return fail(&r, "unknown recipe declaration");
        }
        if (!paren(&r, ')'))
            return 0;
    }
    if (!paren(&r, ')') || peek(&r))
        return fail(&r, "trailing input after recipe");
    if ((seen & (KERNEL | HEADER | SHA256)) != (KERNEL | HEADER | SHA256) || !out->export_count)
        return fail(&r, "expected kernel, source header, source SHA-256 and exports");
    const size_t n = strlen(out->kernel);
    if (strlen(out->header) != n + 8u || memcmp(out->header, "mulib-", 6) != 0 ||
        memcmp(out->header + 6, out->kernel, n) != 0 || memcmp(out->header + 6 + n, ".h", 2) != 0)
        return fail(&r, "recipe source header must be mulib-<kernel>.h");
    return 1;
}
