/** @file mulib-termscript-host.h
 * @brief Standard-library capabilities for the bounded Termscript VM.
 * Define MULIB_IMPL_TERMSCRIPT_HOST and the dependency switches once.
 */
#ifndef MULIB_TERMSCRIPT_HOST_H
#define MULIB_TERMSCRIPT_HOST_H
#include "mulib-termdb.h"
#include "mulib-termscript-library.h"
#include "mulib-termscript-runtime.h"
#include "mulib-termscript-state.h"
#include "mulib-termscript-terminal.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Explicit application authority. A length-delimited command has no NUL.
 * Return 1 for success, 0 for denial/failure. NULL disables shell execution. */
typedef int (*mulib_termscript_command_fn)(void *, const char *, size_t);

typedef struct {
  mulib_termscript_terminal_t *terminal;
  mulib_termdb_t *termdb;
  const char *termdb_profile;
  const char *library_path, *stdlib_root;
  mulib_termscript_command_fn command;
  void *command_context;
} mulib_termscript_host_t;

/* Borrow all configuration until detaching or clearing the VM. Installing
 * does not open capabilities, perform I/O, or enable a shell by itself.
 * With a NULL configuration, detach both the host words and source loader. */
void mulib_termscript_host_attach(mulib_termscript_vm_t *, mulib_termscript_host_t *);
int mulib_termscript_host_dispatch(mulib_termscript_vm_t *, void *, const char *,
                                   size_t, mulib_termscript_error_t *);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSCRIPT_HOST) && !defined(MULIB_TERMSCRIPT_HOST_IMPLEMENTED)
#define MULIB_TERMSCRIPT_HOST_IMPLEMENTED
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

static int
mulib_termscript_host_is_(const char *name, size_t n, const char *literal)
{
  return name && strlen(literal) == n && !memcmp(name, literal, n);
}

static int
mulib_termscript_host_error_(mulib_termscript_vm_t *vm, mulib_termscript_error_t *err,
                             const char *message)
{
  mulib_termscript_fail(vm, err, message);
  return -1;
}

static void
mulib_termscript_host_drop_(mulib_termscript_vm_t *vm, size_t n)
{
  mulib_termscript_value_t ignored;
  while (n--) (void)mulib_termscript_pop(vm, &ignored);
}

static int
mulib_termscript_host_value_(mulib_termscript_vm_t *vm, size_t depth,
                             mulib_termscript_value_t *value)
{
  const mulib_termscript_value_t *top = mulib_termscript_peek(vm, depth);
  return top && mulib_termscript_resolve(vm, top, value);
}

static int
mulib_termscript_host_num_(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *in,
                           int64_t *out)
{
  mulib_termscript_value_t value;
  if (!in || !mulib_termscript_resolve(vm, in, &value) || value.kind != MULIB_TERMSCRIPT_VALUE_INTEGER)
    return 0;
  *out = value.integer;
  return 1;
}

static int
mulib_termscript_host_tuple_(mulib_termscript_vm_t *vm, const mulib_termscript_value_t *value,
                             int32_t args[9], size_t *n)
{
  if (value->kind != MULIB_TERMSCRIPT_VALUE_TUPLE && value->kind != MULIB_TERMSCRIPT_VALUE_ARRAY)
    return 0;
  *n = mulib_termscript_value_count(value);
  if (*n > 9) return 0;
  for (size_t i = 0; i < *n; ++i) {
    int64_t x;
    if (!mulib_termscript_host_num_(vm, mulib_termscript_value_at(value, i, NULL, NULL), &x)
        || x < INT32_MIN || x > INT32_MAX) return 0;
    args[i] = (int32_t)x;
  }
  return 1;
}

static int
mulib_termscript_host_load_(void *ctx, const char *name, size_t length,
                             mulib_termscript_program_t *p, mulib_termscript_error_t *err)
{
  mulib_termscript_host_t *h = (mulib_termscript_host_t *)ctx;
  return mulib_termscript_library_load(h->library_path, h->stdlib_root, name, length, p, err);
}

void
mulib_termscript_host_attach(mulib_termscript_vm_t *vm, mulib_termscript_host_t *h)
{
  mulib_termscript_vm_set_host(vm, h ? mulib_termscript_host_dispatch : NULL, h);
  mulib_termscript_vm_set_loader(vm, h ? mulib_termscript_host_load_ : NULL, h);
}

