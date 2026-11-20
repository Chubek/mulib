/** @file mulib-termscript-runtime.h
 * @brief Bounded TermScript stack interpreter. Define MULIB_IMPL_TERMSCRIPT_RUNTIME.
 */
#ifndef MULIB_TERMSCRIPT_RUNTIME_H
#define MULIB_TERMSCRIPT_RUNTIME_H
#include "mulib-termscript-ast.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MULIB_TERMSCRIPT_VALUE_NIL = 0,
  MULIB_TERMSCRIPT_VALUE_BOOL,
  MULIB_TERMSCRIPT_VALUE_INTEGER,
  MULIB_TERMSCRIPT_VALUE_STRING,
  MULIB_TERMSCRIPT_VALUE_CHARACTER,
  MULIB_TERMSCRIPT_VALUE_COMMAND,
  MULIB_TERMSCRIPT_VALUE_SYMBOL,
  MULIB_TERMSCRIPT_VALUE_ARRAY_SYMBOL,
  MULIB_TERMSCRIPT_VALUE_RECORD_SYMBOL,
  MULIB_TERMSCRIPT_VALUE_PARAMETER,
  MULIB_TERMSCRIPT_VALUE_KEY,
  MULIB_TERMSCRIPT_VALUE_FIELD,
  MULIB_TERMSCRIPT_VALUE_ARRAY,
  MULIB_TERMSCRIPT_VALUE_TUPLE,
  MULIB_TERMSCRIPT_VALUE_RECORD,
  MULIB_TERMSCRIPT_VALUE_QUOTE,
  MULIB_TERMSCRIPT_VALUE_NODE,
  MULIB_TERMSCRIPT_VALUE_MODULE,
  MULIB_TERMSCRIPT_VALUE_ESCAPE
} mulib_termscript_value_kind_t;

typedef struct mulib_termscript_object mulib_termscript_object_t;
typedef struct mulib_termscript_vm mulib_termscript_vm_t;
/* Integer, boolean (0/1) and character (Unicode scalar) use integer.
 * Text-like values use a byte span and are not necessarily NUL terminated;
 * a character also carries its UTF-8 bytes. Access collections via the
 * value_count/value_at functions. program/node identify source-backed values. */
typedef struct {
  mulib_termscript_value_kind_t kind;
  int64_t integer;
  const char *text;
  size_t length;
  const mulib_termscript_object_t *object;
  const mulib_termscript_program_t *program;
  size_t node;
} mulib_termscript_value_t;

/* Zero selects the default for each limit (100000 steps, 64 calls, 4 MiB
 * of allocations, 4096 stack values). Call depth has a hard cap of 128.
 * The allocation budget counts interpreter storage and loaded source/node
 * bytes, not storage allocated independently by host or parser callbacks.
 * Persistent VM objects are kept until clear, even if no longer reachable. */
typedef struct {
  size_t max_steps, max_calls, max_alloc, max_stack;
} mulib_termscript_limits_t;

/* Host receives a length-delimited word. Return 1 handled, 0 unrecognized
 * (leave VM unchanged), or -1 on failure. Use mulib_termscript_fail with a
 * static diagnostic. Host may manipulate the stack or call eval, but must
 * not clear or reinitialize an active VM. No host is installed by default.
 * The core never reads the environment, files, or a terminal or launches
 * a process. Backticks and [[name]] produce inert COMMAND and ESCAPE
 * values. The escape's text is the symbolic name, never output bytes.
 *
 * For import/use the top operand is the requested name; a native import
 * replaces it with VALUE_MODULE, or returns 0 to use the source loader.
 * For exec the top operand is a resolved COMMAND (standalone) or MODULE
 * (with a COMMAND directly below). A module quote is evaluated first in
 * an isolated stack region and must produce exactly one MODULE.
 * For emit, peek at the ESCAPE on top and optional parameter TUPLE
 * below it, validate them, and pop operands only when handled. */
typedef int (*mulib_termscript_host_fn)(mulib_termscript_vm_t *, void *,
                                       const char *, size_t, mulib_termscript_error_t *);
/* out is initialized and empty. Return 1 with an owned parsed program or
 * 0 with an optional diagnostic. Source/nodes must be free()-compatible.
 * The VM caches loaded programs by name and frees them on clear (also
 * frees out on callback failure). Initialization runs at most once in a
 * private namespace and isolated stack region; reads fall back to global
 * bindings and rebinding is local. Source module scalar symbols are accessible
 * as `.name at`; quote symbols can be dispatched by backtick name + exec.
 * The name is available to native callbacks as VALUE_MODULE.text/length.
 * Loader work and memory used before returning are outside VM limits. */
typedef int (*mulib_termscript_load_fn)(void *, const char *, size_t,
                                       mulib_termscript_program_t *, mulib_termscript_error_t *);
struct mulib_termscript_vm {
  struct mulib_termscript_vm_state *state;
  mulib_termscript_limits_t limits;
  mulib_termscript_host_fn host;
  void *userdata;
  mulib_termscript_load_fn load;
  void *load_userdata;
};

/* Initialize a fresh VM; clear before reinitializing. Clear is NULL-safe
 * and idempotent. A VM must not be used concurrently. */
void mulib_termscript_vm_init(mulib_termscript_vm_t *, const mulib_termscript_limits_t *);
void mulib_termscript_vm_clear(mulib_termscript_vm_t *);
void mulib_termscript_vm_set_host(mulib_termscript_vm_t *, mulib_termscript_host_fn, void *);
void mulib_termscript_vm_set_loader(mulib_termscript_vm_t *, mulib_termscript_load_fn, void *);

/* TERM is supplied by the embedding, copied, and available as $TERM
 * (empty string by default). Returns 1 on success, 0 on failure. */
int mulib_termscript_vm_set_term(mulib_termscript_vm_t *, const char *, size_t);
const char *mulib_termscript_vm_term(const mulib_termscript_vm_t *, size_t *);
/* Mode names are copied; use the empty name for the global mode. The
 * borrowed current mode remains valid until VM clear. */
int mulib_termscript_vm_set_mode(mulib_termscript_vm_t *, const char *, size_t);
const char *mulib_termscript_vm_mode(const mulib_termscript_vm_t *, size_t *);

/* Programs, their node arrays, and source buffers are borrowed and must
 * remain immutable and alive until VM clear. Values and definitions can
 * retain AST pointers after run/eval returns. Status: 1 success, 0 failure.
 * A top-level run resets the step counter, not the stack or bindings.
 * Eval inside a host callback shares limits with its caller (e.g. import).
 * A failed run retains earlier successfully committed effects. The VM
 * checks AST node indexes and spans before reading them; the embedding
 * must supply valid buffers of the advertised lengths. */
int mulib_termscript_run(mulib_termscript_vm_t *, const mulib_termscript_program_t *,
                         mulib_termscript_error_t *);
int mulib_termscript_eval(mulib_termscript_vm_t *, const mulib_termscript_program_t *,
                          mulib_termscript_error_t *);
int mulib_termscript_step(mulib_termscript_vm_t *, mulib_termscript_error_t *);
int mulib_termscript_fail(mulib_termscript_vm_t *, mulib_termscript_error_t *, const char *);

/* Stack depth 0 is the top. The peek pointer is invalidated by stack
 * mutation; copies of values and their payloads are valid until VM clear.
 * Push copies the value, not its payload: use only values from this VM or
 * use push_text to copy foreign text. Push/pop return 1 on success.
 * Resolve reads a typed symbol once; other values are returned unchanged.
 * Collections hold values as pushed, including unresolved symbols. Arrays
 * and records are mutable and may alias; tuples are immutable. Sealing a
 * binding does not deep-freeze any aggregate value it references. */
size_t mulib_termscript_stack_size(const mulib_termscript_vm_t *);
const mulib_termscript_value_t *mulib_termscript_peek(const mulib_termscript_vm_t *, size_t);
int mulib_termscript_pop(mulib_termscript_vm_t *, mulib_termscript_value_t *);
int mulib_termscript_push(mulib_termscript_vm_t *, const mulib_termscript_value_t *);
int mulib_termscript_push_int(mulib_termscript_vm_t *, int64_t);
int mulib_termscript_push_bool(mulib_termscript_vm_t *, int);
int mulib_termscript_push_char(mulib_termscript_vm_t *, uint32_t);
int mulib_termscript_push_text(mulib_termscript_vm_t *, mulib_termscript_value_kind_t,
                               const char *, size_t);
int mulib_termscript_push_string(mulib_termscript_vm_t *, const char *, size_t);
int mulib_termscript_resolve(mulib_termscript_vm_t *, const mulib_termscript_value_t *,
                             mulib_termscript_value_t *);
size_t mulib_termscript_value_count(const mulib_termscript_value_t *);
const mulib_termscript_value_t *mulib_termscript_value_at(const mulib_termscript_value_t *,
                                                          size_t, const char **, size_t *);

/* Dispatch a key in the current state, falling back to global bindings.
 * Returns 1 when handled, 0 when unbound, -1 when execution fails. An external
 * key event starts its own step budget; script `!name press` dispatches
 * under the active budget without generating input. Key names are exact. */
int mulib_termscript_key(mulib_termscript_vm_t *, const char *, size_t, mulib_termscript_error_t *);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSCRIPT_RUNTIME) && !defined(MULIB_TERMSCRIPT_RUNTIME_IMPLEMENTED)
#define MULIB_TERMSCRIPT_RUNTIME_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_tsrt_alloc {
  struct mulib_tsrt_alloc *next;
  max_align_t align;
} mulib_tsrt_alloc_t;

typedef struct {
  const char *name;
  size_t length;
  mulib_termscript_value_t value;
} mulib_tsrt_item_t;

#ifdef __cplusplus
#define MULIB_TSRT_ALIGNAS(T) alignas(T)
#else
#define MULIB_TSRT_ALIGNAS(T) _Alignas(T)
#endif
typedef struct mulib_tsrt_frame {
  MULIB_TSRT_ALIGNAS(mulib_tsrt_item_t) struct mulib_tsrt_frame *capture;
  const struct mulib_tsrt_frame *parent;
  size_t count;
} mulib_tsrt_frame_t;
#undef MULIB_TSRT_ALIGNAS

struct mulib_tsrt_module;
struct mulib_termscript_object {
  size_t count, capacity;
  mulib_tsrt_item_t *items;
  const mulib_tsrt_frame_t *frame;
  const mulib_termscript_object_t *params;
  struct mulib_tsrt_module *scope;
  mulib_termscript_value_t parts[2];
  int composed;
};

typedef struct mulib_tsrt_binding {
  struct mulib_tsrt_binding *next;
  const struct mulib_tsrt_module *scope;
  mulib_termscript_value_t ref, value;
  int sealed;
} mulib_tsrt_binding_t;

typedef struct mulib_tsrt_module {
  struct mulib_tsrt_module *next;
  const char *name;
  size_t length;
  mulib_termscript_program_t program;
  int status; /* 0 loading, 1 ready, -1 execution failed */
} mulib_tsrt_module_t;

typedef struct mulib_tsrt_key {
  struct mulib_tsrt_key *next;
  const char *mode, *name;
  size_t mode_length, length;
  mulib_termscript_value_t quote;
} mulib_tsrt_key_t;

struct mulib_termscript_vm_state {
  mulib_tsrt_alloc_t *alloc;
  size_t used, steps, calls, offset, floor;
  int active;
  mulib_termscript_value_t *stack;
  size_t count, cap;
  mulib_tsrt_binding_t *bindings;
  mulib_tsrt_module_t *modules;
  mulib_tsrt_module_t *scope;
  mulib_tsrt_key_t *keys;
  mulib_tsrt_frame_t *frame;
  const char *term, *mode;
  size_t term_length, mode_length;
};

void
mulib_termscript_vm_init(mulib_termscript_vm_t *vm, const mulib_termscript_limits_t *limits)
{
  if (!vm) return;
  mulib_termscript_limits_t lim = {0, 0, 0, 0};
  if (limits) lim = *limits;
  if (!lim.max_steps) lim.max_steps = 100000;
  if (!lim.max_calls) lim.max_calls = 64;
  if (lim.max_calls > 128) lim.max_calls = 128;
  if (!lim.max_alloc) lim.max_alloc = 4u * 1024u * 1024u;
  if (!lim.max_stack) lim.max_stack = 4096;
  memset(vm, 0, sizeof *vm);
  vm->limits = lim;
}

