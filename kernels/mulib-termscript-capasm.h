/** @file mulib-termscript-capasm.h
 * @brief Canonical TermScript AST and CAPASM-backed ANSI C emission.
 *
 * Define MULIB_IMPL_TERMSCRIPT_CAPASM in one translation unit and provide the
 * TermScript AST, strbuf, and CAPASM implementations. Compile the emitted unit
 * as ISO C90 and link it against the normal TermScript runtime, not the compiler.
 */
#ifndef MULIB_TERMSCRIPT_CAPASM_H
#define MULIB_TERMSCRIPT_CAPASM_H
#include "mulib-termscript-ast.h"
#include "mulib-capasm.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Build an independently owned AST, with nodes in preorder and a packed
 * byte pool in place of the original source. Diagnostics from running that
 * AST refer to offsets in the pool, not the original source. Source-node
 * inspection sees canonical nodes. Optimization can reduce VM step, stack
 * and allocation usage; dynamic quotation operands still receive the normal
 * runtime checks. The initialized destination (zeroed or previously owned)
 * is replaced only on success and must not be in use by a VM. */
int mulib_termscript_canonicalize(const mulib_termscript_program_t *program,
                                  mulib_termscript_program_t *canonical,
                                  mulib_termscript_error_t *error);

/* Emit an ISO C90 definition of
 *     int entry(struct mulib_termscript_vm *, struct mulib_termscript_error *);
 * in a malloc-owned, NUL-terminated buffer; free it with free(). CAPASM
 * options can instead select trusted host tool stages, whose final artifact
 * may contain embedded NUL bytes, or add declaration headers that require
 * a later C dialect than the generated body. Both outputs stay unchanged on failure.
 * output is required; output_length is optional and excludes the appended
 * terminator. NULL options selects pure C90. A capsule can encode up to
 * 65536 nodes and a 2 MiB canonical byte pool, subject to CAPASM limits.
 * entry must be a nonreserved ASCII C identifier beginning with a letter,
 * at most 63 bytes, and neither in the MuLIB namespace nor conflicting with
 * names in the embedding application.
 *
 * The output embeds a versioned, optimized executable AST, not source to
 * parse or native operations to execute. The runtime walks the decoded tree. */
int mulib_termscript_emit_ansi(const mulib_termscript_program_t *program,
                               const char *entry, const mulib_capasm_options_t *options,
                               char **output, size_t *output_length, mulib_termscript_error_t *error);

/* Parse source, then emit as above. Source is borrowed for this call only. */
int mulib_termscript_compile(const char *source, size_t length, const char *entry,
                             const mulib_capasm_options_t *options, char **output,
                             size_t *output_length, mulib_termscript_error_t *error);

/* Deprecated source compatibility names: these now emit ANSI C (ISO C90)
 * with default CAPASM options, not the old C11 emitter. */
int mulib_termscript_emit_c11(const mulib_termscript_program_t *program,
                              const char *entry, char **output,
                              mulib_termscript_error_t *error);
int mulib_termscript_compile_c11(const char *source, size_t length, const char *entry,
                                 char **output, mulib_termscript_error_t *error);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSCRIPT_CAPASM) && !defined(MULIB_TERMSCRIPT_CAPASM_IMPLEMENTED)
#define MULIB_TERMSCRIPT_CAPASM_IMPLEMENTED
#include "mulib-termscript-ast.h" /* Private chord scanner. */
#include "mulib-strbuf.h"
#include <stdlib.h>
#include <string.h>
#include "detail/mulib-termscript-capsule.inc"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  size_t input_next, output_parent, output_tail, control_base, prev, prev2;
} mulib_termscript_compiler_frame_;

typedef struct {
  size_t offset;
  int has_else;
} mulib_termscript_compiler_control_;

typedef enum {
  MULIB_TSC_LITERAL_, MULIB_TSC_ADD_, MULIB_TSC_SUB_, MULIB_TSC_MUL_,
  MULIB_TSC_DIV_, MULIB_TSC_MOD_
} mulib_termscript_compiler_op_;

typedef struct {
  int64_t value, left, right;
  mulib_termscript_compiler_op_ op;
} mulib_termscript_compiler_constant_;

static int
mulib_termscript_compiler_fail_(mulib_termscript_error_t *error, size_t offset,
                                const char *message)
{
  if (error) {
    error->offset = offset;
    error->message = message;
  }
  return 0;
}

