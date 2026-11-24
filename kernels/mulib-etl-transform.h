/* MuLIB: Composable pull-based ETL record filtering and transformation callbacks. C11
 * single-header; define MULIB_IMPL_ETL_TRANSFORM once. */
#ifndef MULIB_ETL_TRANSFORM_H
#define MULIB_ETL_TRANSFORM_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-record.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_etl_transform_fn) (const mulib_record_t *input,
                                         mulib_record_t *output, void *context);
  /* transform returns 1 output, 0 skip, negative error; output is zero initialized. */
  int mulib_etl_transform_apply (const mulib_record_t *in, mulib_record_t *out,
                                 mulib_etl_transform_fn transform, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ETL_TRANSFORM) && !defined(MULIB_ETL_TRANSFORM_IMPLEMENTED)
#define MULIB_ETL_TRANSFORM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_etl_transform_apply (const mulib_record_t *in, mulib_record_t *out,
                             mulib_etl_transform_fn f, void *ctx)
  {
    if (!in || !out || in == out || !f)
      return -1;
    mulib_record_t result = { 0 };
    int status = f (in, &result, ctx);
    if (status != 1)
      {
        mulib_record_free (&result);
        return status;
      }
    mulib_record_free (out);
    *out = result;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