static void
mulib_tsrt_program_free(mulib_termscript_program_t *p)
{
  free(p->source);
  free(p->nodes);
}

void
mulib_termscript_vm_clear(mulib_termscript_vm_t *vm)
{
  if (!vm || !vm->state) return;
  struct mulib_termscript_vm_state *s = vm->state;
  for (mulib_tsrt_module_t *m = s->modules; m; m = m->next)
    mulib_tsrt_program_free(&m->program);
  while (s->alloc) {
    mulib_tsrt_alloc_t *a = s->alloc;
    s->alloc = a->next;
    free(a);
  }
  free(s->stack);
  free(s);
  vm->state = NULL;
}

void
mulib_termscript_vm_set_host(mulib_termscript_vm_t *vm, mulib_termscript_host_fn host, void *user)
{
  if (vm) { vm->host = host; vm->userdata = user; }
}

void
mulib_termscript_vm_set_loader(mulib_termscript_vm_t *vm, mulib_termscript_load_fn load, void *user)
{
  if (vm) { vm->load = load; vm->load_userdata = user; }
}

static struct mulib_termscript_vm_state *
mulib_tsrt_state(mulib_termscript_vm_t *vm)
{
  if (!vm || vm->limits.max_alloc < sizeof *vm->state) return NULL;
  if (!vm->state) {
    vm->state = (struct mulib_termscript_vm_state *)calloc(1, sizeof *vm->state);
    if (vm->state) vm->state->used = sizeof *vm->state;
  }
  return vm->state;
}

static int
mulib_tsrt_budget(const mulib_termscript_vm_t *vm, size_t n)
{
  return vm->state->used <= vm->limits.max_alloc
    && n <= vm->limits.max_alloc - vm->state->used;
}

static void *
mulib_tsrt_alloc(mulib_termscript_vm_t *vm, size_t n)
{
  if (n > SIZE_MAX - sizeof(mulib_tsrt_alloc_t)) return NULL;
  n += sizeof(mulib_tsrt_alloc_t);
  if (!mulib_tsrt_state(vm) || !mulib_tsrt_budget(vm, n)) return NULL;
  mulib_tsrt_alloc_t *a = (mulib_tsrt_alloc_t *)calloc(1, n);
  if (!a) return NULL;
  a->next = vm->state->alloc;
  vm->state->alloc = a;
  vm->state->used += n;
  return a + 1;
}

static char *
mulib_tsrt_copy(mulib_termscript_vm_t *vm, const char *text, size_t n)
{
  if ((!text && n) || n == SIZE_MAX) return NULL;
  char *copy = (char *)mulib_tsrt_alloc(vm, n + 1);
  if (copy && n) memcpy(copy, text, n);
  return copy;
}

static int
mulib_tsrt_same(const char *a, size_t na, const char *b, size_t nb)
{
  return na == nb && (!na || (a && b && memcmp(a, b, na) == 0));
}

static int
mulib_tsrt_is(const char *a, size_t na, const char *b)
{
  return mulib_tsrt_same(a, na, b, strlen(b));
}

int
mulib_termscript_vm_set_term(mulib_termscript_vm_t *vm, const char *text, size_t n)
{
  struct mulib_termscript_vm_state *s = mulib_tsrt_state(vm);
  if (!s || (!text && n)) return 0;
  if (mulib_tsrt_same(s->term, s->term_length, text, n)) return 1;
  const char *copy = n ? mulib_tsrt_copy(vm, text, n) : "";
  if (!copy) return 0;
  s->term = copy;
  s->term_length = n;
  return 1;
}

const char *
mulib_termscript_vm_term(const mulib_termscript_vm_t *vm, size_t *length)
{
  const struct mulib_termscript_vm_state *s = vm ? vm->state : NULL;
  if (length) *length = s ? s->term_length : 0;
  return s && s->term ? s->term : "";
}

int
mulib_termscript_vm_set_mode(mulib_termscript_vm_t *vm, const char *text, size_t n)
{
  struct mulib_termscript_vm_state *s = mulib_tsrt_state(vm);
  if (!s || (!text && n)) return 0;
  if (mulib_tsrt_same(s->mode, s->mode_length, text, n)) return 1;
  const char *copy = n ? mulib_tsrt_copy(vm, text, n) : "";
  if (!copy) return 0;
  s->mode = copy;
  s->mode_length = n;
  return 1;
}

const char *
mulib_termscript_vm_mode(const mulib_termscript_vm_t *vm, size_t *length)
{
  const struct mulib_termscript_vm_state *s = vm ? vm->state : NULL;
  if (length) *length = s ? s->mode_length : 0;
  return s && s->mode ? s->mode : "";
}

int
mulib_termscript_fail(mulib_termscript_vm_t *vm, mulib_termscript_error_t *err, const char *msg)
{
  if (err) {
    err->offset = vm && vm->state ? vm->state->offset : 0;
    err->message = msg;
  }
  return 0;
}

int
mulib_termscript_step(mulib_termscript_vm_t *vm, mulib_termscript_error_t *err)
{
  struct mulib_termscript_vm_state *s = mulib_tsrt_state(vm);
  if (!s) return mulib_termscript_fail(vm, err, "allocation limit or out of memory");
  if (s->steps >= vm->limits.max_steps)
    return mulib_termscript_fail(vm, err, "step limit exceeded");
  ++s->steps;
  return 1;
}

static int
mulib_tsrt_nomem(mulib_termscript_vm_t *vm, mulib_termscript_error_t *err)
{
  return mulib_termscript_fail(vm, err, "allocation limit or out of memory");
}

static int
mulib_tsrt_reserve(mulib_termscript_vm_t *vm, size_t extra)
{
  struct mulib_termscript_vm_state *s = mulib_tsrt_state(vm);
  if (!s || s->count > vm->limits.max_stack || extra > vm->limits.max_stack - s->count)
    return 0;
  size_t need = s->count + extra;
  if (need <= s->cap) return 1;
  size_t slot = sizeof *s->stack;
  if (need > SIZE_MAX / slot) return 0;
  size_t cap = s->cap && s->cap <= SIZE_MAX / slot / 2 ? s->cap * 2 : 8;
  if (cap > vm->limits.max_stack) cap = vm->limits.max_stack;
  if (cap < need || !mulib_tsrt_budget(vm, (cap - s->cap) * slot)) cap = need;
  size_t bytes = (cap - s->cap) * slot;
  if (!mulib_tsrt_budget(vm, bytes)) return 0;
  void *stack = realloc(s->stack, cap * slot);
  if (!stack) return 0;
  s->stack = (mulib_termscript_value_t *)stack;
  s->used += bytes;
  s->cap = cap;
  return 1;
}

static mulib_termscript_value_t
mulib_tsrt_value(mulib_termscript_value_kind_t kind, int64_t n, const char *p, size_t len)
{
  mulib_termscript_value_t v;
  memset(&v, 0, sizeof v);
  v.kind = kind;
  v.integer = n;
  v.text = p;
  v.length = len;
  v.node = SIZE_MAX;
  return v;
}

size_t
mulib_termscript_stack_size(const mulib_termscript_vm_t *vm)
{
  return vm && vm->state ? vm->state->count - vm->state->floor : 0;
}

const mulib_termscript_value_t *
mulib_termscript_peek(const mulib_termscript_vm_t *vm, size_t depth)
{
  return depth < mulib_termscript_stack_size(vm)
    ? &vm->state->stack[vm->state->count - 1 - depth] : NULL;
}

int
mulib_termscript_pop(mulib_termscript_vm_t *vm, mulib_termscript_value_t *out)
{
  const mulib_termscript_value_t *v = mulib_termscript_peek(vm, 0);
  if (!v) return 0;
  if (out) *out = *v;
  --vm->state->count;
  return 1;
}

int
mulib_termscript_push(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *value)
{
  if (!value) return 0;
  mulib_termscript_value_t copy = *value; /* value can point into the stack */
  if (!mulib_tsrt_reserve(vm, 1)) return 0;
  vm->state->stack[vm->state->count++] = copy;
  return 1;
}

int
mulib_termscript_push_int(mulib_termscript_vm_t *vm, int64_t n)
{
  mulib_termscript_value_t v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_INTEGER, n, NULL, 0);
  return mulib_termscript_push(vm, &v);
}

int
mulib_termscript_push_bool(mulib_termscript_vm_t *vm, int b)
{
  mulib_termscript_value_t v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_BOOL, !!b, NULL, 0);
  return mulib_termscript_push(vm, &v);
}

int
mulib_termscript_push_text(mulib_termscript_vm_t *vm, mulib_termscript_value_kind_t kind,
                          const char *text, size_t n)
{
  if (kind < MULIB_TERMSCRIPT_VALUE_STRING || (kind > MULIB_TERMSCRIPT_VALUE_FIELD
                                               && kind != MULIB_TERMSCRIPT_VALUE_MODULE
                                               && kind != MULIB_TERMSCRIPT_VALUE_ESCAPE)
      || kind == MULIB_TERMSCRIPT_VALUE_CHARACTER)
    return 0;
  if (!mulib_tsrt_reserve(vm, 1)) return 0;
  const char *copy = mulib_tsrt_copy(vm, text, n);
  if (!copy) return 0;
  mulib_termscript_value_t v = mulib_tsrt_value(kind, 0, copy, n);
  return mulib_termscript_push(vm, &v);
}

int
mulib_termscript_push_string(mulib_termscript_vm_t *vm, const char *s, size_t n)
{
  return mulib_termscript_push_text(vm, MULIB_TERMSCRIPT_VALUE_STRING, s, n);
}

static int
mulib_tsrt_char(mulib_termscript_vm_t *vm, uint32_t scalar, mulib_termscript_value_t *out)
{
  unsigned char bytes[4];
  size_t n;
  if (scalar <= 0x7f) {
    bytes[0] = (unsigned char)scalar;
    n = 1;
  } else {
    if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) return 0;
    n = scalar <= 0x7ff ? 2 : scalar <= 0xffff ? 3 : 4;
    uint32_t x = scalar;
    for (size_t i = n - 1; i; --i) {
      bytes[i] = (unsigned char)(0x80 | (x & 0x3f));
      x >>= 6;
    }
    bytes[0] = (unsigned char)((n == 2 ? 0xc0 : n == 3 ? 0xe0 : 0xf0) | x);
  }
  const char *text = mulib_tsrt_copy(vm, (const char *)bytes, n);
  if (!text) return 0;
  *out = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_CHARACTER, scalar, text, n);
  return 1;
}

int
mulib_termscript_push_char(mulib_termscript_vm_t *vm, uint32_t scalar)
{
  mulib_termscript_value_t v;
  return mulib_tsrt_reserve(vm, 1) && mulib_tsrt_char(vm, scalar, &v)
    && mulib_termscript_push(vm, &v);
}

static int
mulib_tsrt_ref(mulib_termscript_value_kind_t kind)
{
  return kind >= MULIB_TERMSCRIPT_VALUE_SYMBOL && kind <= MULIB_TERMSCRIPT_VALUE_RECORD_SYMBOL;
}

static mulib_tsrt_binding_t *
mulib_tsrt_lookup(const mulib_termscript_vm_t *vm, const mulib_termscript_value_t *ref,
                  const mulib_tsrt_module_t *scope)
{
  for (mulib_tsrt_binding_t *b = vm->state->bindings; b; b = b->next)
    if (b->scope == scope && b->ref.kind == ref->kind
        && mulib_tsrt_same(b->ref.text, b->ref.length, ref->text, ref->length)) return b;
  return NULL;
}

static mulib_tsrt_binding_t *
mulib_tsrt_binding(const mulib_termscript_vm_t *vm, const mulib_termscript_value_t *ref)
{
  mulib_tsrt_binding_t *b = mulib_tsrt_lookup(vm, ref, vm->state->scope);
  return !b && vm->state->scope ? mulib_tsrt_lookup(vm, ref, NULL) : b;
}