static int
mulib_termscript_host_text_(mulib_termscript_vm_t *vm, mulib_termscript_terminal_t *t)
{
  mulib_termscript_value_t v;
  if (!mulib_termscript_host_value_(vm, 0, &v)) return 0;
  const char *text = v.text;
  size_t n = v.length;
  unsigned char ch[4];
  char number[32];
  switch (v.kind) {
  case MULIB_TERMSCRIPT_VALUE_CHARACTER:
    if (v.integer < 0 || v.integer > INT64_C(0x10ffff)) return 0;
    n = mulib_utf8_encode((uint32_t)v.integer, ch, sizeof ch);
    if (!n) return 0;
    text = (const char *)ch;
    break;
  case MULIB_TERMSCRIPT_VALUE_INTEGER: {
    int used = snprintf(number, sizeof number, "%" PRId64, v.integer);
    if (used < 0 || (size_t)used >= sizeof number) return 0;
    text = number;
    n = (size_t)used;
    break;
  }
  case MULIB_TERMSCRIPT_VALUE_BOOL:
    text = v.integer ? "true" : "false";
    n = strlen(text);
    break;
  case MULIB_TERMSCRIPT_VALUE_STRING:
    break;
  default:
    return 0;
  }
  if (!mulib_termscript_terminal_text(t, text, n)) return 0;
  mulib_termscript_host_drop_(vm, 1);
  return 1;
}

static int
mulib_termscript_host_function_(mulib_termscript_vm_t *vm, mulib_termscript_terminal_t *t,
                                 mulib_termfunc_op_t op, size_t consume)
{
  int32_t args[9] = { 0 };
  if (op == MULIB_TERMFUNC_MOVE || op == MULIB_TERMFUNC_REGION) {
    mulib_termscript_value_t value;
    size_t n;
    if (!mulib_termscript_host_value_(vm, consume, &value)
        || !mulib_termscript_host_tuple_(vm, &value, args, &n) || n != 2) return 0;
    ++consume;
  }
  if (!mulib_termscript_terminal_function(t, op, args[0], args[1])) return 0;
  mulib_termscript_host_drop_(vm, consume);
  return 1;
}

static int
mulib_termscript_host_emit_(mulib_termscript_vm_t *vm, mulib_termscript_host_t *h)
{
  mulib_termscript_terminal_t *t = h->terminal;
  mulib_termscript_value_t name, value;
  char symbol[64];
  size_t n = 0, consume = 1;
  int32_t args[9] = { 0 };
  if (!h->termdb || !t || !t->write || !mulib_termscript_host_value_(vm, 0, &name)
      || name.kind != MULIB_TERMSCRIPT_VALUE_ESCAPE || !name.length
      || name.length >= sizeof symbol || !name.text || memchr(name.text, 0, name.length)) return 0;
  if (mulib_termscript_host_value_(vm, 1, &value) && (value.kind == MULIB_TERMSCRIPT_VALUE_TUPLE
      || value.kind == MULIB_TERMSCRIPT_VALUE_ARRAY)) {
    if (!mulib_termscript_host_tuple_(vm, &value, args, &n)) return 0;
    ++consume;
  }
  memcpy(symbol, name.text, name.length);
  symbol[name.length] = 0;
  mulib_strbuf_t bytes = { NULL, 0, 0 };
  int ok = mulib_termdb_emit(h->termdb, h->termdb_profile, symbol, args, n, &bytes)
    && (!bytes.length || t->write(t->context, bytes.data, bytes.length) == 1);
  mulib_strbuf_free(&bytes);
  if (!ok) return 0;
  mulib_termscript_host_drop_(vm, consume);
  return 1;
}

static int
mulib_termscript_host_exec_(mulib_termscript_vm_t *vm, mulib_termscript_host_t *h)
{
  mulib_termscript_value_t v, command;
  if (!mulib_termscript_host_value_(vm, 0, &v)) return 0;
  size_t consume = 1;
  if (v.kind == MULIB_TERMSCRIPT_VALUE_MODULE) {
    if (!mulib_termscript_host_value_(vm, 1, &command) || command.kind != MULIB_TERMSCRIPT_VALUE_COMMAND)
      return 0;
    consume = 2;
    if (mulib_termscript_host_is_(v.text, v.length, "stdlib.terminfo")) {
      int32_t args[9] = { 0 };
      size_t n;
      mulib_termscript_value_t value;
      if (mulib_termscript_host_value_(vm, consume, &value) && (value.kind == MULIB_TERMSCRIPT_VALUE_TUPLE
          || value.kind == MULIB_TERMSCRIPT_VALUE_ARRAY)) {
        if (!mulib_termscript_host_tuple_(vm, &value, args, &n)) return 0;
        ++consume;
      }
      if (!mulib_termscript_terminal_capability(h->terminal, command.text, command.length, args))
        return 0;
      mulib_termscript_host_drop_(vm, consume);
      return 1;
    }
    if (mulib_termscript_host_is_(v.text, v.length, "stdlib.termfunc")
        || mulib_termscript_host_is_(v.text, v.length, "stdlib.vterm")) {
      mulib_termfunc_op_t op;
      return mulib_termfunc_lookup(command.text, command.length, &op)
        && mulib_termscript_host_function_(vm, h->terminal, op, consume);
    }
    if (!mulib_termscript_host_is_(v.text, v.length, "stdlib.shell")) return 0;
  } else {
    command = v;
  }
  if (command.kind != MULIB_TERMSCRIPT_VALUE_COMMAND || !h->command || !command.text
      || !command.length || memchr(command.text, 0, command.length)
      || h->command(h->command_context, command.text, command.length) != 1) return 0;
  mulib_termscript_host_drop_(vm, consume);
  return 1;
}

