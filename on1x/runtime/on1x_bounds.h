#ifndef ON1X_BOUNDS_H
#define ON1X_BOUNDS_H

#include <stdbool.h>
#include <stdint.h>

/* Shared by the C11 reference runtime and the compiler-side MEMGraph proof.
   Kept private: emitted programs still call the existing public ABI. */
static inline bool on1x_index_in_bounds(int64_t index, int64_t length) {
    return length >= 0 && index >= 0 && index < length;
}

static inline bool on1x_slice_in_bounds(int64_t lo, int64_t hi, int64_t length) {
    return length >= 0 && lo >= 0 && hi >= lo && hi <= length;
}

/* The proof evaluates a small subset of checked signed arithmetic using the
   same operations as the runtime. Failure does not return a usable result. */
static inline bool on1x_try_add_i64(int64_t a, int64_t b, int64_t *out) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(a, b, out);
#else
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return false;
    *out = a + b;
    return true;
#endif
}

static inline bool on1x_try_sub_i64(int64_t a, int64_t b, int64_t *out) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_sub_overflow(a, b, out);
#else
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return false;
    *out = a - b;
    return true;
#endif
}

static inline bool on1x_try_mul_i64(int64_t a, int64_t b, int64_t *out) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(a, b, out);
#else
    if ((a > 0 && b > 0 && a > INT64_MAX / b) ||
        (a > 0 && b < 0 && b < INT64_MIN / a) ||
        (a < 0 && b > 0 && a < INT64_MIN / b) ||
        (a < 0 && b < 0 && a < INT64_MAX / b)) return false;
    *out = a * b;
    return true;
#endif
}

static inline bool on1x_valid_divisor_i64(int64_t a, int64_t b) {
    return b != 0 && (a != INT64_MIN || b != -1);
}

#endif