static int
mulib_termscript_compiler_match_(const char *text, size_t len, const char *name)
{
  return strlen(name) == len && (len == 0 || !memcmp(text, name, len));
}

static int
mulib_termscript_compiler_prefix_(const char *text, size_t len, const char *name)
{
  size_t n = strlen(name);
  return n <= len && !memcmp(text, name, n);
}

static const char *
mulib_termscript_compiler_alias_(const char *text, size_t len)
{
  /* Only reserved words with identical runtime behavior. In particular,
   * import/use consult the host with their original spelling and `loop`
   * participates in adjacent syntax. */
  static const struct { const char *alias, *name; } names[] = {
    { "+", "add" }, { "-", "sub" }, { "*", "mul" }, { "/", "div" },
    { "=", "eq?" }, { "!=", "ne?" }, { "<", "lt?" }, { "<=", "le?" },
    { ">", "gt?" }, { ">=", "ge?" }, { "field", "at" }, { "set-field", "set-at" },
    { "length", "len" }, { "call", "invoke" },
    { "state", "mode" }, { "state?", "mode?" }
  };
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    if (mulib_termscript_compiler_match_(text, len, names[i].alias))
      return names[i].name;
  return NULL;
}

/* Lookahead always validates the index and source range before inspecting a
 * word. The main traversal diagnoses malformed branches/links itself. */
static int
mulib_termscript_compiler_is_(const mulib_termscript_program_t *p, size_t index, const char *name)
{
  if (index >= p->count) return 0;
  const mulib_termscript_node_t *node = &p->nodes[index];
  if (node->kind != MULIB_TERMSCRIPT_WORD || node->offset > p->length
      || node->length > p->length - node->offset || !node->length)
    return 0;
  const char *text = p->source + node->offset;
  size_t len = node->length;
  while (len && text[len - 1] == ';') --len;
  return mulib_termscript_compiler_match_(text, len, name);
}

static const char *const mulib_termscript_compiler_operations_[] = {
  "", "add", "sub", "mul", "div", "mod"
};

/* Literal integer operands cannot access bindings or dispatch to the host.
 * Faulting cases stay in the tree to preserve when the interpreter faults
 * and the values it has already put on the stack. */
static int
mulib_termscript_compiler_fold_(mulib_termscript_compiler_op_ op, int64_t a, int64_t b,
                                int64_t *out)
{
  switch (op) {
  case MULIB_TSC_ADD_:
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 0;
    *out = a + b;
    return 1;
  case MULIB_TSC_SUB_:
    if ((b > 0 && a < INT64_MIN + b) || (b < 0 && a > INT64_MAX + b)) return 0;
    *out = a - b;
    return 1;
  case MULIB_TSC_MUL_:
    if (a > 0) {
      if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a)) return 0;
    } else if (a < 0) {
      if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b)) return 0;
    }
    *out = a * b;
    return 1;
  case MULIB_TSC_DIV_: case MULIB_TSC_MOD_:
    if (!b || (a == INT64_MIN && b == -1)) return 0;
    *out = op == MULIB_TSC_MOD_ ? a % b : a / b;
    return 1;
  case MULIB_TSC_LITERAL_: break;
  }
  return 0;
}

static mulib_termscript_compiler_frame_
mulib_termscript_compiler_frame_init_(size_t head, size_t parent, size_t control_depth)
{
  mulib_termscript_compiler_frame_ frame = { head, parent, SIZE_MAX, control_depth, SIZE_MAX,
                                               SIZE_MAX };
  return frame;
}

static int
mulib_termscript_compiler_append_node_(mulib_termscript_program_t *p, mulib_strbuf_t *pool,
                                       mulib_termscript_compiler_frame_ *frame,
                                       mulib_termscript_kind_t kind, const char *text,
                                       size_t len, int64_t integer)
{
  size_t dest = p->count++;
  mulib_termscript_node_t node = { kind, pool->length, len, SIZE_MAX, SIZE_MAX, integer };
  if (len && !mulib_strbuf_append_n(pool, text, len)) return 0;
  p->nodes[dest] = node;
  if (frame->output_tail != SIZE_MAX) p->nodes[frame->output_tail].next = dest;
  else if (frame->output_parent != SIZE_MAX) p->nodes[frame->output_parent].first = dest;
  else p->root = dest;
  frame->output_tail = dest;
  return 1;
}

