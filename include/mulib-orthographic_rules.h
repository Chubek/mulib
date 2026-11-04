/** @file mulib-orthographic_rules.h
 * @brief Ordered literal prefix/suffix/whole-word orthographic rules.
 * Define MULIB_IMPL_ORTHOGRAPHIC_RULES. Byte-based, case-sensitive, no linguistics inferred.
 */
#ifndef MULIB_ORTHOGRAPHIC_RULES_H
#define MULIB_ORTHOGRAPHIC_RULES_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_orthographic_kind { MULIB_ORTHOGRAPHIC_PREFIX, MULIB_ORTHOGRAPHIC_SUFFIX, MULIB_ORTHOGRAPHIC_WORD } mulib_orthographic_kind_t;
typedef struct mulib_orthographic_rule {
  mulib_orthographic_kind_t kind;
  const char *match, *replacement;
  size_t minimum_stem;
} mulib_orthographic_rule_t;
/** Apply the first matching rule once; otherwise copy word. Caller frees result.
 * NULL on invalid rule/OOM/overflow. Empty matches are rejected. No recursive rules. */
char *mulib_orthographic_apply(const char *word, const mulib_orthographic_rule_t *rules, size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ORTHOGRAPHIC_RULES) && !defined(MULIB_ORTHOGRAPHIC_RULES_IMPLEMENTED)
#define MULIB_ORTHOGRAPHIC_RULES_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
char *mulib_orthographic_apply(const char *word, const mulib_orthographic_rule_t *rules, size_t n)
{
  size_t length, start = 0, removed = 0, added = 0;
  const char *replacement = "";
  char *out;
  if (!word || (!rules && n)) return NULL;
  length = strlen(word);
  for (size_t i = 0; i < n; ++i)
    if (!rules[i].match || !*rules[i].match || !rules[i].replacement
        || rules[i].kind < MULIB_ORTHOGRAPHIC_PREFIX || rules[i].kind > MULIB_ORTHOGRAPHIC_WORD) return NULL;
  for (size_t i = 0; i < n; ++i) {
    size_t m = strlen(rules[i].match), pos;
    if (m > length || length - m < rules[i].minimum_stem) continue;
    if (rules[i].kind == MULIB_ORTHOGRAPHIC_WORD && m != length) continue;
    pos = rules[i].kind == MULIB_ORTHOGRAPHIC_SUFFIX ? length - m : 0;
    if (!memcmp(word + pos, rules[i].match, m)) {
      start = pos; removed = m; replacement = rules[i].replacement; added = strlen(replacement); break;
    }
  }
  if (added >= SIZE_MAX - (length - removed)) return NULL;
  out = (char *)malloc(length - removed + added + 1);
  if (!out) return NULL;
  memcpy(out, word, start); memcpy(out + start, replacement, added);
  memcpy(out + start + added, word + start + removed, length - start - removed + 1);
  return out;
}
#ifdef __cplusplus
}
#endif
#endif
