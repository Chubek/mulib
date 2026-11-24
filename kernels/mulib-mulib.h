/** @file mulib-mulib.h
 * @brief Small core umbrella and library version; optional modules stay opt-in.
 * Define MULIB_IMPL_MULIB for version(), and BUFFER/STRBUF switches if used.
 * Does not enable any dependency implementation implicitly. */
#ifndef MULIB_MULIB_H
#define MULIB_MULIB_H
#include "mulib-buffer.h"
#include "mulib-strbuf.h"
#define MULIB_VERSION_MAJOR 0
#define MULIB_VERSION_MINOR 1
#define MULIB_VERSION_PATCH 0
#define MULIB_VERSION_STRING "0.1.0"
#ifdef __cplusplus
extern "C"
{
#endif
  const char *mulib_version (void);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MULIB) && !defined(MULIB_MULIB_IMPLEMENTED)
#define MULIB_MULIB_IMPLEMENTED
const char *
mulib_version (void)
{
  return MULIB_VERSION_STRING;
}
#endif