static int
mulib_termscript_compiler_flush_(mulib_termscript_program_t *p, mulib_strbuf_t *pool,
                                 mulib_termscript_compiler_frame_ *frame,
                                 const mulib_termscript_compiler_constant_ *values,
                                 size_t *count)
{
  for (size_t i = 0; i < *count; ++i) {
    const mulib_termscript_compiler_constant_ *v = &values[i];
    if (v->op == MULIB_TSC_LITERAL_) {
      if (!mulib_termscript_compiler_append_node_(p, pool, frame, MULIB_TERMSCRIPT_INTEGER,
                                                   NULL, 0, v->value))
        return 0;
    } else {
      /* The final instruction must still produce a computed value rather
       * than a literal. The language's `node` operation can tell them apart. */
      const char *word = mulib_termscript_compiler_operations_[v->op];
      if (!mulib_termscript_compiler_append_node_(p, pool, frame, MULIB_TERMSCRIPT_INTEGER,
                                                   NULL, 0, v->left)
          || !mulib_termscript_compiler_append_node_(p, pool, frame, MULIB_TERMSCRIPT_INTEGER,
                                                     NULL, 0, v->right)
          || !mulib_termscript_compiler_append_node_(p, pool, frame, MULIB_TERMSCRIPT_WORD,
                                                     word, strlen(word), 0))
        return 0;
    }
  }
  *count = 0;
  return 1;
}

