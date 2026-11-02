/** @file mulib-dbserializer.h
 * @brief Explicit JSON/BSON/MessagePack/CBOR serialization of shared values.
 * Define MULIB_IMPL_DBSERIALIZER plus selected codec dependencies. All codecs
 * are linked by this dispatcher; output append is failure-atomic. */
#ifndef MULIB_DBSERIALIZER_H
#define MULIB_DBSERIALIZER_H
#include "mulib-bson.h"
#include "mulib-cbor.h"
#include "mulib-messagepack.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_dbformat
  {
    MULIB_DBFORMAT_JSON,
    MULIB_DBFORMAT_BSON,
    MULIB_DBFORMAT_MESSAGEPACK,
    MULIB_DBFORMAT_CBOR
  } mulib_dbformat;
  int mulib_dbserializer_write (mulib_dbformat format,
                                const mulib_json_t *value, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBSERIALIZER) && !defined(MULIB_DBSERIALIZER_IMPLEMENTED)
#define MULIB_DBSERIALIZER_IMPLEMENTED
int
mulib_dbserializer_write (mulib_dbformat f, const mulib_json_t *v,
                          mulib_buffer_t *out)
{
  if (!out)
    return 0;
  switch (f)
    {
    case MULIB_DBFORMAT_BSON:
      return mulib_bson_encode (v, out);
    case MULIB_DBFORMAT_CBOR:
      return mulib_cbor_encode (v, out);
    case MULIB_DBFORMAT_MESSAGEPACK:
      return mulib_messagepack_encode (v, out);
    case MULIB_DBFORMAT_JSON:
      {
        char *s = mulib_json_stringify (v, 0);
        int ok = s && mulib_buffer_append (out, s, strlen (s));
        free (s);
        return ok;
      }
    }
  return 0;
}
#endif
