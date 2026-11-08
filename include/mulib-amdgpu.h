/** @file mulib-amdgpu.h
 * @brief Owned AMDGPU gfx600 wave64 SALU GPU implementation.
 * Define MULIB_IMPL_AMDGPU and GPU's dependency closure. Encodings are the
 * embedded profile's selected scalar forms, not portable across generations.
 */
#ifndef MULIB_AMDGPU_H
#define MULIB_AMDGPU_H
#include "mulib-gpu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_gpu_t mulib_amdgpu_t;
/** Zero initialize; return 1 on success, 0 on failure/live context. */
int mulib_amdgpu_init (mulib_amdgpu_t *);
/** Diagnostic initialization; Boolean result. */
int mulib_amdgpu_init_ex (mulib_amdgpu_t *, mulib_archml_error_t *);
/** Release/reset. NULL/repeated free permitted. */
void mulib_amdgpu_free (mulib_amdgpu_t *);
/** GPU contract aliases; register operands are ordinary s0..s103 IDs.
 * Codecs/emit return MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_amdgpu_document mulib_gpu_document
#define mulib_amdgpu_find mulib_gpu_find
#define mulib_amdgpu_register_info mulib_gpu_register_info
#define mulib_amdgpu_enabled mulib_gpu_enabled
#define mulib_amdgpu_encode mulib_gpu_encode
#define mulib_amdgpu_decode mulib_gpu_decode
#define mulib_amdgpu_emit mulib_gpu_emit
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AMDGPU) && !defined(MULIB_AMDGPU_IMPLEMENTED)
#define MULIB_AMDGPU_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_amdgpu_init (mulib_amdgpu_t *c) { return mulib_amdgpu_init_ex (c, NULL); }
int mulib_amdgpu_init_ex (mulib_amdgpu_t *c, mulib_archml_error_t *e)
{ return mulib_gpu_init (c, "amdgpu", e) == MULIB_ARCHML_OK; }
void mulib_amdgpu_free (mulib_amdgpu_t *c) { mulib_gpu_free (c); }
#ifdef __cplusplus
}
#endif
#endif
