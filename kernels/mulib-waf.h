/** @file mulib-waf.h
 * @brief Fail-closed request size/method/path policy, not a signature WAF.
 * Define MULIB_IMPL_WAF and SECURITY. Parsed HTTP model is borrowed. */
#ifndef MULIB_WAF_H
#define MULIB_WAF_H
#include "mulib-http.h"
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_waf
  {
    size_t max_target, max_body;
    const char *const *methods;
    size_t method_count;
  } mulib_waf_t;
  /** Only strict origin paths with optional query; methods are exact allowlist.
   * Returns 1 allowed, 0 denied/invalid. Does not inspect query/body
   * signatures. */
  int mulib_waf_check (const mulib_waf_t *policy,
                       const mulib_http_message_t *request);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAF) && !defined(MULIB_WAF_IMPLEMENTED)
#define MULIB_WAF_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
int
mulib_waf_check (const mulib_waf_t *p, const mulib_http_message_t *r)
{
  size_t n, path;
  char *tmp;
  int allowed = 0;
  if (!p || !r || r->is_response || !r->method || !r->target || !p->methods
      || !p->method_count || !p->max_target || r->body_length > p->max_body)
    return 0;
  n = strlen (r->target);
  if (n > p->max_target)
    return 0;
  for (size_t i = 0; i < p->method_count; ++i)
    if (p->methods[i] && !strcmp (p->methods[i], r->method))
      allowed = 1;
  if (!allowed || strchr (r->target, '#'))
    return 0;
  path = strcspn (r->target, "?");
  tmp = (char *)malloc (path + 1);
  if (!tmp)
    return 0;
  memcpy (tmp, r->target, path);
  tmp[path] = 0;
  allowed = mulib_security_path (tmp, tmp, path + 1);
  free (tmp);
  return allowed;
}
#endif
