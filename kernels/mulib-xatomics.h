/**
 * @file mulib-xatomics.h
 * @brief Portable sequentially consistent integer and pointer atomics.
 * Define MULIB_IMPL_XATOMICS once. GCC/Clang __atomic or MSVC Interlocked
 * backends; see manual/mulib-xatomics.md. Never access storage directly.
 */
#ifndef MULIB_XATOMICS_H
#define MULIB_XATOMICS_H
#include <stdint.h>
#include <stddef.h>
#define MULIB_XATOMIC_INIT(value) { value }
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_xatomic_u32 { uint32_t value; } mulib_xatomic_u32_t;
/* Explicit alignment also supports 32-bit Windows Interlocked operations. */
#if defined(_MSC_VER)
typedef __declspec(align(8)) struct mulib_xatomic_u64 { uint64_t value; } mulib_xatomic_u64_t;
#elif defined(__GNUC__) || defined(__clang__)
typedef struct __attribute__((aligned(8))) mulib_xatomic_u64 { uint64_t value; } mulib_xatomic_u64_t;
#else
#error "mulib-xatomics requires GCC, Clang or MSVC"
#endif
typedef struct mulib_xatomic_ptr { void *value; } mulib_xatomic_ptr_t;
/** @brief Integer atomic operations. CAS updates *expected on failure;
 * fetch operations return the old value. All operations are seq_cst. */
#define MULIB_XATOMIC_DECLARE_(suffix, type) \
type mulib_xatomic_##suffix##_load (const mulib_xatomic_##suffix##_t *); \
void mulib_xatomic_##suffix##_store (mulib_xatomic_##suffix##_t *, type); \
type mulib_xatomic_##suffix##_exchange (mulib_xatomic_##suffix##_t *, type); \
int mulib_xatomic_##suffix##_compare_exchange (mulib_xatomic_##suffix##_t *, type *expected, type desired); \
type mulib_xatomic_##suffix##_fetch_add (mulib_xatomic_##suffix##_t *, type); \
type mulib_xatomic_##suffix##_fetch_sub (mulib_xatomic_##suffix##_t *, type); \
type mulib_xatomic_##suffix##_fetch_and (mulib_xatomic_##suffix##_t *, type); \
type mulib_xatomic_##suffix##_fetch_or (mulib_xatomic_##suffix##_t *, type); \
type mulib_xatomic_##suffix##_fetch_xor (mulib_xatomic_##suffix##_t *, type);
MULIB_XATOMIC_DECLARE_(u32, uint32_t)
MULIB_XATOMIC_DECLARE_(u64, uint64_t)
#undef MULIB_XATOMIC_DECLARE_
/** Read a pointer atomically. */
void *mulib_xatomic_ptr_load (const mulib_xatomic_ptr_t *);
/** Publish a pointer; pointee ownership is unchanged. */
void mulib_xatomic_ptr_store (mulib_xatomic_ptr_t *, void *);
/** Replace a pointer and return the previous pointer. */
void *mulib_xatomic_ptr_exchange (mulib_xatomic_ptr_t *, void *);
/** Strong CAS; on mismatch replace *expected with the observed pointer. */
int mulib_xatomic_ptr_compare_exchange (mulib_xatomic_ptr_t *, void **expected, void *desired);
/** Sequentially consistent thread fence. */
void mulib_xatomic_fence (void);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_XATOMICS) && !defined(MULIB_XATOMICS_IMPLEMENTED)
#define MULIB_XATOMICS_IMPLEMENTED
#if defined(_MSC_VER)
#include <intrin.h>
/* CAS loops use unsigned arithmetic for defined modulo overflow. */
#define MULIB_XATOMIC_DEFINE_(suffix, type, native, cas) \
type mulib_xatomic_##suffix##_load (const mulib_xatomic_##suffix##_t *a) \
{ return (type)cas ((volatile native *)&a->value, 0, 0); } \
int mulib_xatomic_##suffix##_compare_exchange (mulib_xatomic_##suffix##_t *a, type *e, type d) \
{ type old = (type)cas ((volatile native *)&a->value, (native)d, (native)*e); \
  if (old == *e) return 1; *e = old; return 0; } \