int
mulib_termscript_resolve(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *in,
                        mulib_termscript_value_t *out)
{
  if (!vm || !in || !out) return 0;
  if (!mulib_tsrt_ref(in->kind)) { *out = *in; return 1; }
  mulib_tsrt_binding_t *b = vm->state ? mulib_tsrt_binding(vm, in) : NULL;
  if (!b) return 0;
  *out = b->value;
  return 1;
}

size_t
mulib_termscript_value_count(const mulib_termscript_value_t *v)
{
  return v && v->object && v->kind >= MULIB_TERMSCRIPT_VALUE_ARRAY
    && v->kind <= MULIB_TERMSCRIPT_VALUE_RECORD ? v->object->count : 0;
}

const mulib_termscript_value_t *
mulib_termscript_value_at(const mulib_termscript_value_t *v, size_t i,
                         const char **name, size_t *length)
{
  if (name) *name = NULL;
  if (length) *length = 0;
  if (i >= mulib_termscript_value_count(v)) return NULL;
  const mulib_tsrt_item_t *item = &v->object->items[i];
  if (name) *name = item->name;
  if (length) *length = item->length;
  return &item->value;
}

static const mulib_termscript_node_t *
mulib_tsrt_at(const mulib_termscript_program_t *p, size_t index)
{
  if (!p || !p->nodes || !p->source || index >= p->count) return NULL;
  const mulib_termscript_node_t *n = &p->nodes[index];
  return n->offset <= p->length && n->length <= p->length - n->offset ? n : NULL;
}

static int
mulib_tsrt_word(const mulib_termscript_program_t *p, size_t index, const char *word)
{
  const mulib_termscript_node_t *n = mulib_tsrt_at(p, index);
  return n && n->kind == MULIB_TERMSCRIPT_WORD
    && mulib_tsrt_is(p->source + n->offset, n->length, word);
}

static int
mulib_tsrt_enter(mulib_termscript_vm_t *vm, mulib_termscript_error_t *err)
{
  if (vm->state->calls >= vm->limits.max_calls)
    return mulib_termscript_fail(vm, err, "call limit exceeded");
  if (!mulib_termscript_step(vm, err)) return 0;
  ++vm->state->calls;
  return 1;
}

static int mulib_tsrt_seq(mulib_termscript_vm_t *, const mulib_termscript_program_t *,
                          size_t, mulib_termscript_error_t *);
static int mulib_tsrt_node(mulib_termscript_vm_t *, const mulib_termscript_program_t *,
                           size_t, mulib_termscript_error_t *);
static int mulib_tsrt_invoke(mulib_termscript_vm_t *, const mulib_termscript_value_t *,
                             mulib_termscript_error_t *);

/* Capture parameters by value only if a quote escapes the active frame.
 * Ancestors are already persistent captures; the invocation itself can
 * release its temporary parameter frame. */
static const mulib_tsrt_frame_t *
mulib_tsrt_capture(mulib_termscript_vm_t *vm, mulib_tsrt_frame_t *src)
{
  if (src->capture) return src->capture;
  size_t len = sizeof *src + src->count * sizeof(mulib_tsrt_item_t);
  mulib_tsrt_frame_t *copy = (mulib_tsrt_frame_t *)mulib_tsrt_alloc(vm, len);
  if (!copy) return NULL;
  memcpy(copy, src, len);
  copy->capture = copy;
  src->capture = copy;
  return copy;
}

static int
mulib_tsrt_quote(mulib_termscript_vm_t *vm, const mulib_termscript_program_t *p, size_t i,
                mulib_termscript_value_t *out, mulib_termscript_error_t *err)
{
  *out = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_QUOTE, 0, NULL, 0);
  out->program = p;
  out->node = i;
  if (vm->state->frame || vm->state->scope) {
    mulib_termscript_object_t *obj = (mulib_termscript_object_t *)mulib_tsrt_alloc(vm, sizeof *obj);
    if (!obj) return mulib_tsrt_nomem(vm, err);
    if (vm->state->frame) {
      obj->frame = mulib_tsrt_capture(vm, vm->state->frame);
      if (!obj->frame) return mulib_tsrt_nomem(vm, err);
    }
    obj->scope = vm->state->scope;
    out->object = obj;
  }
  return 1;
}

static int
mulib_tsrt_push(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *v,
               mulib_termscript_error_t *err)
{
  return mulib_termscript_push(vm, v) || mulib_termscript_fail(vm, err, "stack or allocation limit");
}

static int
mulib_tsrt_store(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *ref,
                const mulib_termscript_value_t *value, mulib_termscript_error_t *err)
{
  if (!mulib_tsrt_ref(ref->kind))
    return mulib_termscript_fail(vm, err, "expected a symbol");
  if ((ref->kind == MULIB_TERMSCRIPT_VALUE_ARRAY_SYMBOL && value->kind != MULIB_TERMSCRIPT_VALUE_ARRAY)
      || (ref->kind == MULIB_TERMSCRIPT_VALUE_RECORD_SYMBOL && value->kind != MULIB_TERMSCRIPT_VALUE_RECORD))
    return mulib_termscript_fail(vm, err, "wrong value type for symbol");
  mulib_tsrt_binding_t *b = mulib_tsrt_lookup(vm, ref, vm->state->scope);
  if (b && b->sealed) return mulib_termscript_fail(vm, err, "sealed symbol");
  if (!b) {
    b = (mulib_tsrt_binding_t *)mulib_tsrt_alloc(vm, sizeof *b);
    if (!b) return mulib_tsrt_nomem(vm, err);
    b->ref = *ref;
    b->scope = vm->state->scope;
    b->next = vm->state->bindings;
    vm->state->bindings = b;
  }
  b->value = *value;
  return 1;
}

static int
mulib_tsrt_invoke(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *quote,
                  mulib_termscript_error_t *err)
{
  mulib_termscript_value_t q = *quote;
  if (q.kind != MULIB_TERMSCRIPT_VALUE_QUOTE)
    return mulib_termscript_fail(vm, err, "expected a quote");
  if (!mulib_tsrt_enter(vm, err)) return 0;
  struct mulib_termscript_vm_state *s = vm->state;
  mulib_tsrt_frame_t *outer = s->frame, *frame = NULL;
  mulib_tsrt_module_t *scope = s->scope;
  size_t bytes = 0;
  int ok = 0;
  if (q.object && q.object->composed) {
    ok = mulib_tsrt_invoke(vm, &q.object->parts[0], err)
      && mulib_tsrt_invoke(vm, &q.object->parts[1], err);
  } else {
    const mulib_termscript_node_t *n = mulib_tsrt_at(q.program, q.node);
    if (!n || n->kind != MULIB_TERMSCRIPT_BLOCK) {
      mulib_termscript_fail(vm, err, "invalid quote AST");
      goto done;
    }
    const mulib_tsrt_frame_t *capture = q.object ? q.object->frame : NULL;
    const mulib_termscript_object_t *params = q.object ? q.object->params : NULL;
    if (params && params->count) {
      size_t count = params->count;
      if (count > mulib_termscript_stack_size(vm)) {
        mulib_termscript_fail(vm, err, "too few arguments for quote");
        goto done;
      }
      if (count > (SIZE_MAX - sizeof *frame) / sizeof(mulib_tsrt_item_t)) {
        mulib_tsrt_nomem(vm, err);
        goto done;
      }
      bytes = sizeof *frame + count * sizeof(mulib_tsrt_item_t);
      if (!mulib_tsrt_budget(vm, bytes) || !(frame = (mulib_tsrt_frame_t *)malloc(bytes))) {
        mulib_tsrt_nomem(vm, err);
        goto done;
      }
      s->used += bytes;
      frame->capture = NULL;
      frame->parent = capture;
      frame->count = count;
      mulib_tsrt_item_t *args = (mulib_tsrt_item_t *)(frame + 1);
      for (size_t i = 0; i < count; ++i) {
        args[i] = params->items[i];
        if (!mulib_termscript_resolve(vm, &s->stack[s->count - count + i], &args[i].value)) {
          mulib_termscript_fail(vm, err, "unbound argument");
          goto done;
        }
      }
      s->count -= count;
      s->frame = frame;
    } else {
      /* Captures are immutable except for their memoized self pointer. */
      s->frame = (mulib_tsrt_frame_t *)capture;
    }
    s->scope = q.object ? q.object->scope : NULL;
    ok = mulib_tsrt_seq(vm, q.program, n->first, err);
  }
done:
  s->frame = outer;
  s->scope = scope;
  if (frame) { free(frame); s->used -= bytes; }
  --s->calls;
  return ok;
}

static int
mulib_tsrt_container(mulib_termscript_vm_t *vm, const mulib_termscript_program_t *p,
                     const mulib_termscript_node_t *n, int params, mulib_termscript_error_t *err)
{
  if (!mulib_tsrt_enter(vm, err)) return 0;
  struct mulib_termscript_vm_state *s = vm->state;
  size_t floor = s->floor, base = s->count;
  mulib_termscript_value_kind_t kind = n->kind == MULIB_TERMSCRIPT_ARRAY
    ? MULIB_TERMSCRIPT_VALUE_ARRAY : n->kind == MULIB_TERMSCRIPT_RECORD
    ? MULIB_TERMSCRIPT_VALUE_RECORD : MULIB_TERMSCRIPT_VALUE_TUPLE;
  size_t count, stride = kind == MULIB_TERMSCRIPT_VALUE_RECORD ? 2 : 1;
  mulib_termscript_object_t *obj;
  mulib_termscript_value_t result;
  s->floor = base;
  int ok = 0;
  if (params) {
    for (size_t i = n->first; i != SIZE_MAX;) {
      const mulib_termscript_node_t *param = mulib_tsrt_at(p, i);
      if (!param || param->kind != MULIB_TERMSCRIPT_PARAMETER) {
        mulib_termscript_fail(vm, err, "parameters must be $names");
        goto done;
      }
      s->offset = param->offset;
      if (!mulib_termscript_step(vm, err)) goto done;
      mulib_termscript_value_t v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_PARAMETER, 0,
                                                    p->source + param->offset, param->length);
      if (!mulib_tsrt_push(vm, &v, err)) goto done;
      i = param->next;
    }
  } else if (n->kind == MULIB_TERMSCRIPT_RECORD) {
    for (size_t i = n->first; i != SIZE_MAX;) {
      const mulib_termscript_node_t *field = mulib_tsrt_at(p, i);
      const mulib_termscript_node_t *child = field ? mulib_tsrt_at(p, field->first) : NULL;
      if (!field || field->kind != MULIB_TERMSCRIPT_FIELD || !child || child->next != SIZE_MAX) {
        mulib_termscript_fail(vm, err, "invalid record field AST");
        goto done;
      }
      mulib_termscript_value_t v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_FIELD, 0,
                                                    p->source + field->offset, field->length);
      s->offset = field->offset;
      if (!mulib_termscript_step(vm, err) || !mulib_tsrt_push(vm, &v, err)) goto done;
      s->floor = s->count;
      if (!mulib_tsrt_node(vm, p, field->first, err)) goto done;
      if (s->count - s->floor != 1) {
        mulib_termscript_fail(vm, err, "field must produce one value");
        goto done;
      }
      s->floor = base;
      i = field->next;
    }
  } else if (!mulib_tsrt_seq(vm, p, n->first, err)) {
    goto done;
  }
  count = s->count - base;
  if (count % stride) {
    mulib_termscript_fail(vm, err, "record requires field/value pairs");
    goto done;
  }
  count /= stride;
  if (count > (SIZE_MAX - sizeof(mulib_termscript_object_t)) / sizeof(mulib_tsrt_item_t)) {
    mulib_tsrt_nomem(vm, err);
    goto done;
  }
  obj = (mulib_termscript_object_t *)mulib_tsrt_alloc(vm, sizeof *obj + count * sizeof(mulib_tsrt_item_t));
  if (!obj) { mulib_tsrt_nomem(vm, err); goto done; }
  obj->items = (mulib_tsrt_item_t *)(obj + 1);
  obj->count = count;
  obj->capacity = count;
  for (size_t i = 0; i < count; ++i) {
    mulib_tsrt_item_t *item = &obj->items[i];
    if (stride == 2) {
      mulib_termscript_value_t *field = &s->stack[base + 2 * i];
      if (field->kind != MULIB_TERMSCRIPT_VALUE_FIELD && field->kind != MULIB_TERMSCRIPT_VALUE_STRING) {
        mulib_termscript_fail(vm, err, "expected a field name");
        goto done;
      }
      item->name = field->text;
      item->length = field->length;
      for (size_t j = 0; j < i; ++j) {
        if (!mulib_termscript_step(vm, err)) goto done;
        if (mulib_tsrt_same(item->name, item->length, obj->items[j].name, obj->items[j].length)) {
          mulib_termscript_fail(vm, err, "duplicate record field");
          goto done;
        }
      }
    }
    item->value = s->stack[base + stride * i + stride - 1];
  }
  s->count = base;
  result = mulib_tsrt_value(kind, 0, NULL, 0);
  result.object = obj;
  ok = mulib_tsrt_push(vm, &result, err);
