/** @file mulib-vizigen.h
 * @brief ASDL semantic-action bytecode for push listeners and visitor walkers.
 * Define MULIB_IMPL_VIZIGEN and the dependency switches listed in
 * manual/mulib-vizigen.md. No dependency implementation is enabled implicitly.
 */
#ifndef MULIB_VIZIGEN_H
#define MULIB_VIZIGEN_H
#include "mulib-asdl.h"
#include "mulib-binfilereader.h"
#include "mulib-binfilewriter.h"
#include "mulib-mmfile.h"
#include "mulib-walker.h"
#ifdef __cplusplus
extern "C" {
#endif
/** A symbolic CALL instruction guarded by production and event kind.
 * Products use their type name; sums use constructor names, not type names. */
typedef struct mulib_vizigen_rule {
  const char *production;
  mulib_event_kind_t kind;
  const char *action;
} mulib_vizigen_rule_t;
/** Validated, borrowed, immutable bytecode. Keep the bytes/mapping alive. */
typedef struct mulib_vizigen_program { mulib_binfile_t bytes; } mulib_vizigen_program_t;
/** Named host callback. Never persisted as a machine address. */
typedef struct mulib_vizigen_action {
  const char *name;
  mulib_evtlistener_event_fn call;
  void *user;
} mulib_vizigen_action_t;
/** Initialized executor; borrows bytecode and the complete callback table.
 * Safe to reuse across independent traversals; no traversal state is retained. */
typedef struct mulib_vizigen_runtime {
  mulib_vizigen_program_t program;
  const mulib_vizigen_action_t *actions;
  size_t count;
} mulib_vizigen_runtime_t;
/** Compile ordinary ASDL plus suffix @enter(name)/@leave(name) annotations on
 * products/constructors. Named callbacks, not embedded C. Atomic binary append.
 * NULL source is valid at length zero. Source may alias output. ASDL options
 * control parsing/module name; output format/C prefix are ignored. */
int mulib_vizigen_compile(const char *, size_t, const mulib_asdl_options_t *,
    mulib_buffer_t *, mulib_asdl_error_t *);
/** Compile API-supplied rules against a validated ASDL module; copies inputs
 * into bytecode. At most one rule per production/event kind. Atomic append. */
int mulib_vizigen_generate(const mulib_asdl_module_t *,
    const mulib_vizigen_rule_t *, size_t, mulib_buffer_t *, mulib_asdl_error_t *);
/** Validate exactly one complete binary program, without allocation, copying,
 * alignment requirements or relocation. Output unchanged on failure. */
int mulib_vizigen_open(mulib_vizigen_program_t *, const void *, size_t);
/** Borrow bytecode from an active MuLIB mapping; does not own/unmap it. */
int mulib_vizigen_from_mapping(mulib_vizigen_program_t *, const mulib_mmfile_t *);
/** Append the validated binary representation unchanged, atomically. */
int mulib_vizigen_serialize(const mulib_vizigen_program_t *, mulib_buffer_t *);
/** Consume one length-framed program from a binary reader. Reader and output
 * are unchanged on failure. Result borrows the reader's underlying bytes. */
int mulib_vizigen_deserialize(mulib_binfilereader_t *, mulib_vizigen_program_t *);
/** Bind every referenced action before executing anything. Reject missing or
 * duplicate bindings, invalid bytecode and excessive binding work. 1 success,
 * 0 failure; runtime unchanged on failure. Extra bindings are permitted. */
int mulib_vizigen_runtime_init(mulib_vizigen_runtime_t *,
    const mulib_vizigen_program_t *, const mulib_vizigen_action_t *, size_t);
/** Execute one parser-produced event. 1 continue, 0 stopped, -1 error.
 * Unknown productions are ignored; the bytecode is not an AST validator. */
int mulib_vizigen_dispatch(mulib_vizigen_runtime_t *, const mulib_event_t *);
/** Borrow an executor as an event listener (executor must outlive listener). */
mulib_evtlistener_t mulib_vizigen_listener(mulib_vizigen_runtime_t *);
/** Execute the identical bytecode on a borrowed AST via mulib-walker. */
int mulib_vizigen_walk(mulib_vizigen_runtime_t *, const mulib_ast_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_VIZIGEN) && !defined(MULIB_VIZIGEN_IMPLEMENTED)
#define MULIB_VIZIGEN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_VIZIGEN_LIMIT_ 67108864u
#define MULIB_VIZIGEN_COUNT_ 65536u
#define MULIB_VIZIGEN_WORK_ 1000000u
static int mulib_vizigen_fail_(mulib_asdl_error_t *e, mulib_asdl_error_code_t code,
    const char *source, size_t offset, const char *message) {
  size_t i;
  if (e) {
    memset(e, 0, sizeof *e); e->code = code; e->span.offset = offset;
    e->line = e->column = 1;
    for (i = 0; source && i < offset; ++i)
      if (source[i] == '\n') { ++e->line; e->column = 1; } else ++e->column;
    snprintf(e->message, sizeof e->message, "%s", message);
  }
  return 0;
}
static int mulib_vizigen_letter_(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int mulib_vizigen_more_(unsigned char c) {
  return mulib_vizigen_letter_(c) || c == '_' || (c >= '0' && c <= '9');
}
static int mulib_vizigen_id_(const char *s) {
  size_t i;
  if (!s || !mulib_vizigen_letter_((unsigned char)*s)) return 0;
  for (i = 1; i <= 255 && s[i]; ++i)
    if (!mulib_vizigen_more_((unsigned char)s[i])) return 0;
  return i <= 255;
}
static int mulib_vizigen_space_(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
/* Records are read through binfilereader, never native struct casts. */
static int mulib_vizigen_record_(mulib_binfilereader_t *r, mulib_vizigen_rule_t *rule) {
  uint64_t kind, np, na;
  mulib_binfile_t p, a;
  if (!mulib_binfilereader_uint(r, 4, &kind) || kind > 1 ||
      !mulib_binfilereader_uint(r, 4, &np) || np < 2 || np > 256 ||
      !mulib_binfilereader_uint(r, 4, &na) || na < 2 || na > 256 ||
      !mulib_binfilereader_bytes(r, (size_t)np, &p) ||
      !mulib_binfilereader_bytes(r, (size_t)na, &a) ||
      p.data[np - 1] || a.data[na - 1] ||
      memchr(p.data, 0, (size_t)np - 1) || memchr(a.data, 0, (size_t)na - 1) ||
      !mulib_vizigen_id_((const char *)p.data) || !mulib_vizigen_id_((const char *)a.data)) return 0;
  rule->kind = kind ? MULIB_EVENT_LEAVE : MULIB_EVENT_ENTER;
  rule->production = (const char *)p.data; rule->action = (const char *)a.data;
  return 1;
}
int mulib_vizigen_open(mulib_vizigen_program_t *out, const void *data, size_t size) {
  mulib_binfilereader_t r;
  mulib_vizigen_rule_t rule;
  uint64_t version, length, count, i;
  if (!out || !data || size < 16 || size > MULIB_VIZIGEN_LIMIT_ ||
      memcmp(data, "VIZI", 4) || !mulib_binfilereader_init(&r, data, size)) return 0;
  r.position = 4;
  if (!mulib_binfilereader_uint(&r, 4, &version) || version != 1 ||
      !mulib_binfilereader_uint(&r, 4, &length) || length != size ||
      !mulib_binfilereader_uint(&r, 4, &count) || count > MULIB_VIZIGEN_COUNT_) return 0;
  for (i = 0; i < count; ++i) if (!mulib_vizigen_record_(&r, &rule)) return 0;
  if (r.position != size) return 0;
  out->bytes = r.file; return 1;
}
int mulib_vizigen_from_mapping(mulib_vizigen_program_t *out, const mulib_mmfile_t *m) {
  return m && m->active && mulib_vizigen_open(out, m->data, m->size);
}
int mulib_vizigen_serialize(const mulib_vizigen_program_t *p, mulib_buffer_t *out) {
  mulib_vizigen_program_t checked;
  return p && mulib_vizigen_open(&checked, p->bytes.data, p->bytes.size) &&
      mulib_buffer_append(out, checked.bytes.data, checked.bytes.size);
}
int mulib_vizigen_deserialize(mulib_binfilereader_t *reader, mulib_vizigen_program_t *out) {
  mulib_binfilereader_t r;
  mulib_binfile_t bytes;
  uint64_t length;
  size_t start;
  mulib_vizigen_program_t p;
  if (!reader || !out) return 0;
  r = *reader; start = r.position;
  if (!mulib_binfilereader_bytes(&r, 8, &bytes) ||
      !mulib_binfilereader_uint(&r, 4, &length) || length < 16 || length > MULIB_VIZIGEN_LIMIT_ ||
      !mulib_binfilereader_seek(&r, start) ||
      !mulib_binfilereader_bytes(&r, (size_t)length, &bytes) ||
      !mulib_vizigen_open(&p, bytes.data, bytes.size)) return 0;
  *reader = r; *out = p; return 1;
}
int mulib_vizigen_generate(const mulib_asdl_module_t *m, const mulib_vizigen_rule_t *rules,
    size_t count, mulib_buffer_t *out, mulib_asdl_error_t *error) {
  mulib_buffer_t b = {NULL, 0, 0};
  size_t i, j, k, work = 0;
  int found, ok = 0;
  if (error) memset(error, 0, sizeof *error);
  if (!m || !out || (!rules && count))
    return mulib_vizigen_fail_(error, MULIB_ASDL_INVALID, NULL, 0, "invalid module, rules or output");
  if (count > MULIB_VIZIGEN_COUNT_) goto limit;
  for (i = 0; i < count; ++i) {
    if (!mulib_vizigen_id_(rules[i].production) || !mulib_vizigen_id_(rules[i].action) ||
        (rules[i].kind != MULIB_EVENT_ENTER && rules[i].kind != MULIB_EVENT_LEAVE))
      return mulib_vizigen_fail_(error, MULIB_ASDL_INVALID, NULL, 0, "invalid instruction");
    found = 0;
    for (j = 0; j < mulib_asdl_type_count(m); ++j) {
      const mulib_asdl_type_t *t = mulib_asdl_type(m, j);
      if (++work > MULIB_VIZIGEN_WORK_) goto limit;
      if (t->kind == MULIB_ASDL_PRODUCT && !strcmp(t->name, rules[i].production)) found = 1;
      for (k = 0; k < t->constructor_count; ++k) {
        if (++work > MULIB_VIZIGEN_WORK_) goto limit;
        if (!strcmp(t->constructors[k].name, rules[i].production)) found = 1;
      }
    }
    if (!found) return mulib_vizigen_fail_(error, MULIB_ASDL_NAME, NULL, 0, "unknown production");
    for (j = 0; j < i; ++j) {
      if (++work > MULIB_VIZIGEN_WORK_) goto limit;
      if (rules[j].kind == rules[i].kind && !strcmp(rules[j].production, rules[i].production))
        return mulib_vizigen_fail_(error, MULIB_ASDL_DUPLICATE, NULL, 0, "duplicate production action");
    }
  }
  if (!mulib_buffer_append(&b, "VIZI", 4) || !mulib_binfilewriter_uint(&b, 4, 1) ||
      !mulib_binfilewriter_uint(&b, 4, 0) || !mulib_binfilewriter_uint(&b, 4, count)) goto memory;
  for (i = 0; i < count; ++i) {
    size_t np = strlen(rules[i].production) + 1, na = strlen(rules[i].action) + 1;
    if (!mulib_binfilewriter_uint(&b, 4, rules[i].kind == MULIB_EVENT_LEAVE) ||
        !mulib_binfilewriter_uint(&b, 4, np) || !mulib_binfilewriter_uint(&b, 4, na) ||
        !mulib_buffer_append(&b, rules[i].production, np) ||
        !mulib_buffer_append(&b, rules[i].action, na)) goto memory;
  }
  if (!mulib_binfilewriter_patch(&b, 8, 4, b.length) || !mulib_buffer_append(out, b.data, b.length)) goto memory;
  ok = 1; goto done;
limit:
  mulib_vizigen_fail_(error, MULIB_ASDL_LIMIT, NULL, 0, "bytecode count/work limit"); goto done;
memory:
  mulib_vizigen_fail_(error, MULIB_ASDL_MEMORY, NULL, 0, "cannot append bytecode");
done:
  mulib_buffer_free(&b); return ok;
}
typedef struct mulib_vizigen_annotation_ {
  size_t offset, length;
  mulib_event_kind_t kind;
  char action[256];
} mulib_vizigen_annotation_;
int mulib_vizigen_compile(const char *source, size_t length, const mulib_asdl_options_t *options,
    mulib_buffer_t *out, mulib_asdl_error_t *error) {
  char *clean = NULL;
  mulib_asdl_module_t *m = NULL;
  mulib_vizigen_annotation_ *annotations = NULL;
  mulib_vizigen_rule_t *rules = NULL;
  size_t i = 0, j, k, start = 0, name, n, count = 0, capacity = 0, work = 0;
  int ok = 0;
  if (error) memset(error, 0, sizeof *error);
  if ((!source && length) || !out || length > 16777216u)
    return mulib_vizigen_fail_(error, length > 16777216u ? MULIB_ASDL_LIMIT : MULIB_ASDL_INVALID,
        NULL, 0, "invalid or oversized source/output");
  clean = (char *)malloc(length + 1);
  if (!clean) goto memory;
  if (length) memcpy(clean, source, length);
  clean[length] = 0;
  while (i < length) {
    start = i;
    if (i + 1 < length && ((source[i] == '-' && source[i + 1] == '-') ||
        (source[i] == '/' && source[i + 1] == '/'))) {
      while (i < length && source[i] != '\n' && source[i] != '\r') ++i;
    } else if (i + 1 < length && source[i] == '/' && source[i + 1] == '*') {
      i += 2;
      while (i + 1 < length && !(source[i] == '*' && source[i + 1] == '/')) ++i;
      if (i + 1 >= length) goto syntax;
      i += 2;
    } else if (source[i] == '@') {
      mulib_vizigen_annotation_ a;
      if (length - i >= 7 && !memcmp(source + i, "@enter(", 7)) a.kind = MULIB_EVENT_ENTER;
      else if (length - i >= 7 && !memcmp(source + i, "@leave(", 7)) a.kind = MULIB_EVENT_LEAVE;
      else goto syntax;
      i += 7; name = i;
      if (i == length || !mulib_vizigen_letter_((unsigned char)source[i])) goto syntax;
      while (i < length && mulib_vizigen_more_((unsigned char)source[i])) ++i;
      n = i - name;
      if (n > 255 || i == length || source[i] != ')') goto syntax;
      ++i; a.offset = start; a.length = i - start;
      memcpy(a.action, source + name, n); a.action[n] = 0;
      if (count == MULIB_VIZIGEN_COUNT_) goto limit;
      if (count == capacity) {
        size_t cap = capacity ? capacity * 2 : 16;
        void *p = realloc(annotations, cap * sizeof *annotations);
        if (!p) goto memory;
        annotations = (mulib_vizigen_annotation_ *)p; capacity = cap;
      }
      annotations[count++] = a;
    } else { ++i; continue; }
    /* Mask rather than delete: source spans and line/column remain exact. */
    for (j = start; j < i; ++j) {
      if (!source[j]) goto syntax;
      if (source[j] != '\n' && source[j] != '\r') clean[j] = ' ';
    }
  }
  m = mulib_asdl_parse(clean, length, options, error);
  if (!m) goto done;
  if (count) {
    rules = (mulib_vizigen_rule_t *)calloc(count, sizeof *rules);
    if (!rules) goto memory;
  }
  for (i = 0; i < count; ++i) {
    mulib_asdl_span_t span = {0, 0};
    start = annotations[i].offset;
    for (j = 0; j < mulib_asdl_type_count(m); ++j) {
      const mulib_asdl_type_t *t = mulib_asdl_type(m, j);
      if (++work > MULIB_VIZIGEN_WORK_) goto limit;
      if (t->kind == MULIB_ASDL_PRODUCT && start >= t->span.offset &&
          start - t->span.offset < t->span.length) {
        rules[i].production = t->name; span = t->span;
      }
      for (k = 0; k < t->constructor_count; ++k) {
        const mulib_asdl_constructor_t *c = &t->constructors[k];
        if (++work > MULIB_VIZIGEN_WORK_) goto limit;
        if (start >= c->span.offset && start - c->span.offset < c->span.length) {
          rules[i].production = c->name; span = c->span;
        }
      }
    }
    if (!rules[i].production || annotations[i].length > span.offset + span.length - start) goto syntax;
    /* Only suffix actions: annotations embedded in fields/names are errors. */
    for (j = start; j < span.offset + span.length; ++j) {
      if (++work > MULIB_VIZIGEN_WORK_) goto limit;
      if (!mulib_vizigen_space_((unsigned char)clean[j])) goto syntax;
    }
    rules[i].kind = annotations[i].kind; rules[i].action = annotations[i].action;
  }
  ok = mulib_vizigen_generate(m, rules, count, out, error); goto done;
syntax:
  mulib_vizigen_fail_(error, MULIB_ASDL_SYNTAX, source, start, "invalid action suffix or comment"); goto done;
limit:
  mulib_vizigen_fail_(error, MULIB_ASDL_LIMIT, source, start, "annotation count/work limit"); goto done;
memory:
  mulib_vizigen_fail_(error, MULIB_ASDL_MEMORY, NULL, 0, "out of memory");
done:
  free(clean); free(annotations); free(rules); mulib_asdl_destroy(m); return ok;
}
int mulib_vizigen_runtime_init(mulib_vizigen_runtime_t *out, const mulib_vizigen_program_t *p,
    const mulib_vizigen_action_t *actions, size_t count) {
  mulib_vizigen_runtime_t rt;
  mulib_binfilereader_t r;
  mulib_vizigen_rule_t rule;
  size_t i, j, work = 0;
  if (!out || !p || (!actions && count) || count > MULIB_VIZIGEN_COUNT_ ||
      !mulib_vizigen_open(&rt.program, p->bytes.data, p->bytes.size)) return 0;
  for (i = 0; i < count; ++i) {
    if (!mulib_vizigen_id_(actions[i].name) || !actions[i].call) return 0;
    for (j = 0; j < i; ++j) {
      if (++work > MULIB_VIZIGEN_WORK_ || !strcmp(actions[i].name, actions[j].name)) return 0;
    }
  }
  mulib_binfilereader_init(&r, p->bytes.data, p->bytes.size); r.position = 16;
  while (r.position < r.file.size) {
    if (!mulib_vizigen_record_(&r, &rule)) return 0;
    for (i = 0; i < count; ++i) {
      if (++work > MULIB_VIZIGEN_WORK_) return 0;
      if (!strcmp(rule.action, actions[i].name)) break;
    }
    if (i == count) return 0;
  }
  rt.actions = actions; rt.count = count; *out = rt; return 1;
}
int mulib_vizigen_dispatch(mulib_vizigen_runtime_t *rt, const mulib_event_t *event) {
  mulib_binfilereader_t r;
  mulib_vizigen_rule_t rule;
  size_t i;
  int result;
  if (!rt || !event || !event->node || !event->node->production ||
      (event->kind != MULIB_EVENT_ENTER && event->kind != MULIB_EVENT_LEAVE)) return -1;
  /* Runtime/table and bytes must remain immutable after successful init. */
  if (!mulib_binfilereader_init(&r, rt->program.bytes.data, rt->program.bytes.size) ||
      !mulib_binfilereader_seek(&r, 16)) return -1;
  while (r.position < r.file.size) {
    if (!mulib_vizigen_record_(&r, &rule)) return -1;
    if (event->kind != rule.kind || strcmp(event->node->production, rule.production)) continue;
    for (i = 0; i < rt->count; ++i)
      if (!strcmp(rule.action, rt->actions[i].name)) {
        result = rt->actions[i].call(event, rt->actions[i].user);
        if (result <= 0) return result < 0 ? -1 : 0;
        break;
      }
    if (i == rt->count) return -1;
  }
  return 1;
}
static int mulib_vizigen_event_(const mulib_event_t *event, void *user) {
  return mulib_vizigen_dispatch((mulib_vizigen_runtime_t *)user, event);
}
mulib_evtlistener_t mulib_vizigen_listener(mulib_vizigen_runtime_t *rt) {
  mulib_evtlistener_t listener = {mulib_vizigen_event_, rt}; return listener;
}
int mulib_vizigen_walk(mulib_vizigen_runtime_t *rt, const mulib_ast_t *root) {
  mulib_evtlistener_t listener;
  if (!rt) return -1;
  listener = mulib_vizigen_listener(rt); return mulib_walker_walk(root, &listener);
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
