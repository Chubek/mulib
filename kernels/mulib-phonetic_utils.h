/** @file mulib-phonetic_utils.h
 * @brief ASCII English Soundex phonetic keys.
 * Define MULIB_IMPL_PHONETIC_UTILS. Not a multilingual pronunciation engine.
 */
#ifndef MULIB_PHONETIC_UTILS_H
#define MULIB_PHONETIC_UTILS_H
#ifdef __cplusplus
extern "C" {
#endif
/** Write a four-character key and NUL. Accept ASCII letters only, nonempty.
 * H/W do not separate equal codes; vowels and Y do. Failure preserves output. */
int mulib_phonetic_soundex(const char *word, char output[5]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PHONETIC_UTILS) && !defined(MULIB_PHONETIC_UTILS_IMPLEMENTED)
#define MULIB_PHONETIC_UTILS_IMPLEMENTED
#include <stddef.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static unsigned char mulib_phonetic_upper_(unsigned char c)
{ return c >= 'a' && c <= 'z' ? (unsigned char)(c - 'a' + 'A') : c; }
static char mulib_phonetic_code_(unsigned char c)
{
  static const char codes[] = "01230120022455012623010202";
  return codes[c - 'A'];
}
int mulib_phonetic_soundex(const char *word, char out[5])
{
  char key[5] = {0}, previous;
  size_t count = 1;
  if (!word || !out || !*word) return 0;
  for (const char *p = word; *p; ++p) {
    unsigned char c = mulib_phonetic_upper_((unsigned char)*p);
    if (c < 'A' || c > 'Z') return 0;
  }
  key[0] = (char)mulib_phonetic_upper_((unsigned char)*word++);
  previous = mulib_phonetic_code_((unsigned char)key[0]);
  for (; *word && count < 4; ++word) {
    unsigned char c = mulib_phonetic_upper_((unsigned char)*word);
    char code = mulib_phonetic_code_(c);
    if (c == 'H' || c == 'W') continue;
    if (code != '0' && code != previous) key[count++] = code;
    previous = code;
  }
  while (count < 4) key[count++] = '0';
  memcpy(out, key, 5); return 1;
}
#ifdef __cplusplus
}
#endif
#endif
