/** @file mulib-termscript-compiler.h
 * @brief Canonical TermScript AST and ISO C11 emission.
 *
 * Define MULIB_IMPL_TERMSCRIPT_COMPILER in one translation unit and link the
 * TermScript AST and strbuf implementations. Compile the emitted C separately
 * and link it against the TermScript runtime, not the compiler.
 */
#ifndef MULIB_TERMSCRIPT_COMPILER_H
#define MULIB_TERMSCRIPT_COMPILER_H
#include "mulib-termscript-ast.h"
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

/* Emit a definition of
 *     int entry(mulib_termscript_vm_t *, mulib_termscript_error_t *);
 * into a malloc-owned, NUL-terminated string. The output pointer is assigned
 * only on success; free the returned string with free(). The entry must be a
 * nonreserved ASCII C identifier beginning with a letter, at most 63 bytes,
 * outside the MuLIB namespace and not conflicting with names in the embedding
 * application.
 * The generated unit embeds an optimized executable AST, including the
 * checked lexical control forms. The runtime executes it by tree walk;
 * operations and branches are not inlined into native C. It neither reads
 * nor parses the compiler input at run time. */
int mulib_termscript_emit_c11(const mulib_termscript_program_t *program,
                              const char *entry, char **output,
                              mulib_termscript_error_t *error);

/* Parse source, then emit as above. Source is borrowed for this call only. */
int mulib_termscript_compile_c11(const char *source, size_t length, const char *entry,
                                 char **output, mulib_termscript_error_t *error);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSCRIPT_COMPILER) && !defined(MULIB_TERMSCRIPT_COMPILER_IMPLEMENTED)
#define MULIB_TERMSCRIPT_COMPILER_IMPLEMENTED
#include "mulib-strbuf.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static const char *
mulib_termscript_compiler_kind_(mulib_termscript_kind_t kind)
{
  switch (kind) {
  case MULIB_TERMSCRIPT_WORD: return "WORD";
  case MULIB_TERMSCRIPT_INTEGER: return "INTEGER";
  case MULIB_TERMSCRIPT_STRING: return "STRING";
  case MULIB_TERMSCRIPT_COMMAND: return "COMMAND";
  case MULIB_TERMSCRIPT_CHARACTER: return "CHARACTER";
  case MULIB_TERMSCRIPT_SYMBOL: return "SYMBOL";
  case MULIB_TERMSCRIPT_ARRAY_SYMBOL: return "ARRAY_SYMBOL";
  case MULIB_TERMSCRIPT_RECORD_SYMBOL: return "RECORD_SYMBOL";
  case MULIB_TERMSCRIPT_PARAMETER: return "PARAMETER";
  case MULIB_TERMSCRIPT_KEY: return "KEY";
  case MULIB_TERMSCRIPT_BLOCK: return "BLOCK";
  case MULIB_TERMSCRIPT_TUPLE: return "TUPLE";
  case MULIB_TERMSCRIPT_ARRAY: return "ARRAY";
  case MULIB_TERMSCRIPT_RECORD: return "RECORD";
  case MULIB_TERMSCRIPT_FIELD: return "FIELD";
  case MULIB_TERMSCRIPT_DIRECTIVE: return "DIRECTIVE";
  case MULIB_TERMSCRIPT_ESCAPE: return "ESCAPE";
  }
  return NULL;
}

static int
mulib_termscript_compiler_children_(mulib_termscript_kind_t kind)
{
  return kind == MULIB_TERMSCRIPT_BLOCK || kind == MULIB_TERMSCRIPT_TUPLE
    || kind == MULIB_TERMSCRIPT_ARRAY || kind == MULIB_TERMSCRIPT_RECORD
    || kind == MULIB_TERMSCRIPT_FIELD;
}