static int
mulib_termscript_host_state_(mulib_termscript_vm_t *vm)
{
  int64_t current, event;
  mulib_termscript_value_t table;
  mulib_termscript_transition_t triples[256];
  mulib_termscript_state_t state;
  if (!mulib_termscript_host_num_(vm, mulib_termscript_peek(vm, 2), &current)
      || !mulib_termscript_host_num_(vm, mulib_termscript_peek(vm, 1), &event)
      || !mulib_termscript_host_value_(vm, 0, &table) || table.kind != MULIB_TERMSCRIPT_VALUE_ARRAY)
    return 0;
  size_t n = mulib_termscript_value_count(&table);
  if (n > sizeof triples / sizeof *triples) return 0;
  for (size_t i = 0; i < n; ++i) {
    const mulib_termscript_value_t *tuple = mulib_termscript_value_at(&table, i, NULL, NULL);
    if (!tuple || tuple->kind != MULIB_TERMSCRIPT_VALUE_TUPLE || mulib_termscript_value_count(tuple) != 3
        || !mulib_termscript_host_num_(vm, mulib_termscript_value_at(tuple, 0, NULL, NULL), &triples[i].source)
        || !mulib_termscript_host_num_(vm, mulib_termscript_value_at(tuple, 1, NULL, NULL), &triples[i].event)
        || !mulib_termscript_host_num_(vm, mulib_termscript_value_at(tuple, 2, NULL, NULL), &triples[i].target))
      return 0;
  }
  if (!mulib_termscript_state_init(&state, current, triples, n)) return 0;
  (void)mulib_termscript_state_send(&state, event, NULL, NULL);
  mulib_termscript_host_drop_(vm, 3);
  return mulib_termscript_push_int(vm, state.state);
}

int
mulib_termscript_host_dispatch(mulib_termscript_vm_t *vm, void *context, const char *name,
                               size_t n, mulib_termscript_error_t *err)
{
  mulib_termscript_host_t *h = (mulib_termscript_host_t *)context;
  if (!vm || !h || !name) return 0;
#define MULIB_TS_HOST_WORD_(word) mulib_termscript_host_is_(name, n, (word))
  int ok;
  const char *failure;
  if (MULIB_TS_HOST_WORD_("typeout")) {
    ok = mulib_termscript_host_text_(vm, h->terminal);
    failure = "expected printable data and a terminal writer";
  } else if (MULIB_TS_HOST_WORD_("emit")) {
    ok = mulib_termscript_host_emit_(vm, h);
    failure = "escape code unavailable or wrong database arity";
  } else if (MULIB_TS_HOST_WORD_("exec")) {
    ok = mulib_termscript_host_exec_(vm, h);
    failure = "command unavailable or rejected by host";
  } else if (MULIB_TS_HOST_WORD_("state-next")) {
    ok = mulib_termscript_host_state_(vm);
    failure = "expected current state, event, and sorted transition triples";
  } else if (MULIB_TS_HOST_WORD_("termios.raw") || MULIB_TS_HOST_WORD_("termios.restore")) {
    int rc = MULIB_TS_HOST_WORD_("termios.raw") ? mulib_termscript_terminal_raw(h->terminal)
      : mulib_termscript_terminal_restore(h->terminal);
    ok = rc == MULIB_TERMUTILS_OK;
    failure = "unable to change termios mode";
  } else if (MULIB_TS_HOST_WORD_("termios.size")) {
    unsigned cols, rows;
    ok = mulib_termscript_terminal_size(h->terminal, &cols, &rows) == MULIB_TERMUTILS_OK;
    if (ok) {
      ok = mulib_termscript_push_int(vm, cols);
      if (ok && !mulib_termscript_push_int(vm, rows)) {
        mulib_termscript_host_drop_(vm, 1);
        ok = 0;
      }
    }
    failure = "unable to get terminal size";
  } else {
    mulib_termfunc_op_t op;
    if (MULIB_TS_HOST_WORD_("move")) op = MULIB_TERMFUNC_MOVE;
    else if (n <= 5 || memcmp(name, "term.", 5) || !mulib_termfunc_lookup(name + 5, n - 5, &op))
      return 0;
    ok = mulib_termscript_host_function_(vm, h->terminal, op, 0);
    failure = "terminal function unavailable or wrong parameters";
  }
#undef MULIB_TS_HOST_WORD_
  return ok ? 1 : mulib_termscript_host_error_(vm, err, failure);
}

#ifdef __cplusplus
}
#endif
#endif