int
mulib_termscript_canonicalize(const mulib_termscript_program_t *program,
                              mulib_termscript_program_t *canonical,
                              mulib_termscript_error_t *error)
{
  mulib_termscript_program_t result = { NULL, 0, NULL, 0, SIZE_MAX };
  mulib_strbuf_t pool = { NULL, 0, 0 };
  mulib_termscript_compiler_frame_ *frames = NULL;
  mulib_termscript_compiler_control_ *control = NULL;
  mulib_termscript_compiler_constant_ *values = NULL;
  unsigned char *seen = NULL;
  size_t depth = 0, control_depth = 0, constants = 0, visited = 0, offset = 0;
  const char *problem = "out of memory";

  mulib_termscript_compiler_fail_(error, 0, NULL);
  if (!program || !canonical || (program->length && !program->source)
      || (program->count && !program->nodes))
    return mulib_termscript_compiler_fail_(error, 0, "invalid program or destination");
  if (program->count > SIZE_MAX / sizeof(*result.nodes)
      || program->count > SIZE_MAX / sizeof(*control)
      || program->count > SIZE_MAX / sizeof(*values)
      || program->count >= SIZE_MAX / sizeof(*frames))
    return mulib_termscript_compiler_fail_(error, 0, "AST too large");
  if (program->count) {
    result.nodes = (mulib_termscript_node_t *)malloc(program->count * sizeof(*result.nodes));
    seen = (unsigned char *)calloc(program->count, 1);
    control = (mulib_termscript_compiler_control_ *)malloc(program->count * sizeof(*control));
    values = (mulib_termscript_compiler_constant_ *)malloc(program->count * sizeof(*values));
    if (!result.nodes || !seen || !control || !values) goto done;
  }
  frames = (mulib_termscript_compiler_frame_ *)malloc((program->count + 1) * sizeof(*frames));
  if (!frames || !mulib_strbuf_append(&pool, "")) goto done;
  frames[depth++] = mulib_termscript_compiler_frame_init_(program->root, SIZE_MAX, 0);

  /* Instructions are appended in preorder. A fold is local to the active
   * sibling list and must flush before entry to a nested list. The separate
   * directive stack cannot close a conditional opened in a different list. */
  while (depth) {
    mulib_termscript_compiler_frame_ *frame = &frames[depth - 1];
    size_t i = frame->input_next;
    if (i == SIZE_MAX) {
      if (control_depth != frame->control_base) {
        offset = control[control_depth - 1].offset;
        problem = "unterminated %if-term:";
        goto done;
      }
      if (!mulib_termscript_compiler_flush_(&result, &pool, frame, values, &constants)) goto done;
      --depth;
      continue;
    }
    if (i >= program->count || seen[i]) {
      problem = "invalid, cyclic or shared AST link";
      goto done;
    }
    const mulib_termscript_node_t *input = &program->nodes[i];
    offset = input->offset;
    if ((input->kind != MULIB_TERMSCRIPT_CHORD && mulib_tscap_wire_kind_(input->kind) < 0)
        || offset > program->length || input->length > program->length - offset
        || (input->first != SIZE_MAX && !mulib_tscap_children_(input->kind))) {
      problem = "invalid AST node";
      goto done;
    }
    seen[i] = 1;
    ++visited;
    frame->input_next = input->next;
    const char *text = program->source ? program->source + offset : "";
    size_t len = input->length;
    size_t prev = frame->prev, prev2 = frame->prev2;
    frame->prev2 = prev;
    frame->prev = i;
    mulib_termscript_kind_t parent_kind = frame->output_parent == SIZE_MAX ? MULIB_TERMSCRIPT_BLOCK
      : result.nodes[frame->output_parent].kind;
    if ((parent_kind == MULIB_TERMSCRIPT_RECORD) != (input->kind == MULIB_TERMSCRIPT_FIELD)
        || (input->kind == MULIB_TERMSCRIPT_FIELD
            && (input->first >= program->count || program->nodes[input->first].next != SIZE_MAX))) {
      problem = "invalid record field";
      goto done;
    }
    if (input->kind == MULIB_TERMSCRIPT_DIRECTIVE) {
      if (mulib_termscript_compiler_prefix_(text, len, "%if-term:")) {
        if (!mulib_tscap_term_valid_(text + 9, len - 9)) {
          problem = "invalid %if-term: operand";
          goto done;
        }
        if (control_depth - frame->control_base == 128) {
          problem = "directive nesting limit";
          goto done;
        }
        mulib_termscript_compiler_control_ opening = { offset, 0 };
        control[control_depth++] = opening;
      } else if (mulib_termscript_compiler_prefix_(text, len, "%else:")) {
        if (control_depth == frame->control_base || control[control_depth - 1].has_else) {
          problem = "unmatched or duplicate %else:";
          goto done;
        }
        control[control_depth - 1].has_else = 1;
      } else if (mulib_termscript_compiler_prefix_(text, len, "%end:")) {
        if (control_depth == frame->control_base) {
          problem = "unmatched %end:";
          goto done;
        }
        --control_depth;
      }
    }

    if (input->kind == MULIB_TERMSCRIPT_INTEGER) {
      mulib_termscript_compiler_constant_ literal = { input->integer, 0, 0, MULIB_TSC_LITERAL_ };
      values[constants++] = literal;
      continue;
    }
    if (input->kind == MULIB_TERMSCRIPT_WORD) {
      while (len && text[len - 1] == ';') --len;
      int after_block = prev != SIZE_MAX && program->nodes[prev].kind == MULIB_TERMSCRIPT_BLOCK;
      if (after_block && mulib_termscript_compiler_is_(program, prev2, "if")
          && mulib_termscript_compiler_match_(text, len, "else")
          && (input->next >= program->count || program->nodes[input->next].kind != MULIB_TERMSCRIPT_BLOCK)) {
        problem = "else requires a block";
        goto done;
      }
      /* These tokens participate in spelling-sensitive adjacent syntax. */
      int parameter_pair = prev != SIZE_MAX && program->nodes[prev].kind == MULIB_TERMSCRIPT_PARAMETER
        && input->next < program->count && program->nodes[input->next].kind == MULIB_TERMSCRIPT_PARAMETER;
      if (!parameter_pair) {
        const char *alias = mulib_termscript_compiler_alias_(text, len);
        if (alias) {
          text = alias;
          len = strlen(alias);
        }
      }
      /* A symbol or arbitrary word terminates the fold. The deferred values
       * never contain a quote, binding access, or host call. */
      if (constants >= 2) {
        mulib_termscript_compiler_constant_ *left = &values[constants - 2];
        const mulib_termscript_compiler_constant_ *right = &values[constants - 1];
        mulib_termscript_compiler_op_ op = MULIB_TSC_LITERAL_;
        for (size_t j = MULIB_TSC_ADD_; j <= MULIB_TSC_MOD_; ++j)
          if (mulib_termscript_compiler_match_(text, len, mulib_termscript_compiler_operations_[j])) {
            op = (mulib_termscript_compiler_op_)j;
            break;
          }
        int64_t value;
        if (mulib_termscript_compiler_fold_(op, left->value, right->value, &value)) {
          left->left = left->value;
          left->right = right->value;
          left->value = value;
          left->op = op;
          --constants;
          continue;
        }
      }
    }
    mulib_termscript_kind_t kind = input->kind;
    char key[MULIB_TERMSCRIPT_CHORD_KEY_LIMIT_ + 1];
    if (kind == MULIB_TERMSCRIPT_CHORD) {
      size_t consumed;
      if (!mulib_termscript_chord_(text, len, &consumed, key, &len, NULL)
          || consumed != input->length) {
        problem = "invalid chord AST";
        goto done;
      }
      kind = MULIB_TERMSCRIPT_KEY;
      text = key;
    }
    if (!mulib_termscript_compiler_flush_(&result, &pool, frame, values, &constants)
        || !mulib_termscript_compiler_append_node_(&result, &pool, frame, kind, text,
                                                     mulib_tscap_text_(kind)
                                                    ? len : 0,
                                                    input->kind == MULIB_TERMSCRIPT_CHARACTER
                                                    ? input->integer : 0))
      goto done;
    if (input->first != SIZE_MAX)
      frames[depth++] = mulib_termscript_compiler_frame_init_(input->first, frame->output_tail,
                                                              control_depth);
  }
  if (visited != program->count) {
    offset = 0;
    problem = "unreachable AST node";
    goto done;
  }

  result.source = pool.data;
  result.length = pool.length;
  mulib_termscript_program_free(canonical);
  *canonical = result;
  pool.data = NULL;
  result.nodes = NULL;
  problem = NULL;
done:
  free(result.nodes);
  free(frames);
  free(control);
  free(values);
  free(seen);
  mulib_strbuf_free(&pool);
  return problem ? mulib_termscript_compiler_fail_(error, offset, problem) : 1;
}