static int
mulib_termscript_compiler_text_(mulib_termscript_kind_t kind)
{
  return kind != MULIB_TERMSCRIPT_INTEGER && kind != MULIB_TERMSCRIPT_BLOCK
    && kind != MULIB_TERMSCRIPT_TUPLE && kind != MULIB_TERMSCRIPT_ARRAY
    && kind != MULIB_TERMSCRIPT_RECORD && kind != MULIB_TERMSCRIPT_CHARACTER;
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
   * import/use both consult the host with their original spelling. */
  static const struct { const char *alias, *name; } names[] = {
    { "+", "add" }, { "-", "sub" }, { "*", "mul" }, { "/", "div" },
    { "=", "eq?" }, { "!=", "ne?" }, { "<", "lt?" }, { "<=", "le?" },
    { ">", "gt?" }, { ">=", "ge?" }, { "field", "at" }, { "set-field", "set-at" },
    { "length", "len" }, { "call", "invoke" }, { "loop", "while" },
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

static int
mulib_termscript_compiler_term_sep_(char c)
{
  return c == ' ' || (c >= '\t' && c <= '\r') || c == ',' || c == '|';
}

static int
mulib_termscript_compiler_term_valid_(const char *text, size_t len)
{
  size_t count = 0;
  for (size_t i = 0; i < len;) {
    while (i < len && mulib_termscript_compiler_term_sep_(text[i])) ++i;
    if (i == len) break;
    ++count;
    if (text[i] == '"') {
      do { ++i; } while (i < len && text[i] != '"');
      if (i == len || (++i < len && !mulib_termscript_compiler_term_sep_(text[i])))
        return 0;
    } else {
      do { ++i; } while (i < len && !mulib_termscript_compiler_term_sep_(text[i]));
    }
  }
  return count != 0;
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
    if (!mulib_termscript_compiler_kind_(input->kind)
        || offset > program->length || input->length > program->length - offset
        || (input->first != SIZE_MAX && !mulib_termscript_compiler_children_(input->kind))) {
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
        if (!mulib_termscript_compiler_term_valid_(text + 9, len - 9)) {
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
      int loop_marker = after_block && mulib_termscript_compiler_is_(program, prev2, "while")
        && mulib_termscript_compiler_match_(text, len, "loop");
      int parameter_pair = prev != SIZE_MAX && program->nodes[prev].kind == MULIB_TERMSCRIPT_PARAMETER
        && input->next < program->count && program->nodes[input->next].kind == MULIB_TERMSCRIPT_PARAMETER;
      if (!loop_marker && !parameter_pair) {
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
    if (!mulib_termscript_compiler_flush_(&result, &pool, frame, values, &constants)
        || !mulib_termscript_compiler_append_node_(&result, &pool, frame, input->kind, text,
                                                    mulib_termscript_compiler_text_(input->kind)
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

static int
mulib_termscript_compiler_appendf_(mulib_strbuf_t *buffer, const char *format, ...)
{
  /* Callers interpolate only bounded identifiers or integer constants. */
  char line[256];
  va_list args;
  va_start(args, format);
  int n = vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  return n >= 0 && (size_t)n < sizeof(line)
    && mulib_strbuf_append_n(buffer, line, (size_t)n);
}

static int
mulib_termscript_compiler_index_(mulib_strbuf_t *buffer, size_t i)
{
  return i == SIZE_MAX ? mulib_strbuf_append(buffer, "SIZE_MAX")
    : mulib_termscript_compiler_appendf_(buffer, "%zu", i);
}

static int
mulib_termscript_compiler_integer_(mulib_strbuf_t *buffer, int64_t value)
{
  if (value == INT64_MIN) return mulib_strbuf_append(buffer, "INT64_MIN");
  if (value < 0) return mulib_termscript_compiler_appendf_(buffer, "-INT64_C(%" PRId64 ")", -value);
  return mulib_termscript_compiler_appendf_(buffer, "INT64_C(%" PRId64 ")", value);
}

static int
mulib_termscript_compiler_emit_(const mulib_termscript_program_t *program,
                                const char *entry, mulib_strbuf_t *buffer)
{
  if (!mulib_strbuf_append(buffer,
       "/* TermScript executable AST, generated by MuLIB. */\n"
       "#include \"mulib-termscript-runtime.h\"\n\n"
       "static unsigned char mulib_termscript_bytes[] = {\n  "))
    return 0;
  /* Numeric byte initializers preserve the exact data without quotes, C
   * escape ambiguity, trigraphs, or limits on C string literal length. */
  for (size_t i = 0; i < program->length; ++i) {
    if (!mulib_termscript_compiler_appendf_(buffer, "0x%02x,",
          (unsigned)(unsigned char)program->source[i])
        || !mulib_strbuf_append(buffer, i % 16 == 15 ? "\n  " : " "))
      return 0;
  }
  if (!mulib_strbuf_append(buffer, "0\n};\n")) return 0;
  if (program->count) {
    if (!mulib_strbuf_append(buffer, "static mulib_termscript_node_t mulib_termscript_nodes[] = {\n"))
      return 0;
    for (size_t i = 0; i < program->count; ++i) {
      const mulib_termscript_node_t *node = &program->nodes[i];
      if (!mulib_termscript_compiler_appendf_(buffer,
           "  { MULIB_TERMSCRIPT_%s, %zu, %zu, ",
           mulib_termscript_compiler_kind_(node->kind), node->offset, node->length)
          || !mulib_termscript_compiler_index_(buffer, node->first)
          || !mulib_strbuf_append(buffer, ", ")
          || !mulib_termscript_compiler_index_(buffer, node->next)
          || !mulib_strbuf_append(buffer, ", ")
          || !mulib_termscript_compiler_integer_(buffer, node->integer)
          || !mulib_strbuf_append(buffer, " },\n"))
        return 0;
    }
    if (!mulib_strbuf_append(buffer, "};\n")) return 0;
  }
  return mulib_termscript_compiler_appendf_(buffer,
         "static const mulib_termscript_program_t mulib_termscript_program = {\n"
         "  (char *)mulib_termscript_bytes, %zu, %s, %zu, ",
         program->length, program->count ? "mulib_termscript_nodes" : "NULL", program->count)
    && mulib_termscript_compiler_index_(buffer, program->root)
    && mulib_termscript_compiler_appendf_(buffer,
         "\n};\n\nint %s(mulib_termscript_vm_t *, mulib_termscript_error_t *);\n\n", entry)
    && mulib_termscript_compiler_appendf_(buffer,
         "int\n%s(mulib_termscript_vm_t *vm, mulib_termscript_error_t *error)\n"
         "{\n  return mulib_termscript_run(vm, &mulib_termscript_program, error);\n}\n", entry);
}

int
mulib_termscript_emit_c11(const mulib_termscript_program_t *program,
                          const char *entry, char **output, mulib_termscript_error_t *error)
{
  mulib_termscript_program_t canonical = { NULL, 0, NULL, 0, SIZE_MAX };
  mulib_strbuf_t buffer = { NULL, 0, 0 };
  if (!output || !mulib_termscript_compiler_entry_(entry))
    return mulib_termscript_compiler_fail_(error, 0, "invalid C entry identifier or output");
  if (!mulib_termscript_canonicalize(program, &canonical, error)) return 0;
  int ok = mulib_termscript_compiler_emit_(&canonical, entry, &buffer);
  mulib_termscript_program_free(&canonical);
  if (!ok) {
    mulib_strbuf_free(&buffer);
    return mulib_termscript_compiler_fail_(error, 0, "unable to emit C11");
  }
  *output = buffer.data;
  return 1;
}

int
mulib_termscript_compile_c11(const char *source, size_t length, const char *entry,
                             char **output, mulib_termscript_error_t *error)
{
  mulib_termscript_program_t program = { NULL, 0, NULL, 0, SIZE_MAX };
  if (!output || !mulib_termscript_compiler_entry_(entry))
    return mulib_termscript_compiler_fail_(error, 0, "invalid C entry identifier or output");
  if (!mulib_termscript_parse(source, length, &program, error)) return 0;
  int ok = mulib_termscript_emit_c11(&program, entry, output, error);
  mulib_termscript_program_free(&program);
  return ok;
}

#ifdef __cplusplus
}
#endif
#endif