done:
  if (!ok) s->count = base;
  s->floor = floor;
  --s->calls;
  return ok;
}

static int
mulib_tsrt_lambda(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *params,
                  mulib_termscript_value_t *quote, mulib_termscript_error_t *err)
{
  if (params->kind != MULIB_TERMSCRIPT_VALUE_TUPLE || quote->kind != MULIB_TERMSCRIPT_VALUE_QUOTE
      || (quote->object && (quote->object->composed || quote->object->params)))
    return mulib_termscript_fail(vm, err, "expected a parameter tuple and a block");
  size_t count = mulib_termscript_value_count(params);
  for (size_t i = 0; i < count; ++i) {
    const mulib_termscript_value_t *arg = &params->object->items[i].value;
    if (arg->kind != MULIB_TERMSCRIPT_VALUE_PARAMETER || !arg->length)
      return mulib_termscript_fail(vm, err, "parameters must be $names");
    for (size_t j = 0; j < i; ++j) {
      const mulib_termscript_value_t *prev = &params->object->items[j].value;
      if (!mulib_termscript_step(vm, err)) return 0;
      if (mulib_tsrt_same(prev->text, prev->length, arg->text, arg->length))
        return mulib_termscript_fail(vm, err, "duplicate parameter");
    }
    /* The parameter tuple remains visible to callers and isn't mutated. */
  }
  if (count > (SIZE_MAX - sizeof(mulib_termscript_object_t)) / sizeof(mulib_tsrt_item_t))
    return mulib_tsrt_nomem(vm, err);
  mulib_termscript_object_t *names = (mulib_termscript_object_t *)
    mulib_tsrt_alloc(vm, sizeof *names + count * sizeof(mulib_tsrt_item_t));
  mulib_termscript_object_t *obj = (mulib_termscript_object_t *)mulib_tsrt_alloc(vm, sizeof *obj);
  if (!names || !obj) return mulib_tsrt_nomem(vm, err);
  names->count = count;
  names->items = (mulib_tsrt_item_t *)(names + 1);
  for (size_t i = 0; i < count; ++i) {
    names->items[i].name = params->object->items[i].value.text;
    names->items[i].length = params->object->items[i].value.length;
  }
  obj->frame = quote->object ? quote->object->frame : NULL;
  obj->scope = quote->object ? quote->object->scope : NULL;
  obj->params = names;
  quote->object = obj;
  return 1;
}

enum mulib_tsrt_op {
  MULIB_TSRT_ADD, MULIB_TSRT_SUB, MULIB_TSRT_MUL, MULIB_TSRT_DIV, MULIB_TSRT_MOD,
  MULIB_TSRT_EQ, MULIB_TSRT_NE, MULIB_TSRT_LT, MULIB_TSRT_LE, MULIB_TSRT_GT, MULIB_TSRT_GE
};

static int
mulib_tsrt_calculate(enum mulib_tsrt_op op, int64_t left, int64_t right, int64_t *out)
{
  switch (op) {
  case MULIB_TSRT_ADD:
    if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right))
      return 0;
    *out = left + right; return 1;
  case MULIB_TSRT_SUB:
    if ((right < 0 && left > INT64_MAX + right) || (right > 0 && left < INT64_MIN + right))
      return 0;
    *out = left - right; return 1;
  case MULIB_TSRT_MUL:
    if (left > 0) {
      if ((right > 0 && left > INT64_MAX / right) || (right < 0 && right < INT64_MIN / left))
        return 0;
    } else if (left < 0) {
      if ((right > 0 && left < INT64_MIN / right) || (right < 0 && left < INT64_MAX / right))
        return 0;
    }
    *out = left * right; return 1;
  case MULIB_TSRT_DIV: case MULIB_TSRT_MOD:
    if (right == 0 || (left == INT64_MIN && right == -1)) return 0;
    *out = op == MULIB_TSRT_MOD ? left % right : left / right; return 1;
  case MULIB_TSRT_EQ: *out = left == right; return 1;
  case MULIB_TSRT_NE: *out = left != right; return 1;
  case MULIB_TSRT_LT: *out = left < right; return 1;
  case MULIB_TSRT_LE: *out = left <= right; return 1;
  case MULIB_TSRT_GT: *out = left > right; return 1;
  case MULIB_TSRT_GE: *out = left >= right; return 1;
  }
  return 0;
}

static int
mulib_tsrt_binary(mulib_termscript_vm_t *vm, enum mulib_tsrt_op op, mulib_termscript_error_t *err)
{
  const mulib_termscript_value_t *a = mulib_termscript_peek(vm, 1), *b = mulib_termscript_peek(vm, 0);
  if (!a) return mulib_termscript_fail(vm, err, "stack underflow");
  mulib_termscript_value_t left, right;
  if (!mulib_termscript_resolve(vm, a, &left) || !mulib_termscript_resolve(vm, b, &right))
    return mulib_termscript_fail(vm, err, "unbound symbol");
  int64_t result;
  if (left.kind == MULIB_TERMSCRIPT_VALUE_INTEGER && right.kind == MULIB_TERMSCRIPT_VALUE_INTEGER) {
    if (!mulib_tsrt_calculate(op, left.integer, right.integer, &result))
      return mulib_termscript_fail(vm, err,
        (op == MULIB_TSRT_DIV || op == MULIB_TSRT_MOD) && right.integer == 0
          ? "division by zero" : "integer overflow");
  } else if (op >= MULIB_TSRT_EQ) {
    int same = left.kind == right.kind;
    if (same && left.text && right.text && op >= MULIB_TSRT_LT
        && (left.kind == MULIB_TERMSCRIPT_VALUE_STRING || left.kind == MULIB_TERMSCRIPT_VALUE_CHARACTER)) {
      size_t min = left.length < right.length ? left.length : right.length;
      int cmp = memcmp(left.text, right.text, min);
      if (cmp == 0) cmp = left.length < right.length ? -1 : left.length != right.length;
      if (!mulib_tsrt_calculate(op, cmp, 0, &result)) return 0;
    } else if (op == MULIB_TSRT_EQ || op == MULIB_TSRT_NE) {
      same = same && left.integer == right.integer && left.object == right.object
        && mulib_tsrt_same(left.text, left.length, right.text, right.length);
      if (same && (left.kind == MULIB_TERMSCRIPT_VALUE_NODE || left.kind == MULIB_TERMSCRIPT_VALUE_QUOTE))
        same = left.program == right.program && left.node == right.node;
      result = op == MULIB_TSRT_EQ ? same : !same;
    } else {
      return mulib_termscript_fail(vm, err, "incompatible comparison types");
    }
  } else {
    return mulib_termscript_fail(vm, err, "expected integers");
  }
  mulib_termscript_value_t value = mulib_tsrt_value(op >= MULIB_TSRT_EQ
    ? MULIB_TERMSCRIPT_VALUE_BOOL : MULIB_TERMSCRIPT_VALUE_INTEGER, result, NULL, 0);
  if (op < MULIB_TSRT_EQ && mulib_tsrt_ref(a->kind)) {
    if (!mulib_tsrt_store(vm, a, &value, err)) return 0;
    vm->state->count -= 2;
  } else {
    vm->state->count -= 2;
    return mulib_tsrt_push(vm, &value, err);
  }
  return 1;
}

static int
mulib_tsrt_host(mulib_termscript_vm_t *vm, const char *word, size_t n, mulib_termscript_error_t *err)
{
  if (!vm->host) return 0;
  if (!mulib_tsrt_enter(vm, err)) return -1;
  struct mulib_termscript_vm_state *s = vm->state;
  size_t offset = s->offset;
  int result = vm->host(vm, vm->userdata, word, n, err);
  s->offset = offset;
  --s->calls;
  if (result == 1 || result == 0) return result;
  if (!err || !err->message) mulib_termscript_fail(vm, err, "host operation failed");
  return -1;
}

static int
mulib_tsrt_import(mulib_termscript_vm_t *vm, const char *word, size_t n, mulib_termscript_error_t *err)
{
  int supported = mulib_tsrt_host(vm, word, n, err);
  if (supported) return supported == 1;
  if (!vm->load) return mulib_termscript_fail(vm, err, "module loading is not enabled");
  const mulib_termscript_value_t *p = mulib_termscript_peek(vm, 0);
  mulib_termscript_value_t name;
  if (!p || !mulib_termscript_resolve(vm, p, &name) || name.kind != MULIB_TERMSCRIPT_VALUE_STRING)
    return mulib_termscript_fail(vm, err, "expected a module name string");
  if (!name.length || !name.text || memchr(name.text, '\0', name.length))
    return mulib_termscript_fail(vm, err, "invalid module name");
  struct mulib_termscript_vm_state *s = vm->state;
  mulib_tsrt_module_t *m;
  for (m = s->modules; m; m = m->next)
    if (mulib_tsrt_same(name.text, name.length, m->name, m->length)) break;
  if (m && m->status != 1)
    return mulib_termscript_fail(vm, err, m->status ? "module initialization failed" : "cyclic import");
  if (!m) {
    if (!mulib_tsrt_enter(vm, err)) return 0;
    mulib_termscript_program_t program = {NULL, 0, NULL, 0, SIZE_MAX};
    int loaded = vm->load(vm->load_userdata, name.text, name.length, &program, err);
    if (loaded != 1) {
      mulib_tsrt_program_free(&program);
      --s->calls;
      if (err && err->message) return 0;
      return mulib_termscript_fail(vm, err, "unable to load module");
    }
    size_t slot = sizeof *program.nodes;
    if (program.length == SIZE_MAX || program.count > (SIZE_MAX - program.length - 1) / slot
        || !mulib_tsrt_budget(vm, program.length + 1 + program.count * slot)) {
      mulib_tsrt_program_free(&program);
      --s->calls;
      return mulib_tsrt_nomem(vm, err);
    }
    size_t bytes = program.length + 1 + program.count * slot;
    s->used += bytes;
    m = (mulib_tsrt_module_t *)mulib_tsrt_alloc(vm, sizeof *m);
    if (!m) {
      s->used -= bytes;
      mulib_tsrt_program_free(&program);
      --s->calls;
      return mulib_tsrt_nomem(vm, err);
    }
    /* 'name' already points to immutable AST or VM-owned storage. */
    m->name = name.text;
    m->length = name.length;
    m->program = program;
    m->next = s->modules;
    s->modules = m;
    size_t outer_floor = s->floor, base = s->count, offset = s->offset;
    mulib_tsrt_frame_t *outer_frame = s->frame;
    mulib_tsrt_module_t *outer_scope = s->scope;
    s->floor = base;
    s->frame = NULL;
    s->scope = m;
    m->status = mulib_tsrt_seq(vm, &m->program, program.root, err) ? 1 : -1;
    s->scope = outer_scope;
    s->frame = outer_frame;
    s->floor = outer_floor;
    s->count = base;
    s->offset = offset;
    --s->calls;
    if (m->status != 1) return 0;
  }
  s->stack[s->count - 1] = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_MODULE, 0, m->name, m->length);
  s->stack[s->count - 1].program = &m->program;
  return 1;
}

