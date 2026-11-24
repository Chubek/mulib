/** @file mulib-hamming_distance.h
 * @brief Equal-length byte and bit Hamming distances.
 * Define MULIB_IMPL_HAMMING_DISTANCE.
 */
#ifndef MULIB_HAMMING_DISTANCE_H
#define MULIB_HAMMING_DISTANCE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Count differing bytes; SIZE_MAX for NULL with nonzero length. */
size_t mulib_hamming_distance(const void *a, const void *b, size_t length);
/** Count differing bits; SIZE_MAX on invalid input or count overflow. */
size_t mulib_hamming_bits(const void *a, const void *b, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HAMMING_DISTANCE) && !defined(MULIB_HAMMING_DISTANCE_IMPLEMENTED)
#define MULIB_HAMMING_DISTANCE_IMPLEMENTED
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_hamming_distance(const void *a, const void *b, size_t n)
{
  const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
  size_t count = 0;
  if ((!a || !b) && n) return SIZE_MAX;
  for (size_t i = 0; i < n; ++i) count += x[i] != y[i];
  return count;
}
size_t mulib_hamming_bits(const void *a, const void *b, size_t n)
{
  const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
  size_t count = 0;
  if ((!a || !b) && n) return SIZE_MAX;
  for (size_t i = 0; i < n; ++i) {
    unsigned int v = x[i] ^ y[i];
    while (v) { if (count == SIZE_MAX) return SIZE_MAX; ++count; v &= v - 1; }
  }
  return count;
}
#ifdef __cplusplus
}
#endif
#endif