static int
mulib_termscript_compiler_alpha_(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int
mulib_termscript_compiler_entry_(const char *entry)
{
  /* Leading underscores are reserved at file scope. Also exclude names of
   * interfaces/macros from our included headers and the host entry point. */
  static const char *const rejected[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline",
    "int", "long", "register", "restrict", "return", "short", "signed", "sizeof",
    "static", "struct", "switch", "typedef", "union", "unsigned", "void",
    "volatile", "while", "main", "bool", "true", "false", "NULL", "offsetof",
    "size_t", "ptrdiff_t", "wchar_t", "max_align_t", "intptr_t", "uintptr_t",
    "intmax_t", "uintmax_t"
  };
  size_t n = 0;
  if (!entry || !mulib_termscript_compiler_alpha_(entry[0])) return 0;
  while (entry[n]) {
    char c = entry[n++];
    if (!mulib_termscript_compiler_alpha_(c) && c != '_' && !(c >= '0' && c <= '9'))
      return 0;
    if (n > 63) return 0;
  }
  if (!strncmp(entry, "mulib_", 6) || !strncmp(entry, "MULIB_", 6)
      || !strncmp(entry, "INT", 3) || !strncmp(entry, "UINT", 4)
      || !strncmp(entry, "SIZE_", 5) || !strncmp(entry, "PTRDIFF_", 8)
      || !strncmp(entry, "WCHAR_", 6) || !strncmp(entry, "WINT_", 5))
    return 0;
  if ((!strncmp(entry, "int", 3) || !strncmp(entry, "uint", 4))
      && n >= 2 && entry[n - 2] == '_' && entry[n - 1] == 't')
    return 0;
  for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i)
    if (!strcmp(entry, rejected[i])) return 0;
  return 1;
}