static mulib_tsrt_binding_t *
mulib_tsrt_export(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *module,
                  const char *name, size_t length)
{
  if (module->kind != MULIB_TERMSCRIPT_VALUE_MODULE || !module->program) return NULL;
  for (mulib_tsrt_module_t *m = vm->state->modules; m; m = m->next) {
    if (&m->program != module->program || m->status != 1) continue;
    mulib_termscript_value_t ref = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_SYMBOL, 0, name, length);
    return mulib_tsrt_lookup(vm, &ref, m);
  }
  return NULL;
}

static int
mulib_tsrt_exec(mulib_termscript_vm_t *vm, mulib_termscript_error_t *err)
{
  const mulib_termscript_value_t *top = mulib_termscript_peek(vm, 0);
  mulib_termscript_value_t value;
  struct mulib_termscript_vm_state *s = vm->state;
  if (!top) return mulib_termscript_fail(vm, err, "stack underflow");
  if (!mulib_termscript_resolve(vm, top, &value))
    return mulib_termscript_fail(vm, err, "unbound dispatch target");
  if (value.kind == MULIB_TERMSCRIPT_VALUE_QUOTE) {
    const mulib_termscript_value_t *below = mulib_termscript_peek(vm, 1);
    if (!below || below->kind != MULIB_TERMSCRIPT_VALUE_COMMAND)
      return mulib_termscript_fail(vm, err, "expected a command before the module quote");
    size_t floor = s->floor, base = s->count, offset = s->offset;
    mulib_termscript_value_t target = *top;
    s->floor = --s->count;
    int ok = mulib_tsrt_invoke(vm, &value, err);
    s->offset = offset;
    s->floor = floor;
    if (!ok || s->count != base || s->stack[base - 1].kind != MULIB_TERMSCRIPT_VALUE_MODULE) {
      s->stack[base - 1] = target;
      s->count = base;
      return ok ? mulib_termscript_fail(vm, err, "module quote must produce one module") : 0;
    }
    value = s->stack[base - 1];
  } else {
    s->stack[s->count - 1] = value;
  }
  if (value.kind == MULIB_TERMSCRIPT_VALUE_MODULE) {
    const mulib_termscript_value_t *command = mulib_termscript_peek(vm, 1);
    if (!command || command->kind != MULIB_TERMSCRIPT_VALUE_COMMAND)
      return mulib_termscript_fail(vm, err, "expected a command before the module");
    mulib_tsrt_binding_t *b = mulib_tsrt_export(vm, &value, command->text, command->length);
    if (b) {
      if (b->value.kind != MULIB_TERMSCRIPT_VALUE_QUOTE)
        return mulib_termscript_fail(vm, err, "module member is not a quote");
      s->count -= 2;
      return mulib_tsrt_invoke(vm, &b->value, err);
    }
  } else if (value.kind != MULIB_TERMSCRIPT_VALUE_COMMAND) {
    return mulib_termscript_fail(vm, err, "expected a command or module");
  }
  int handled = mulib_tsrt_host(vm, "exec", 4, err);
  return handled == 1 || (handled == 0
    && mulib_termscript_fail(vm, err, "command execution is not enabled for this target"));
}

static int
mulib_tsrt_element(mulib_termscript_vm_t *vm, int write, mulib_termscript_error_t *err)
{
  const mulib_termscript_value_t *a = mulib_termscript_peek(vm, write ? 2 : 1);
  const mulib_termscript_value_t *b = mulib_termscript_peek(vm, write ? 1 : 0);
  if (!a) return mulib_termscript_fail(vm, err, "stack underflow");
  mulib_termscript_value_t object, key;
  if (!mulib_termscript_resolve(vm, a, &object))
    return mulib_termscript_fail(vm, err, "unbound symbol");
  if (object.kind == MULIB_TERMSCRIPT_VALUE_RECORD && b->kind == MULIB_TERMSCRIPT_VALUE_SYMBOL) {
    key = *b;
    key.kind = MULIB_TERMSCRIPT_VALUE_FIELD;
  } else if (!mulib_termscript_resolve(vm, b, &key)) {
    return mulib_termscript_fail(vm, err, "unbound symbol");
  }
  if (object.kind == MULIB_TERMSCRIPT_VALUE_MODULE && !write) {
    if (key.kind != MULIB_TERMSCRIPT_VALUE_FIELD && key.kind != MULIB_TERMSCRIPT_VALUE_STRING)
      return mulib_termscript_fail(vm, err, "expected a module member name");
    mulib_tsrt_binding_t *exported = mulib_tsrt_export(vm, &object, key.text, key.length);
    if (!exported) return mulib_termscript_fail(vm, err, "unknown module member");
    vm->state->count -= 2;
    return mulib_tsrt_push(vm, &exported->value, err);
  }
  size_t i = 0, count = mulib_termscript_value_count(&object);
  if (object.kind == MULIB_TERMSCRIPT_VALUE_ARRAY || object.kind == MULIB_TERMSCRIPT_VALUE_TUPLE) {
    if (write && object.kind == MULIB_TERMSCRIPT_VALUE_TUPLE)
      return mulib_termscript_fail(vm, err, "tuple is immutable");
    if (key.kind != MULIB_TERMSCRIPT_VALUE_INTEGER)
      return mulib_termscript_fail(vm, err, "expected an integer index");
    if (key.integer < 0 || (uint64_t)key.integer >= count)
      return mulib_termscript_fail(vm, err, "index out of bounds");
    i = (size_t)key.integer;
  } else if (object.kind == MULIB_TERMSCRIPT_VALUE_RECORD) {
    if (key.kind != MULIB_TERMSCRIPT_VALUE_FIELD && key.kind != MULIB_TERMSCRIPT_VALUE_STRING)
      return mulib_termscript_fail(vm, err, "expected a field name");
    for (; i < count; ++i) {
      if (!mulib_termscript_step(vm, err)) return 0;
      const mulib_tsrt_item_t *item = &object.object->items[i];
      if (mulib_tsrt_same(item->name, item->length, key.text, key.length)) break;
    }
    if (i == count) return mulib_termscript_fail(vm, err, "unknown record field");
  } else {
    return mulib_termscript_fail(vm, err, "expected an array, tuple, record, or module");
  }
  if (!write) {
    vm->state->count -= 2;
    return mulib_tsrt_push(vm, &object.object->items[i].value, err);
  }
  mulib_tsrt_binding_t *binding = mulib_tsrt_ref(a->kind) ? mulib_tsrt_binding(vm, a) : NULL;
  if (binding && binding->sealed) return mulib_termscript_fail(vm, err, "sealed symbol");
  mulib_termscript_value_t value;
  if (!mulib_termscript_resolve(vm, mulib_termscript_peek(vm, 0), &value))
    return mulib_termscript_fail(vm, err, "unbound value");
  object.object->items[i].value = value;
  vm->state->count -= 3;
  return 1;
}

static int
mulib_tsrt_array_word(mulib_termscript_vm_t *vm, int push, mulib_termscript_error_t *err)
{
  const mulib_termscript_value_t *top = mulib_termscript_peek(vm, 0);
  mulib_termscript_value_t array, value;
  if (!top || !mulib_termscript_resolve(vm, top, &array) || array.kind != MULIB_TERMSCRIPT_VALUE_ARRAY
      || !array.object) return mulib_termscript_fail(vm, err, "expected an array");
  mulib_tsrt_binding_t *binding = mulib_tsrt_ref(top->kind) ? mulib_tsrt_binding(vm, top) : NULL;
  if (binding && binding->sealed) return mulib_termscript_fail(vm, err, "sealed symbol");
  /* ARRAY objects are mutable and allocated by this VM. */
  mulib_termscript_object_t *obj = (mulib_termscript_object_t *)array.object;
  if (!push) {
    if (!obj->count) return mulib_termscript_fail(vm, err, "pop from empty array");
    vm->state->stack[vm->state->count - 1] = obj->items[--obj->count].value;
    return 1;
  }
  const mulib_termscript_value_t *below = mulib_termscript_peek(vm, 1);
  if (!below || !mulib_termscript_resolve(vm, below, &value))
    return mulib_termscript_fail(vm, err, "expected a value before the array");
  if (obj->count == obj->capacity) {
    if (obj->capacity > SIZE_MAX / sizeof *obj->items / 2)
      return mulib_tsrt_nomem(vm, err);
    size_t cap = obj->capacity ? obj->capacity * 2 : 4;
    mulib_tsrt_item_t *items = (mulib_tsrt_item_t *)mulib_tsrt_alloc(vm, cap * sizeof *items);
    if (!items) return mulib_tsrt_nomem(vm, err);
    if (obj->count) memcpy(items, obj->items, obj->count * sizeof *items);
    obj->items = items;
    obj->capacity = cap;
  }
  obj->items[obj->count++].value = value;
  vm->state->count -= 2;
  return 1;
}

static int
mulib_tsrt_iota(mulib_termscript_vm_t *vm, mulib_termscript_error_t *err)
{
  const mulib_termscript_value_t *a = mulib_termscript_peek(vm, 1), *b = mulib_termscript_peek(vm, 0);
  mulib_termscript_value_t first, last;
  if (!a || !mulib_termscript_resolve(vm, a, &first) || !mulib_termscript_resolve(vm, b, &last)
      || first.kind != MULIB_TERMSCRIPT_VALUE_INTEGER || last.kind != MULIB_TERMSCRIPT_VALUE_INTEGER)
    return mulib_termscript_fail(vm, err, "iota needs two integer endpoints");
  if (first.integer > last.integer) return mulib_termscript_fail(vm, err, "iota endpoints are reversed");
  uint64_t span = (uint64_t)last.integer - (uint64_t)first.integer;
  if (span >= (SIZE_MAX - sizeof(mulib_termscript_object_t)) / sizeof(mulib_tsrt_item_t))
    return mulib_tsrt_nomem(vm, err);
  size_t n = (size_t)span + 1;
  if (n > vm->limits.max_steps - vm->state->steps)
    return mulib_termscript_fail(vm, err, "step limit exceeded");
  mulib_termscript_object_t *obj = (mulib_termscript_object_t *)
    mulib_tsrt_alloc(vm, sizeof *obj + n * sizeof(mulib_tsrt_item_t));
  if (!obj) return mulib_tsrt_nomem(vm, err);
  obj->count = obj->capacity = n;
  obj->items = (mulib_tsrt_item_t *)(obj + 1);
  int64_t x = first.integer;
  for (size_t i = 0; i < n; ++i) {
    obj->items[i].value = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_INTEGER, x, NULL, 0);
    if (i + 1 < n) ++x;
  }
  vm->state->steps += n;
  mulib_termscript_value_t v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_ARRAY, 0, NULL, 0);
  v.object = obj;
  vm->state->stack[--vm->state->count - 1] = v;
  return 1;
}

static mulib_tsrt_key_t *
mulib_tsrt_key_find(const mulib_termscript_vm_t *vm, const char *mode, size_t mode_n,
                    const char *key, size_t key_n)
{
  for (mulib_tsrt_key_t *k = vm->state->keys; k; k = k->next)
    if (mulib_tsrt_same(mode, mode_n, k->mode, k->mode_length)
        && mulib_tsrt_same(key, key_n, k->name, k->length)) return k;
  return NULL;
}

static mulib_tsrt_key_t *
mulib_tsrt_key_match(const mulib_termscript_vm_t *vm, const char *name, size_t len)
{
  const struct mulib_termscript_vm_state *s = vm->state;
  mulib_tsrt_key_t *key = mulib_tsrt_key_find(vm, s->mode, s->mode_length, name, len);
  return !key && s->mode_length ? mulib_tsrt_key_find(vm, NULL, 0, name, len) : key;
}

static int
mulib_tsrt_key_bind(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *key,
                    const mulib_termscript_value_t *quote, mulib_termscript_error_t *err)
{
  struct mulib_termscript_vm_state *s = vm->state;
  mulib_tsrt_key_t *k = mulib_tsrt_key_find(vm, s->mode, s->mode_length, key->text, key->length);
  if (!k) {
    k = (mulib_tsrt_key_t *)mulib_tsrt_alloc(vm, sizeof *k);
    if (!k) return mulib_tsrt_nomem(vm, err);
    k->mode = s->mode;
    k->mode_length = s->mode_length;
    k->name = key->text;
    k->length = key->length;
    k->next = s->keys;
    s->keys = k;
  }
  k->quote = *quote;
  return 1;
}

