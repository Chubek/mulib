/** @file mulib-memtest.h
 * @brief Destructive volatile RAM pattern tests on explicitly owned word regions.
 * Define MULIB_IMPL_MEMTEST once. Never scans a firmware map automatically.
 */
#ifndef MULIB_MEMTEST_H
#define MULIB_MEMTEST_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint64_t reads, writes, errors; size_t first_word; uint64_t expected, actual; } mulib_memtest_result_t;
typedef struct {
    void *context;
    /** Called between write and verify; return nonzero to cancel (status 1). */
    int (*checkpoint)(void *, unsigned pass, unsigned pattern);
    /** Optional hardware cache maintenance/barrier, called before each verify. */
    void (*flush)(void *, volatile uint64_t *, size_t);
} mulib_memtest_hooks_t;
/** @brief Write/verify zero, ones, address-derived, and 64 walking-bit patterns.
 * Returns 0 clean, 1 cancelled, 2 mismatches, -1 invalid. Contents are destroyed.
 * words must be aligned writable RAM, not MMIO, firmware, stacks or page tables.
 */
int mulib_memtest_run(volatile uint64_t *memory, size_t words, unsigned passes,
                     const mulib_memtest_hooks_t *hooks, mulib_memtest_result_t *result);
/** @brief Destructive March C-: w0; up(r0,w1); up(r1,w0);
 * down(r0,w1); down(r1,w0); r0. Detects address/coupling faults beyond fills.
 * Same statuses as run. Hooks run between sweeps (patterns 0..5).
 */
int mulib_memtest_march(volatile uint64_t *memory, size_t words, unsigned passes,
                       const mulib_memtest_hooks_t *hooks, mulib_memtest_result_t *result);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MEMTEST) && !defined(MULIB_MEMTEST_IMPLEMENTED)
#define MULIB_MEMTEST_IMPLEMENTED
int mulib_memtest_march(volatile uint64_t *m, size_t n, unsigned passes,
                       const mulib_memtest_hooks_t *h, mulib_memtest_result_t *r) {
    unsigned pass, sweep; size_t k, i; uint64_t expected, actual;
    mulib_memtest_result_t zero = {0, 0, 0, SIZE_MAX, 0, 0};
    if (!r) return -1;
    *r = zero;
    if ((!m && n) || !passes || (n && (uintptr_t)m % sizeof(uint64_t)) ||
        n > SIZE_MAX / sizeof(uint64_t) || (uint64_t)n > UINT64_MAX / 5 / passes) return -1;
    for (pass = 0; pass < passes; ++pass) for (sweep = 0; sweep < 6; ++sweep) {
        expected = (sweep == 2 || sweep == 4) ? UINT64_MAX : 0;
        for (k = 0; k < n; ++k) {
            i = (sweep == 3 || sweep == 4) ? n - 1 - k : k;
            if (sweep) {
                actual = m[i]; ++r->reads;
                if (actual != expected) {
                    if (!r->errors) { r->first_word = i; r->expected = expected; r->actual = actual; }
                    ++r->errors;
                }
            }
            if (sweep < 5) { m[i] = sweep ? ~expected : 0; ++r->writes; }
        }
        if (h && h->flush) h->flush(h->context, m, n);
        if (h && h->checkpoint && h->checkpoint(h->context, pass, sweep)) return 1;
    }
    return r->errors ? 2 : 0;
}
static uint64_t mulib_memtest_pattern_(size_t i, unsigned p, unsigned pass) {
    if (p == 0) return 0;
    if (p == 1) return UINT64_MAX;
    if (p == 2) return ((uint64_t)i * UINT64_C(0x9e3779b97f4a7c15)) ^ pass;
    return UINT64_C(1) << (p - 3);
}
int mulib_memtest_run(volatile uint64_t *m, size_t n, unsigned passes,
                     const mulib_memtest_hooks_t *h, mulib_memtest_result_t *r) {
    size_t i; unsigned pass, p; uint64_t expected, actual;
    mulib_memtest_result_t zero = {0, 0, 0, SIZE_MAX, 0, 0};
    if (!r) return -1;
    *r = zero;
    if ((!m && n) || !passes || (n && (uintptr_t)m % sizeof(uint64_t)) ||
        n > SIZE_MAX / sizeof(uint64_t) || (uint64_t)n > UINT64_MAX / 67 / passes) return -1;
    for (pass = 0; pass < passes; ++pass) for (p = 0; p < 67; ++p) {
        for (i = 0; i < n; ++i) { m[i] = mulib_memtest_pattern_(i, p, pass); ++r->writes; }
        if (h && h->flush) h->flush(h->context, m, n);
        if (h && h->checkpoint && h->checkpoint(h->context, pass, p)) return 1;
        for (i = 0; i < n; ++i) {
            expected = mulib_memtest_pattern_(i, p, pass); actual = m[i]; ++r->reads;
            if (actual != expected) {
                if (!r->errors) { r->first_word = i; r->expected = expected; r->actual = actual; }
                ++r->errors;
            }
        }
    }
    return r->errors ? 2 : 0;
}
#endif
