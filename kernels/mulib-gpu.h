/** @file mulib-gpu.h
 * @brief GPU binary-profile and virtual-ISA text emission contract.
 * Define MULIB_IMPL_GPU and UNIT's dependency closure. No device runtime,
 * kernel launch or hardware discovery is performed.
 */
#ifndef MULIB_GPU_H
#define MULIB_GPU_H
#include "mulib-unit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_unit_t mulib_gpu_t;
/** Initialize amdgpu (gfx600) or nvptx (virtual ISA). Success is 1. */
mulib_archml_result mulib_gpu_init (mulib_gpu_t *, const char *, mulib_archml_error_t *);
/** Load an owned future/custom GPU profile. Zero initialize first. */
mulib_archml_result mulib_gpu_parse (mulib_gpu_t *, const char *, size_t,
    const mulib_archml_options_t *, mulib_archml_error_t *);
/** Release and reset. NULL/repeated free permitted. */
void mulib_gpu_free (mulib_gpu_t *);
/** Shared queries/codecs. Text-only operations return MISMATCH from codecs. */
#define mulib_gpu_document mulib_unit_document
#define mulib_gpu_find mulib_unit_find
#define mulib_gpu_register_info mulib_unit_register_info
#define mulib_gpu_enabled mulib_unit_enabled
#define mulib_gpu_encode mulib_unit_encode
#define mulib_gpu_decode mulib_unit_decode
#define mulib_gpu_emit mulib_unit_emit
/** Render an operation's assembly template, substituting named operands in
 * profile order. Only virtual-register value operands are supported: names
 * match %[A-Za-z_$][A-Za-z0-9_$]*, at most 128 bytes. No immediate expressions.
 * Appends a newline, no NUL; emitter order is irrelevant. Max rendered size is
 * 4096 bytes, max operands MULIB_ARCHML_MAX_OPERANDS. Failure is transactional.
 * Caller supplies declarations/module directives and checks operand types. */
mulib_archml_result mulib_gpu_emit_text (const mulib_gpu_t *, mulib_emitter_t *,
    const char *operation, const char *const *operands, size_t,
    mulib_archml_error_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GPU) && !defined(MULIB_GPU_IMPLEMENTED)
#define MULIB_GPU_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_archml_result mulib_gpu_init (mulib_gpu_t *c, const char *name, mulib_archml_error_t *e)
{ return mulib_unit_init (c, MULIB_UNIT_GPU, name, e); }
mulib_archml_result mulib_gpu_parse (mulib_gpu_t *c, const char *s, size_t n,
    const mulib_archml_options_t *o, mulib_archml_error_t *e)
{ return mulib_unit_parse (c, MULIB_UNIT_GPU, s, n, o, e); }
void mulib_gpu_free (mulib_gpu_t *c) { mulib_unit_free (c); }
static mulib_archml_result mulib_gpu_error_ (mulib_archml_error_t *e,
    mulib_archml_result rc, const char *message)
{
  if (e) { e->code = rc; e->offset = 0; e->line = e->column = 1; e->message = message; }
  return rc;
}
static int mulib_gpu_letter_ (unsigned char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$'; }
static int mulib_gpu_register_ (const char *s)
{
  size_t n;
  if (!s || s[0] != '%' || !mulib_gpu_letter_ ((unsigned char)s[1])) return 0;
  for (n = 2; s[n]; ++n)
    if (n >= 128 || (!mulib_gpu_letter_ ((unsigned char)s[n]) && (s[n] < '0' || s[n] > '9'))) return 0;
  return 1;
}
mulib_archml_result mulib_gpu_emit_text (const mulib_gpu_t *c, mulib_emitter_t *image,
    const char *name, const char *const *values, size_t count, mulib_archml_error_t *e)
{
  const mulib_archml_node_t *op, *assembly, *emission, *operand;
  const char *names[MULIB_ARCHML_MAX_OPERANDS], *text;
  char bytes[4096];
  size_t n = 0, used = 0;
  if (!c || !c->document || c->family != MULIB_UNIT_GPU || !image || !name
      || image->size > image->capacity || (!image->data && image->capacity)
      || (image->order != MULIB_EMITTER_LITTLE && image->order != MULIB_EMITTER_BIG)
      || (!values && count) || count > MULIB_ARCHML_MAX_OPERANDS)
    return mulib_gpu_error_ (e, MULIB_ARCHML_INVALID, "invalid GPU text arguments");
  op = mulib_gpu_find (c, "operation", name);
  assembly = mulib_archml_child (mulib_archml_property (op, "assembly"), 1);
  emission = mulib_archml_child (mulib_archml_property (op, "emission"), 1);
  if (!op || !assembly || !assembly->text || !emission || !emission->text
      || strcmp (emission->text, "text"))
    return mulib_gpu_error_ (e, MULIB_ARCHML_MISMATCH, "operation has no text emission template");
  for (operand = mulib_archml_child (mulib_archml_property (op, "operands"), 1);
       operand; operand = operand->next) {
    const mulib_archml_node_t *kind = mulib_archml_child (mulib_archml_property (operand, "kind"), 1);
    const mulib_archml_node_t *id = mulib_archml_child (operand, 0);
    if (n >= count || !kind || !kind->text || strcmp (kind->text, "value") || !id || !id->text)
      return mulib_gpu_error_ (e, MULIB_ARCHML_INVALID, "unsupported operand kind or count");
    if (!mulib_gpu_register_ (values[n]))
      return mulib_gpu_error_ (e, MULIB_ARCHML_INVALID, "expected a virtual register identifier");
    names[n++] = id->text;
  }
  if (n != count) return mulib_gpu_error_ (e, MULIB_ARCHML_INVALID, "operand count mismatch");
  text = assembly->text;
  while (*text) {
    const char *part = text;
    size_t length = 1;
    if (*text == '{') {
      const char *end = strchr (++text, '}');
      size_t i;
      if (!end) return mulib_gpu_error_ (e, MULIB_ARCHML_SCHEMA, "unterminated template operand");
      for (i = 0; i < count; ++i)
        if (strlen (names[i]) == (size_t)(end - text) && !memcmp (names[i], text, (size_t)(end - text))) break;
      if (i == count) return mulib_gpu_error_ (e, MULIB_ARCHML_SCHEMA, "unknown template operand");
      part = values[i]; length = strlen (part); text = end + 1;
    } else {
      if (*text == '}') return mulib_gpu_error_ (e, MULIB_ARCHML_SCHEMA, "unmatched template brace");
      ++text;
    }
    if (length > sizeof (bytes) - 1 - used)
      return mulib_gpu_error_ (e, MULIB_ARCHML_LIMIT, "text instruction exceeds 4096 bytes");
    memcpy (bytes + used, part, length); used += length;
  }
  bytes[used++] = '\n';
  if (used > image->capacity - image->size)
    return mulib_gpu_error_ (e, MULIB_ARCHML_LIMIT, "image capacity exceeded");
  (void)mulib_emitter_bytes (image, bytes, used);
  return mulib_gpu_error_ (e, MULIB_ARCHML_OK, NULL);
}
#ifdef __cplusplus
}
#endif
#endif