static int
mulib_tsrt_predicate(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *input,
                     int *taken, mulib_termscript_error_t *err)
{
  mulib_termscript_value_t value;
  if (!mulib_termscript_resolve(vm, input, &value))
    return mulib_termscript_fail(vm, err, "unbound condition");
  if (value.kind == MULIB_TERMSCRIPT_VALUE_QUOTE) {
    struct mulib_termscript_vm_state *s = vm->state;
    size_t floor = s->floor, base = s->count, offset = s->offset;
    s->floor = base;
    int ok = mulib_tsrt_invoke(vm, &value, err), one = s->count - base == 1;
    int resolved = ok && one && mulib_termscript_resolve(vm, &s->stack[base], &value);
    s->floor = floor;
    s->count = base;
    s->offset = offset;
    if (!ok) return 0;
    if (!one) return mulib_termscript_fail(vm, err, "condition must produce one boolean");
    if (!resolved) return mulib_termscript_fail(vm, err, "unbound condition result");
  }
  if (value.kind != MULIB_TERMSCRIPT_VALUE_BOOL)
    return mulib_termscript_fail(vm, err, "expected a boolean condition");
  *taken = value.integer != 0;
  return 1;
}

static int
mulib_tsrt_if(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *cond,
              const mulib_termscript_value_t *yes, const mulib_termscript_value_t *no,
              mulib_termscript_error_t *err)
{
  if (yes->kind != MULIB_TERMSCRIPT_VALUE_QUOTE || (no && no->kind != MULIB_TERMSCRIPT_VALUE_QUOTE))
    return mulib_termscript_fail(vm, err, "expected branch quotes");
  int taken;
  if (!mulib_tsrt_predicate(vm, cond, &taken, err)) return 0;
  return taken ? mulib_tsrt_invoke(vm, yes, err) : (!no || mulib_tsrt_invoke(vm, no, err));
}

static int
mulib_tsrt_while(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *cond,
                 const mulib_termscript_value_t *body, mulib_termscript_error_t *err)
{
  if (cond->kind != MULIB_TERMSCRIPT_VALUE_QUOTE || body->kind != MULIB_TERMSCRIPT_VALUE_QUOTE)
    return mulib_termscript_fail(vm, err, "while requires condition and body quotes");
  size_t offset = vm->state->offset;
  for (;;) {
    int taken;
    vm->state->offset = offset;
    if (!mulib_tsrt_predicate(vm, cond, &taken, err)) return 0;
    if (!taken) return 1;
    if (!mulib_tsrt_invoke(vm, body, err)) return 0;
  }
}

static int
mulib_tsrt_repeat(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *count,
                  const mulib_termscript_value_t *body, mulib_termscript_error_t *err)
{
  if (count->kind != MULIB_TERMSCRIPT_VALUE_INTEGER || count->integer < 0
      || body->kind != MULIB_TERMSCRIPT_VALUE_QUOTE)
    return mulib_termscript_fail(vm, err, "repeat requires a nonnegative count and a quote");
  size_t offset = vm->state->offset;
  for (int64_t i = 0; i < count->integer; ++i) {
    vm->state->offset = offset;
    if (!mulib_tsrt_invoke(vm, body, err)) return 0;
  }
  return 1;
}

static int
mulib_tsrt_dispatch(mulib_termscript_vm_t *vm, const char *word, size_t n, mulib_termscript_error_t *err)
{
  static const struct { const char *name, *alias; enum mulib_tsrt_op op; } ops[] = {
    {"add", "+", MULIB_TSRT_ADD}, {"sub", "-", MULIB_TSRT_SUB}, {"mul", "*", MULIB_TSRT_MUL},
    {"div", "/", MULIB_TSRT_DIV}, {"mod", NULL, MULIB_TSRT_MOD}, {"eq?", "=", MULIB_TSRT_EQ},
    {"ne?", NULL, MULIB_TSRT_NE}, {"lt?", NULL, MULIB_TSRT_LT}, {"le?", NULL, MULIB_TSRT_LE},
    {"gt?", NULL, MULIB_TSRT_GT}, {"ge?", NULL, MULIB_TSRT_GE}
  };
  struct mulib_termscript_vm_state *s = vm->state;
  const mulib_termscript_value_t *a = mulib_termscript_peek(vm, 1);
  const mulib_termscript_value_t *b = mulib_termscript_peek(vm, 0);
  const mulib_termscript_value_t *c = mulib_termscript_peek(vm, 2);
  mulib_termscript_value_t x, y, z;
  const char *problem = "stack underflow";
#define MULIB_TSRT_WORD(str) mulib_tsrt_is(word, n, str)
  if (!n) return 1;
  for (size_t i = 0; i < sizeof ops / sizeof *ops; ++i)
    if (MULIB_TSRT_WORD(ops[i].name) || (ops[i].alias && MULIB_TSRT_WORD(ops[i].alias)))
      return mulib_tsrt_binary(vm, ops[i].op, err);
  if (MULIB_TSRT_WORD("true") || MULIB_TSRT_WORD("false") || MULIB_TSRT_WORD("nil")) {
    x = mulib_tsrt_value(MULIB_TSRT_WORD("nil") ? MULIB_TERMSCRIPT_VALUE_NIL : MULIB_TERMSCRIPT_VALUE_BOOL,
                        MULIB_TSRT_WORD("true"), NULL, 0);
    return mulib_tsrt_push(vm, &x, err);
  }
  if (MULIB_TSRT_WORD("dup") || MULIB_TSRT_WORD("over")) {
    const mulib_termscript_value_t *v = MULIB_TSRT_WORD("dup") ? b : a;
    return v ? mulib_tsrt_push(vm, v, err) : mulib_termscript_fail(vm, err, problem);
  }
  if (MULIB_TSRT_WORD("drop")) {
    if (!mulib_termscript_pop(vm, NULL)) goto fail;
    return 1;
  }
  if (MULIB_TSRT_WORD("swap") || MULIB_TSRT_WORD("rot") || MULIB_TSRT_WORD("nip")) {
    if (!a) goto fail;
    x = *a;
    y = *b;
    if (MULIB_TSRT_WORD("nip")) --s->count;
    else if (MULIB_TSRT_WORD("swap")) s->stack[s->count - 1] = x;
    else {
      if (!c) goto fail;
      s->stack[s->count - 1] = *c;
      s->stack[s->count - 3] = x;
    }
    s->stack[s->count - 2 + (size_t)MULIB_TSRT_WORD("nip")] = y;
    return 1;
  }
  if (MULIB_TSRT_WORD("clear")) { s->count = s->floor; return 1; }
  if (MULIB_TSRT_WORD("depth")) {
    size_t depth = mulib_termscript_stack_size(vm);
    if (depth > (uint64_t)INT64_MAX) return mulib_termscript_fail(vm, err, "depth out of range");
    return mulib_termscript_push_int(vm, (int64_t)depth)
      || mulib_termscript_fail(vm, err, "stack or allocation limit");
  }
  if (MULIB_TSRT_WORD("neg") || MULIB_TSRT_WORD("not") || MULIB_TSRT_WORD("and") || MULIB_TSRT_WORD("or")) {
    int logical = !MULIB_TSRT_WORD("neg"), binary = MULIB_TSRT_WORD("and") || MULIB_TSRT_WORD("or");
    if (!b || (binary && !a)) goto fail;
    if (!mulib_termscript_resolve(vm, b, &x) || (binary && !mulib_termscript_resolve(vm, a, &y)))
      return mulib_termscript_fail(vm, err, "unbound symbol");
    if (logical) {
      if (x.kind != MULIB_TERMSCRIPT_VALUE_BOOL || (binary && y.kind != x.kind))
        return mulib_termscript_fail(vm, err, "expected booleans");
      x.integer = !binary ? !x.integer : MULIB_TSRT_WORD("and")
        ? (y.integer && x.integer) : (y.integer || x.integer);
    } else {
      if (x.kind != MULIB_TERMSCRIPT_VALUE_INTEGER)
        return mulib_termscript_fail(vm, err, "expected an integer");
      if (x.integer == INT64_MIN) return mulib_termscript_fail(vm, err, "integer overflow");
      x.integer = -x.integer;
    }
    if (!logical && mulib_tsrt_ref(b->kind)) {
      if (!mulib_tsrt_store(vm, b, &x, err)) return 0;
      --s->count;
    } else {
      s->count -= (size_t)binary;
      s->stack[s->count - 1] = x;
    }
    return 1;
  }
  if (MULIB_TSRT_WORD("set")) {
    if (!a) goto fail;
    if (!mulib_termscript_resolve(vm, b, &x))
      return mulib_termscript_fail(vm, err, "unbound value");
    if (!mulib_tsrt_store(vm, a, &x, err)) return 0;
    s->count -= 2;
    return 1;
  }
  if (MULIB_TSRT_WORD("get")) {
    if (!b) goto fail;
    if (a && (b->kind == MULIB_TERMSCRIPT_VALUE_FIELD
              || (a->kind == MULIB_TERMSCRIPT_VALUE_ARRAY_SYMBOL && b->kind == MULIB_TERMSCRIPT_VALUE_INTEGER)
              || (a->kind == MULIB_TERMSCRIPT_VALUE_RECORD_SYMBOL && (b->kind == MULIB_TERMSCRIPT_VALUE_STRING
                  || b->kind == MULIB_TERMSCRIPT_VALUE_SYMBOL))))
      return mulib_tsrt_element(vm, 0, err);
    if (!mulib_termscript_resolve(vm, b, &x))
      return mulib_termscript_fail(vm, err, "unbound symbol");
    s->stack[s->count - 1] = x;
    return 1;
  }
  if (MULIB_TSRT_WORD("at") || MULIB_TSRT_WORD("field")) return mulib_tsrt_element(vm, 0, err);
  if (MULIB_TSRT_WORD("set-at") || MULIB_TSRT_WORD("set-field")) return mulib_tsrt_element(vm, 1, err);
  if (MULIB_TSRT_WORD("push") || MULIB_TSRT_WORD("pop"))
    return mulib_tsrt_array_word(vm, MULIB_TSRT_WORD("push"), err);
  if (MULIB_TSRT_WORD("iota")) return mulib_tsrt_iota(vm, err);
  if (MULIB_TSRT_WORD("len") || MULIB_TSRT_WORD("length")) {
    if (!b) goto fail;
    if (!mulib_termscript_resolve(vm, b, &x)) return mulib_termscript_fail(vm, err, "unbound symbol");
    size_t len;
    if (x.kind == MULIB_TERMSCRIPT_VALUE_STRING || x.kind == MULIB_TERMSCRIPT_VALUE_COMMAND
        || x.kind == MULIB_TERMSCRIPT_VALUE_KEY || x.kind == MULIB_TERMSCRIPT_VALUE_ESCAPE) len = x.length;
    else if (x.kind >= MULIB_TERMSCRIPT_VALUE_ARRAY && x.kind <= MULIB_TERMSCRIPT_VALUE_RECORD)
      len = mulib_termscript_value_count(&x);
    else return mulib_termscript_fail(vm, err, "expected a string or collection");
    if (len > (uint64_t)INT64_MAX) return mulib_termscript_fail(vm, err, "length out of range");
    s->stack[s->count - 1] = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_INTEGER, (int64_t)len, NULL, 0);
    return 1;
  }
  if (MULIB_TSRT_WORD("bind") || MULIB_TSRT_WORD("bind-key")) {
    if (!a) goto fail;
    x = *a;
    y = *b;
    size_t consumed = 2;
    if (x.kind == MULIB_TERMSCRIPT_VALUE_TUPLE && c && mulib_tsrt_ref(c->kind)) {
      if (!mulib_tsrt_lambda(vm, a, &y, err)) return 0;
      x = *c;
      consumed = 3;
    }
    if (x.kind == MULIB_TERMSCRIPT_VALUE_QUOTE) { z = x; x = y; y = z; }
    if (y.kind != MULIB_TERMSCRIPT_VALUE_QUOTE)
      return mulib_termscript_fail(vm, err, "bind requires a quote");
    if (x.kind == MULIB_TERMSCRIPT_VALUE_KEY) {
      if (!mulib_tsrt_key_bind(vm, &x, &y, err)) return 0;
      s->count -= consumed;
      return 1;
    }
    if (MULIB_TSRT_WORD("bind-key")) return mulib_termscript_fail(vm, err, "expected a key");
    if (!mulib_tsrt_store(vm, &x, &y, err)) return 0;
    s->count -= consumed - 1; /* Retain the symbol for chained seal. */
    s->stack[s->count - 1] = x;
    return 1;
  }
  if (MULIB_TSRT_WORD("seal")) {
    if (!b) goto fail;
    mulib_tsrt_binding_t *entry = mulib_tsrt_ref(b->kind) ? mulib_tsrt_lookup(vm, b, s->scope) : NULL;
    if (!entry) return mulib_termscript_fail(vm, err, "expected a bound local symbol");
    entry->sealed = 1;
    --s->count;
    return 1;
  }
  if (MULIB_TSRT_WORD("lambda")) {
    if (!a) goto fail;
    x = *b;
    if (!mulib_tsrt_lambda(vm, a, &x, err)) return 0;
    s->stack[--s->count - 1] = x;
    return 1;
  }
  if (MULIB_TSRT_WORD("compose")) {
    if (!a) goto fail;
    if (!mulib_termscript_resolve(vm, a, &x) || !mulib_termscript_resolve(vm, b, &y)
        || x.kind != MULIB_TERMSCRIPT_VALUE_QUOTE || y.kind != MULIB_TERMSCRIPT_VALUE_QUOTE)
      return mulib_termscript_fail(vm, err, "compose requires two quotes");
    mulib_termscript_object_t *obj = (mulib_termscript_object_t *)mulib_tsrt_alloc(vm, sizeof *obj);
    if (!obj) return mulib_tsrt_nomem(vm, err);
    obj->composed = 1;
    obj->parts[0] = x;
    obj->parts[1] = y;
    z = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_QUOTE, 0, NULL, 0);
    z.object = obj;
    s->stack[--s->count - 1] = z;
    return 1;
  }
  if (MULIB_TSRT_WORD("node")) {
    if (!b) goto fail;
    if (!mulib_tsrt_at(b->program, b->node))
      return mulib_termscript_fail(vm, err, "value has no source node");
    x = *b;
    x.kind = MULIB_TERMSCRIPT_VALUE_NODE;
    s->stack[s->count - 1] = x;
    return 1;
  }
  if (MULIB_TSRT_WORD("invoke") || MULIB_TSRT_WORD("call") || MULIB_TSRT_WORD("eval")) {
    if (!b) goto fail;
    if (!mulib_termscript_resolve(vm, b, &x))
      return mulib_termscript_fail(vm, err, "unbound quote");
    if (x.kind == MULIB_TERMSCRIPT_VALUE_NODE && MULIB_TSRT_WORD("eval")) {
      if (!mulib_tsrt_enter(vm, err)) return 0;
      --s->count;
      int good = mulib_tsrt_node(vm, x.program, x.node, err);
      --s->calls;
      return good;
    }
    if (x.kind != MULIB_TERMSCRIPT_VALUE_QUOTE)
      return mulib_termscript_fail(vm, err, "expected a quote");
    --s->count;
    return mulib_tsrt_invoke(vm, &x, err);
  }
  if (MULIB_TSRT_WORD("if") || MULIB_TSRT_WORD("ifelse")) {
    int both = MULIB_TSRT_WORD("ifelse");
    if (!a || (both && !c)) goto fail;
    x = *(both ? c : a);
    y = *(both ? a : b);
    if (both) z = *b;
    s->count -= both ? 3 : 2;
    return mulib_tsrt_if(vm, &x, &y, both ? &z : NULL, err);
  }
  if (MULIB_TSRT_WORD("while") || MULIB_TSRT_WORD("repeat") || MULIB_TSRT_WORD("loop")) {
    int repeat = MULIB_TSRT_WORD("repeat");
    if (!a) goto fail;
    if (!mulib_termscript_resolve(vm, a, &x) || !mulib_termscript_resolve(vm, b, &y))
      return mulib_termscript_fail(vm, err, "unbound loop argument");
    if (repeat && x.kind == MULIB_TERMSCRIPT_VALUE_QUOTE) { z = x; x = y; y = z; }
    s->count -= 2;
    return repeat ? mulib_tsrt_repeat(vm, &x, &y, err) : mulib_tsrt_while(vm, &x, &y, err);
  }
  if (MULIB_TSRT_WORD("import") || MULIB_TSRT_WORD("use")) return mulib_tsrt_import(vm, word, n, err);
  if (MULIB_TSRT_WORD("exec")) return mulib_tsrt_exec(vm, err);
  if (MULIB_TSRT_WORD("emit")) {
    int handled = mulib_tsrt_host(vm, word, n, err);
    return handled == 1 || (handled == 0 && mulib_termscript_fail(vm, err, "terminal emission is not enabled"));
  }
  if (MULIB_TSRT_WORD("mode") || MULIB_TSRT_WORD("state")) {
    if (!b) goto fail;
    if (!mulib_termscript_resolve(vm, b, &x) || x.kind != MULIB_TERMSCRIPT_VALUE_STRING)
      return mulib_termscript_fail(vm, err, "expected a mode name string");
    if (!mulib_termscript_vm_set_mode(vm, x.text, x.length)) return mulib_tsrt_nomem(vm, err);
    --s->count;
    return 1;
  }
  if (MULIB_TSRT_WORD("mode?") || MULIB_TSRT_WORD("state?")) {
    x = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_STRING, 0, s->mode ? s->mode : "", s->mode_length);
    return mulib_tsrt_push(vm, &x, err);
  }
  if (MULIB_TSRT_WORD("press")) {
    if (!b) goto fail;
    if (!mulib_termscript_resolve(vm, b, &x) || x.kind != MULIB_TERMSCRIPT_VALUE_KEY)
      return mulib_termscript_fail(vm, err, "expected a key");
    mulib_tsrt_key_t *key = mulib_tsrt_key_match(vm, x.text, x.length);
    if (!key) return mulib_termscript_fail(vm, err, "unbound key");
    --s->count;
    return mulib_tsrt_invoke(vm, &key->quote, err);
  }
  if (word[0] == '.' && n > 1) {
    x = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_FIELD, 0, word + 1, n - 1);
    return mulib_tsrt_push(vm, &x, err);
  }
  {
    x = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_SYMBOL, 0, word, n);
    mulib_tsrt_binding_t *entry = mulib_tsrt_binding(vm, &x);
    if (entry) {
      return entry->value.kind == MULIB_TERMSCRIPT_VALUE_QUOTE
        ? mulib_tsrt_invoke(vm, &entry->value, err) : mulib_tsrt_push(vm, &entry->value, err);
    }
    int handled = mulib_tsrt_host(vm, word, n, err);
    if (handled == 1) return 1;
    if (handled == -1) return 0;
  }
  problem = "unknown word";
