/** @file mulib-objfilestub.h
 * @brief Emit an x86-64 absolute indirect-jump thunk as data, never execute
 * it. Define MULIB_IMPL_OBJFILESTUB plus BUFFER and BYCUTILS. */
#ifndef MULIB_OBJFILESTUB_H
#define MULIB_OBJFILESTUB_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append FF 25 00 00 00 00 followed by LE64 destination (14 bytes).
   * Requires long mode when executed by caller. 1 success, 0 failure
   * unchanged. */
  int mulib_objfilestub_x64_jump (mulib_buffer_t *out, uint64_t destination);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILESTUB) && !defined(MULIB_OBJFILESTUB_IMPLEMENTED)
#define MULIB_OBJFILESTUB_IMPLEMENTED
#include "mulib-bycutils.h"
int
mulib_objfilestub_x64_jump (mulib_buffer_t *b, uint64_t address)
{
  unsigned char p[14] = { 0xff, 0x25, 0, 0, 0, 0 };
  mulib_bycutils_store (p, 14, 6, 8, address);
  return mulib_buffer_append (b, p, 14);
}
#endif
