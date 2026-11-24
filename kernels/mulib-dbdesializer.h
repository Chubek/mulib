/** @file mulib-dbdesializer.h
 * @brief Exact owned value decoding, retaining the historical filename.
 * Define MULIB_IMPL_DBDESIALIZER and all codec dependencies.
 * JSON input is bounded to 16 MiB like the binary codecs. NULL means failure.
 */
#ifndef MULIB_DBDESIALIZER_H
#define MULIB_DBDESIALIZER_H
#include "mulib-dbserializer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_json_t *mulib_dbdeserializer_read (mulib_dbformat format,
                                           const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBDESIALIZER) && !defined(MULIB_DBDESIALIZER_IMPLEMENTED)
#define MULIB_DBDESIALIZER_IMPLEMENTED
mulib_json_t *
mulib_dbdeserializer_read (mulib_dbformat f, const void *p, size_t n)
{
  if (!p || n > 16U * 1024U * 1024U)
    return NULL;
  switch (f)
    {
    case MULIB_DBFORMAT_JSON:
      return mulib_json_parse ((const char *)p, n, NULL);
    case MULIB_DBFORMAT_CBOR:
      return mulib_cbor_decode (p, n);
    case MULIB_DBFORMAT_MESSAGEPACK:
      return mulib_messagepack_decode (p, n);
    case MULIB_DBFORMAT_BSON:
      return mulib_bson_decode (p, n);
    }
  return NULL;
}
#endif