fail:
  return mulib_termscript_fail(vm, err, problem);
#undef MULIB_TSRT_WORD
}

static int
mulib_tsrt_hex(unsigned char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static int
mulib_tsrt_decode(mulib_termscript_vm_t *vm, mulib_termscript_value_t *v, mulib_termscript_error_t *err)
{
  if (!memchr(v->text, '\\', v->length)) return 1;
  if (v->length == SIZE_MAX) return mulib_tsrt_nomem(vm, err);
  char *out = (char *)mulib_tsrt_alloc(vm, v->length + 1);
  if (!out) return mulib_tsrt_nomem(vm, err);
  size_t used = 0;
  for (size_t i = 0; i < v->length; ++i) {
    unsigned char c = (unsigned char)v->text[i];
    if (c == '\\') {
      if (++i >= v->length) goto invalid;
      c = (unsigned char)v->text[i];
      switch (c) {
      case '\\': case '"': case '`': break;
      case '0': c = 0; break;
      case 'a': c = '\a'; break;
      case 'b': c = '\b'; break;
      case 'e': c = 27; break;
      case 'f': c = '\f'; break;
      case 'n': c = '\n'; break;
      case 'r': c = '\r'; break;
      case 't': c = '\t'; break;
      case 'v': c = '\v'; break;
      case 'x': {
        if (v->length - i < 3) goto invalid;
        int high = mulib_tsrt_hex((unsigned char)v->text[i + 1]);
        int low = mulib_tsrt_hex((unsigned char)v->text[i + 2]);
        if (high < 0 || low < 0) goto invalid;
        c = (unsigned char)(16 * high + low);
        i += 2;
        break;
      }
      default: goto invalid;
      }
    }
    ((unsigned char *)out)[used++] = c;
  }
  out[used] = '\0';
  v->text = out;
  v->length = used;
  return 1;
invalid:
  return mulib_termscript_fail(vm, err, "invalid quoted escape");
}

static int
mulib_tsrt_node(mulib_termscript_vm_t *vm, const mulib_termscript_program_t *p,
                size_t index, mulib_termscript_error_t *err)
{
  const mulib_termscript_node_t *n = mulib_tsrt_at(p, index);
  if (!n) return mulib_termscript_fail(vm, err, "invalid AST node");
  struct mulib_termscript_vm_state *s = vm->state;
  const char *text = p->source + n->offset;
  size_t len = n->length;
  mulib_termscript_value_t v;
  s->offset = n->offset;
  if (!mulib_termscript_step(vm, err)) return 0;
  switch (n->kind) {
  case MULIB_TERMSCRIPT_WORD:
    return mulib_tsrt_dispatch(vm, text, len, err);
  case MULIB_TERMSCRIPT_INTEGER:
    v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_INTEGER, n->integer, NULL, 0);
    break;
  case MULIB_TERMSCRIPT_STRING:
  case MULIB_TERMSCRIPT_COMMAND:
    v = mulib_tsrt_value(n->kind == MULIB_TERMSCRIPT_STRING ? MULIB_TERMSCRIPT_VALUE_STRING
                         : MULIB_TERMSCRIPT_VALUE_COMMAND, 0, text, len);
    if (!mulib_tsrt_decode(vm, &v, err)) return 0;
    break;
  case MULIB_TERMSCRIPT_CHARACTER:
    if (n->integer < 0 || n->integer > 0x10ffff
        || (n->integer >= 0xd800 && n->integer <= 0xdfff))
      return mulib_termscript_fail(vm, err, "invalid character AST");
    if (!mulib_tsrt_char(vm, (uint32_t)n->integer, &v)) return mulib_tsrt_nomem(vm, err);
    break;
  case MULIB_TERMSCRIPT_SYMBOL:
    v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_SYMBOL, 0, text, len);
    break;
  case MULIB_TERMSCRIPT_ARRAY_SYMBOL:
    v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_ARRAY_SYMBOL, 0, text, len);
    break;
  case MULIB_TERMSCRIPT_RECORD_SYMBOL:
    v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_RECORD_SYMBOL, 0, text, len);
    break;
  case MULIB_TERMSCRIPT_PARAMETER:
    if (mulib_tsrt_is(text, len, "TERM")) {
      v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_STRING, 0, s->term ? s->term : "", s->term_length);
    } else {
      for (const mulib_tsrt_frame_t *f = s->frame; f; f = f->parent) {
        if (!mulib_termscript_step(vm, err)) return 0;
        const mulib_tsrt_item_t *args = (const mulib_tsrt_item_t *)(f + 1);
        for (size_t i = 0; i < f->count; ++i)
          if (mulib_tsrt_same(text, len, args[i].name, args[i].length))
            return mulib_tsrt_push(vm, &args[i].value, err);
      }
      if (s->frame) return mulib_termscript_fail(vm, err, "unbound parameter");
      v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_PARAMETER, 0, text, len);
    }
    break;
  case MULIB_TERMSCRIPT_KEY:
    v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_KEY, 0, text, len);
    break;
  case MULIB_TERMSCRIPT_ESCAPE:
    v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_ESCAPE, 0, text, len);
    break;
  case MULIB_TERMSCRIPT_FIELD:
    v = mulib_tsrt_value(MULIB_TERMSCRIPT_VALUE_FIELD, 0, text, len);
    break;
  case MULIB_TERMSCRIPT_BLOCK:
    if (!mulib_tsrt_quote(vm, p, index, &v, err)) return 0;
    break;
  case MULIB_TERMSCRIPT_ARRAY: case MULIB_TERMSCRIPT_RECORD: case MULIB_TERMSCRIPT_TUPLE:
    if (!mulib_tsrt_container(vm, p, n, 0, err)) return 0;
    s->stack[s->count - 1].program = p;
    s->stack[s->count - 1].node = index;
    return 1;
  default:
    return mulib_termscript_fail(vm, err, "unsupported AST node");
  }
  v.program = p;
  v.node = index;
  return mulib_tsrt_push(vm, &v, err);
}