type mulib_xatomic_##suffix##_exchange (mulib_xatomic_##suffix##_t *a, type d) \
{ type old = mulib_xatomic_##suffix##_load (a); \
  while (!mulib_xatomic_##suffix##_compare_exchange (a, &old, d)) {} return old; } \
void mulib_xatomic_##suffix##_store (mulib_xatomic_##suffix##_t *a, type d) \
{ (void)mulib_xatomic_##suffix##_exchange (a, d); } \
type mulib_xatomic_##suffix##_fetch_add (mulib_xatomic_##suffix##_t *a, type d) \
{ type old = mulib_xatomic_##suffix##_load (a); \
  while (!mulib_xatomic_##suffix##_compare_exchange (a, &old, (type)(old + d))) {} return old; } \
type mulib_xatomic_##suffix##_fetch_sub (mulib_xatomic_##suffix##_t *a, type d) \
{ return mulib_xatomic_##suffix##_fetch_add (a, (type)(0 - d)); } \
type mulib_xatomic_##suffix##_fetch_and (mulib_xatomic_##suffix##_t *a, type d) \
{ type old = mulib_xatomic_##suffix##_load (a); \
  while (!mulib_xatomic_##suffix##_compare_exchange (a, &old, (type)(old & d))) {} return old; } \
type mulib_xatomic_##suffix##_fetch_or (mulib_xatomic_##suffix##_t *a, type d) \
{ type old = mulib_xatomic_##suffix##_load (a); \
  while (!mulib_xatomic_##suffix##_compare_exchange (a, &old, (type)(old | d))) {} return old; } \
type mulib_xatomic_##suffix##_fetch_xor (mulib_xatomic_##suffix##_t *a, type d) \
{ type old = mulib_xatomic_##suffix##_load (a); \
  while (!mulib_xatomic_##suffix##_compare_exchange (a, &old, (type)(old ^ d))) {} return old; }
MULIB_XATOMIC_DEFINE_(u32, uint32_t, long, _InterlockedCompareExchange)
MULIB_XATOMIC_DEFINE_(u64, uint64_t, __int64, _InterlockedCompareExchange64)
#undef MULIB_XATOMIC_DEFINE_
void *mulib_xatomic_ptr_load (const mulib_xatomic_ptr_t *a)
{ return _InterlockedCompareExchangePointer ((void *volatile *)&a->value, NULL, NULL); }
void *mulib_xatomic_ptr_exchange (mulib_xatomic_ptr_t *a, void *v)
{ return _InterlockedExchangePointer (&a->value, v); }
void mulib_xatomic_ptr_store (mulib_xatomic_ptr_t *a, void *v)
{ (void)mulib_xatomic_ptr_exchange (a, v); }
int mulib_xatomic_ptr_compare_exchange (mulib_xatomic_ptr_t *a, void **e, void *d)
{ void *old = _InterlockedCompareExchangePointer (&a->value, d, *e);
  if (old == *e) return 1;
  *e = old; return 0; }
void mulib_xatomic_fence (void)
{ volatile long value = 0; (void)_InterlockedExchange (&value, 1); }
#else
#define MULIB_XATOMIC_DEFINE_(suffix, type) \
type mulib_xatomic_##suffix##_load (const mulib_xatomic_##suffix##_t *a) \
{ return __atomic_load_n (&a->value, __ATOMIC_SEQ_CST); } \
void mulib_xatomic_##suffix##_store (mulib_xatomic_##suffix##_t *a, type v) \
{ __atomic_store_n (&a->value, v, __ATOMIC_SEQ_CST); } \
type mulib_xatomic_##suffix##_exchange (mulib_xatomic_##suffix##_t *a, type v) \
{ return __atomic_exchange_n (&a->value, v, __ATOMIC_SEQ_CST); } \
int mulib_xatomic_##suffix##_compare_exchange (mulib_xatomic_##suffix##_t *a, type *e, type d) \
{ return __atomic_compare_exchange_n (&a->value, e, d, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); } \
type mulib_xatomic_##suffix##_fetch_add (mulib_xatomic_##suffix##_t *a, type v) \
{ return __atomic_fetch_add (&a->value, v, __ATOMIC_SEQ_CST); } \
type mulib_xatomic_##suffix##_fetch_sub (mulib_xatomic_##suffix##_t *a, type v) \
{ return __atomic_fetch_sub (&a->value, v, __ATOMIC_SEQ_CST); } \
type mulib_xatomic_##suffix##_fetch_and (mulib_xatomic_##suffix##_t *a, type v) \
{ return __atomic_fetch_and (&a->value, v, __ATOMIC_SEQ_CST); } \
type mulib_xatomic_##suffix##_fetch_or (mulib_xatomic_##suffix##_t *a, type v) \
{ return __atomic_fetch_or (&a->value, v, __ATOMIC_SEQ_CST); } \
type mulib_xatomic_##suffix##_fetch_xor (mulib_xatomic_##suffix##_t *a, type v) \
{ return __atomic_fetch_xor (&a->value, v, __ATOMIC_SEQ_CST); }
MULIB_XATOMIC_DEFINE_(u32, uint32_t)
MULIB_XATOMIC_DEFINE_(u64, uint64_t)
#undef MULIB_XATOMIC_DEFINE_
void *mulib_xatomic_ptr_load (const mulib_xatomic_ptr_t *a)
{ return __atomic_load_n (&a->value, __ATOMIC_SEQ_CST); }
void mulib_xatomic_ptr_store (mulib_xatomic_ptr_t *a, void *v)
{ __atomic_store_n (&a->value, v, __ATOMIC_SEQ_CST); }
void *mulib_xatomic_ptr_exchange (mulib_xatomic_ptr_t *a, void *v)
{ return __atomic_exchange_n (&a->value, v, __ATOMIC_SEQ_CST); }
int mulib_xatomic_ptr_compare_exchange (mulib_xatomic_ptr_t *a, void **e, void *d)
{ return __atomic_compare_exchange_n (&a->value, e, d, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }
void mulib_xatomic_fence (void) { __atomic_thread_fence (__ATOMIC_SEQ_CST); }
#endif
#endif
