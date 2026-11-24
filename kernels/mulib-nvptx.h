/** @file mulib-nvptx.h
 * @brief Owned NVPTX64 virtual-ISA text GPU implementation.
 * Define MULIB_IMPL_NVPTX and GPU's dependency closure. Selected unpredicated
 * integer operations emit PTX text; this API never invents NVIDIA SASS bytes.
 */
#ifndef MULIB_NVPTX_H
#define MULIB_NVPTX_H
#include "mulib-gpu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_gpu_t mulib_nvptx_t;
/** Zero initialize; return 1 on success, 0 on failure/live context. */
int mulib_nvptx_init (mulib_nvptx_t *);
/** Diagnostic initialization; Boolean result. */
int mulib_nvptx_init_ex (mulib_nvptx_t *, mulib_archml_error_t *);
/** Release/reset. NULL/repeated free permitted. */
void mulib_nvptx_free (mulib_nvptx_t *);
/** GPU queries and explicit binary rejection. Text operands are virtual
 * register names; mulib_nvptx_emit appends an operation plus newline.
 * Emission returns MULIB_ARCHML_OK (1) or a negative error. */
#define mulib_nvptx_document mulib_gpu_document
#define mulib_nvptx_find mulib_gpu_find
#define mulib_nvptx_enabled mulib_gpu_enabled
#define mulib_nvptx_encode mulib_gpu_encode
#define mulib_nvptx_decode mulib_gpu_decode
#define mulib_nvptx_emit mulib_gpu_emit_text
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NVPTX) && !defined(MULIB_NVPTX_IMPLEMENTED)
#define MULIB_NVPTX_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_nvptx_init (mulib_nvptx_t *c) { return mulib_nvptx_init_ex (c, NULL); }
int mulib_nvptx_init_ex (mulib_nvptx_t *c, mulib_archml_error_t *e)
{ return mulib_gpu_init (c, "nvptx", e) == MULIB_ARCHML_OK; }
void mulib_nvptx_free (mulib_nvptx_t *c) { mulib_gpu_free (c); }
#ifdef __cplusplus
}
#endif
#endif