static int
mulib_tsrt_space(char c)
{
  return c == ' ' || (c >= '\t' && c <= '\r');
}

static int
mulib_tsrt_term_sep(char c)
{
  return mulib_tsrt_space(c) || c == ',' || c == '|';
}

/* TERM patterns are exact names or prefixes with a final '*'. Unselected
 * branches still count toward the step budget. */
static int
mulib_tsrt_term_matches(const struct mulib_termscript_vm_state *s, const char *p, size_t n)
{
  int found = 0, matched = 0;
  for (size_t i = 0; i < n;) {
    while (i < n && mulib_tsrt_term_sep(p[i])) ++i;
    if (i == n) break;
    int quoted = p[i] == '"';
    if (quoted) ++i;
    size_t start = i;
    while (i < n && (quoted ? p[i] != '"' : !mulib_tsrt_term_sep(p[i]))) ++i;
    if ((quoted && i == n) || (!quoted && i == start)) return -1;
    size_t len = i - start;
    if (quoted && ++i < n && !mulib_tsrt_term_sep(p[i])) return -1;
    found = 1;
    if (len && p[start + len - 1] == '*') {
      --len;
      if (s->term_length >= len && (!len || !memcmp(s->term, p + start, len))) matched = 1;
    } else if (mulib_tsrt_same(s->term, s->term_length, p + start, len)) {
      matched = 1;
    }
  }
  return found ? matched : -1;
}

static int
mulib_tsrt_adjacent(mulib_termscript_vm_t *vm, const mulib_termscript_program_t *p,
                    size_t index, size_t *next, mulib_termscript_error_t *err)
{
  /* 1 ran combined syntax; 0 is not a match; -1 is a runtime error. */
  const mulib_termscript_node_t *n = mulib_tsrt_at(p, index);
  const mulib_termscript_node_t *b = mulib_tsrt_at(p, n->next);
  if (!b) return 0;
  struct mulib_termscript_vm_state *s = vm->state;
  if ((n->kind == MULIB_TERMSCRIPT_SYMBOL || n->kind == MULIB_TERMSCRIPT_ARRAY_SYMBOL
       || n->kind == MULIB_TERMSCRIPT_RECORD_SYMBOL) && b->kind == MULIB_TERMSCRIPT_BLOCK
      && mulib_tsrt_word(p, b->next, "set")) {
    /* Literal block initializer: evaluate in isolation; `bind` is lazy and
     * computed quotes can still be stored by ordinary stack `set`. */
    if (!mulib_tsrt_node(vm, p, index, err)) return -1;
    mulib_termscript_value_t ref = s->stack[s->count - 1], quote, value;
    size_t base = --s->count, floor = s->floor;
    s->offset = b->offset;
    if (!mulib_termscript_step(vm, err) || !mulib_tsrt_quote(vm, p, n->next, &quote, err)) return -1;
    s->floor = base;
    int ok = mulib_tsrt_invoke(vm, &quote, err);
    int one = s->count - base == 1 && mulib_termscript_resolve(vm, &s->stack[base], &value);
    s->floor = floor;
    s->count = base;
    if (!ok) return -1;
    s->offset = p->nodes[b->next].offset;
    if (!one) {
      mulib_termscript_fail(vm, err, "initializer must produce one value");
      return -1;
    }
    if (!mulib_termscript_step(vm, err) || !mulib_tsrt_store(vm, &ref, &value, err)) return -1;
    *next = p->nodes[b->next].next;
    return 1;
  }
  if (n->kind == MULIB_TERMSCRIPT_TUPLE && b->kind == MULIB_TERMSCRIPT_BLOCK
      && (mulib_tsrt_word(p, b->next, "lambda") || mulib_tsrt_word(p, b->next, "bind"))) {
    s->offset = n->offset;
    if (!mulib_termscript_step(vm, err) || !mulib_tsrt_container(vm, p, n, 1, err)) return -1;
    *next = n->next;
    return 1;
  }
  /* An isolated pair of parameters may be written `$left + $right`.
   * The regular postfix spelling works for all other expressions. */
  if (n->kind == MULIB_TERMSCRIPT_PARAMETER && b->kind == MULIB_TERMSCRIPT_WORD) {
    const mulib_termscript_node_t *c = mulib_tsrt_at(p, b->next);
    if (c && c->kind == MULIB_TERMSCRIPT_PARAMETER
        && (mulib_tsrt_word(p, n->next, "+") || mulib_tsrt_word(p, n->next, "-")
            || mulib_tsrt_word(p, n->next, "*") || mulib_tsrt_word(p, n->next, "/"))) {
      if (!mulib_tsrt_node(vm, p, index, err) || !mulib_tsrt_node(vm, p, b->next, err)
          || !mulib_tsrt_node(vm, p, n->next, err)) return -1;
      *next = c->next;
      return 1;
    }
  }
  if (n->kind != MULIB_TERMSCRIPT_WORD || b->kind != MULIB_TERMSCRIPT_BLOCK) return 0;
  int loop = mulib_tsrt_word(p, index, "while") && mulib_tsrt_word(p, b->next, "loop");
  if (!loop && !mulib_tsrt_word(p, index, "if")) return 0;
  s->offset = n->offset;
  if (!mulib_termscript_step(vm, err)) return -1;
  const mulib_termscript_value_t *top = mulib_termscript_peek(vm, 0);
  mulib_termscript_value_t cond, body, otherwise;
  if (!top || !mulib_termscript_resolve(vm, top, &cond)) {
    mulib_termscript_fail(vm, err, "missing or unbound condition");
    return -1;
  }
  if (!mulib_termscript_step(vm, err) || !mulib_tsrt_quote(vm, p, n->next, &body, err)) return -1;
  *next = b->next;
  int has_else = !loop && mulib_tsrt_word(p, *next, "else");
  if (loop || has_else) {
    if (!mulib_termscript_step(vm, err)) return -1;
    *next = p->nodes[*next].next;
  }
  if (has_else) {
    const mulib_termscript_node_t *branch = mulib_tsrt_at(p, *next);
    if (!branch || branch->kind != MULIB_TERMSCRIPT_BLOCK) {
      mulib_termscript_fail(vm, err, "else requires a block");
      return -1;
    }
    if (!mulib_termscript_step(vm, err) || !mulib_tsrt_quote(vm, p, *next, &otherwise, err)) return -1;
    *next = branch->next;
  }
  --s->count;
  int ok = loop ? mulib_tsrt_while(vm, &cond, &body, err)
    : mulib_tsrt_if(vm, &cond, &body, has_else ? &otherwise : NULL, err);
  return ok ? 1 : -1;
}

static int
mulib_tsrt_seq(mulib_termscript_vm_t *vm, const mulib_termscript_program_t *p,
               size_t index, mulib_termscript_error_t *err)
{
  struct mulib_termscript_vm_state *s = vm->state;
  unsigned char branches[128];
  size_t depth = 0;
  int active = 1;
  while (index != SIZE_MAX) {
    const mulib_termscript_node_t *n = mulib_tsrt_at(p, index);
    if (!n) return mulib_termscript_fail(vm, err, "invalid AST node");
    s->offset = n->offset;
    if (n->kind == MULIB_TERMSCRIPT_DIRECTIVE) {
      const char *text = p->source + n->offset;
      if (!mulib_termscript_step(vm, err)) return 0;
      if (n->length >= 9 && !memcmp(text, "%if-term:", 9)) {
        if (depth == sizeof branches) return mulib_termscript_fail(vm, err, "directive nesting limit");
        int match = mulib_tsrt_term_matches(s, text + 9, n->length - 9);
        if (match < 0) return mulib_termscript_fail(vm, err, "invalid %if-term directive");
        branches[depth++] = (unsigned char)(active | (match << 1));
        active = active && match;
      } else if (n->length >= 6 && !memcmp(text, "%else:", 6)) {
        if (!depth || (branches[depth - 1] & 4))
          return mulib_termscript_fail(vm, err, "unexpected %else directive");
        active = (branches[depth - 1] & 3) == 1;
        branches[depth - 1] |= 4;
      } else if (n->length >= 5 && !memcmp(text, "%end:", 5)) {
        if (!depth) return mulib_termscript_fail(vm, err, "unexpected %end directive");
        active = branches[--depth] & 1;
      } else if (n->length < 8 || memcmp(text, "%module:", 8)) {
        return mulib_termscript_fail(vm, err, "unknown directive");
      }
    } else if (active) {
      size_t next;
      int adjacent = mulib_tsrt_adjacent(vm, p, index, &next, err);
      if (adjacent < 0) return 0;
      if (adjacent == 1) { index = next; continue; }
      if (!mulib_tsrt_node(vm, p, index, err)) return 0;
    } else if (!mulib_termscript_step(vm, err)) {
      return 0;
    }
    index = n->next;
  }
  return !depth || mulib_termscript_fail(vm, err, "missing %end directive");
}

int
mulib_termscript_run(mulib_termscript_vm_t *vm, const mulib_termscript_program_t *p,
                     mulib_termscript_error_t *err)
{
  if (err) { err->offset = 0; err->message = NULL; }
  if (!vm || !p) return mulib_termscript_fail(vm, err, "invalid run arguments");
  struct mulib_termscript_vm_state *s = mulib_tsrt_state(vm);
  if (!s) return mulib_tsrt_nomem(vm, err);
  if (s->active) return mulib_termscript_fail(vm, err, "run is not reentrant; use eval");
  s->steps = s->calls = s->offset = 0;
  s->active = 1;
  int ok = mulib_tsrt_seq(vm, p, p->root, err);
  s->active = 0;
  return ok;
}

int
mulib_termscript_eval(mulib_termscript_vm_t *vm, const mulib_termscript_program_t *p,
                      mulib_termscript_error_t *err)
{
  if (!vm || !vm->state || !vm->state->active) return mulib_termscript_run(vm, p, err);
  if (!p) return mulib_termscript_fail(vm, err, "invalid eval argument");
  if (!mulib_tsrt_enter(vm, err)) return 0;
  struct mulib_termscript_vm_state *s = vm->state;
  size_t offset = s->offset;
  int ok = mulib_tsrt_seq(vm, p, p->root, err);
  s->offset = offset;
  --s->calls;
  return ok;
}

int
mulib_termscript_key(mulib_termscript_vm_t *vm, const char *name, size_t len,
                     mulib_termscript_error_t *err)
{
  if (err) { err->offset = 0; err->message = NULL; }
  if (!vm || !name || !len) {
    mulib_termscript_fail(vm, err, "invalid key event");
    return -1;
  }
  struct mulib_termscript_vm_state *s = vm->state;
  if (!s) return 0;
  if (s->active) {
    mulib_termscript_fail(vm, err, "key events are not reentrant");
    return -1;
  }
  mulib_tsrt_key_t *k = mulib_tsrt_key_match(vm, name, len);
  if (!k) return 0;
  s->steps = s->calls = s->offset = 0;
  s->active = 1;
  int ok = mulib_tsrt_invoke(vm, &k->quote, err);
  s->active = 0;
  return ok ? 1 : -1;
}

#ifdef __cplusplus
}
#endif
#endif