static unsigned char *
mulib_termscript_compiler_encode_(const mulib_termscript_program_t *p, size_t *size,
                                  mulib_termscript_error_t *error)
{
  if (p->count > MULIB_TSCAP_MAX_NODES_ || p->length > MULIB_TSCAP_MAX_POOL_
      || !mulib_tscap_size_(p->count, p->length, size)) {
    mulib_termscript_compiler_fail_(error, 0, "TermScript capsule size limit");
    return NULL;
  }
  unsigned char *bytes = (unsigned char *)malloc(*size);
  if (!bytes) {
    mulib_termscript_compiler_fail_(error, 0, "out of memory");
    return NULL;
  }
  memcpy(bytes, mulib_tscap_magic_, sizeof mulib_tscap_magic_);
  mulib_tscap_write_(bytes + 8, p->count);
  mulib_tscap_write_(bytes + 16, p->length);
  mulib_tscap_write_(bytes + 24, mulib_tscap_link_(p->root));
  size_t parent = SIZE_MAX;
  unsigned char *row = bytes + MULIB_TSCAP_HEADER_;
  for (size_t i = 0; i < p->count; ++i, row += MULIB_TSCAP_NODE_) {
    const mulib_termscript_node_t *node = &p->nodes[i];
    row[MULIB_TSCAP_KIND_] = (unsigned char)mulib_tscap_wire_kind_(node->kind);
    mulib_tscap_write_(row + MULIB_TSCAP_OFFSET_, node->offset);
    mulib_tscap_write_(row + MULIB_TSCAP_LENGTH_, node->length);
    mulib_tscap_write_(row + MULIB_TSCAP_FIRST_, mulib_tscap_link_(node->first));
    mulib_tscap_write_(row + MULIB_TSCAP_NEXT_, mulib_tscap_link_(node->next));
    mulib_tscap_write_(row + MULIB_TSCAP_PARENT_, mulib_tscap_link_(parent));
    mulib_tscap_write_(row + MULIB_TSCAP_INTEGER_, (uint64_t)node->integer);
    if (node->first != SIZE_MAX) parent = i;
    else {
      size_t next = node->next;
      while (next == SIZE_MAX && parent != SIZE_MAX) {
        next = p->nodes[parent].next;
        parent = mulib_tscap_index_(mulib_tscap_read_(
          mulib_tscap_row_(bytes, parent) + MULIB_TSCAP_PARENT_));
      }
    }
  }
  memcpy(row, p->source, p->length);
  size_t count, pool, bad;
  if (!mulib_tscap_validate_(bytes, *size, &count, &pool, &bad)) {
    free(bytes);
    mulib_termscript_compiler_fail_(error, bad, "invalid canonical TermScript capsule");
    return NULL;
  }
  return bytes;
}

int
mulib_termscript_emit_ansi(const mulib_termscript_program_t *program,
                           const char *entry, const mulib_capasm_options_t *options,
                           char **output, size_t *output_length, mulib_termscript_error_t *error)
{
  mulib_termscript_program_t canonical = { NULL, 0, NULL, 0, SIZE_MAX };
  mulib_xmacir_program_t blob = { 0 };
  mulib_xmacir_error_t capasm_error = { 0, NULL };
  size_t result_length;
  char *result = NULL;
  if (!output || !mulib_termscript_compiler_entry_(entry))
    return mulib_termscript_compiler_fail_(error, 0, "invalid C entry identifier or output");
  if (!mulib_termscript_canonicalize(program, &canonical, error)) return 0;
  blob.mode = MULIB_XMACIR_BLOB;
  memcpy(blob.entry, entry, strlen(entry) + 1);
  memcpy(blob.bridge, "mulib_termscript_run_capsule", sizeof "mulib_termscript_run_capsule");
  memcpy(blob.context_tag, "mulib_termscript_vm", sizeof "mulib_termscript_vm");
  memcpy(blob.error_tag, "mulib_termscript_error", sizeof "mulib_termscript_error");
  blob.bytes = mulib_termscript_compiler_encode_(&canonical, &blob.length, error);
  mulib_termscript_program_free(&canonical);
  if (!blob.bytes) return 0;
  int ok = mulib_capasm_emit(&blob, options, &result, &result_length, &capasm_error);
  free(blob.bytes);
  if (!ok)
    return mulib_termscript_compiler_fail_(error, capasm_error.offset,
                                          capasm_error.message ? capasm_error.message : "CAPASM failed");
  *output = result;
  if (output_length) *output_length = result_length;
  return 1;
}

int
mulib_termscript_compile(const char *source, size_t length, const char *entry,
                         const mulib_capasm_options_t *options, char **output,
                         size_t *output_length, mulib_termscript_error_t *error)
{
  mulib_termscript_program_t program = { NULL, 0, NULL, 0, SIZE_MAX };
  if (!output || !mulib_termscript_compiler_entry_(entry))
    return mulib_termscript_compiler_fail_(error, 0, "invalid C entry identifier or output");
  if (!mulib_termscript_parse(source, length, &program, error)) return 0;
  int ok = mulib_termscript_emit_ansi(&program, entry, options, output, output_length, error);
  mulib_termscript_program_free(&program);
  return ok;
}

int
mulib_termscript_emit_c11(const mulib_termscript_program_t *program,
                          const char *entry, char **output, mulib_termscript_error_t *error)
{
  return mulib_termscript_emit_ansi(program, entry, NULL, output, NULL, error);
}

int
mulib_termscript_compile_c11(const char *source, size_t length, const char *entry,
                             char **output, mulib_termscript_error_t *error)
{
  return mulib_termscript_compile(source, length, entry, NULL, output, NULL, error);
}

#ifdef __cplusplus
}
#endif
#endif
