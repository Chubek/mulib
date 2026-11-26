#ifndef BASIS_HTTP_PROBE_CONTEXT_H
#define BASIS_HTTP_PROBE_CONTEXT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  BASIS_HTTP_PROBE_KEY_MAX = 63,
  BASIS_HTTP_PROBE_CONTEXT_MAX = 7 + 3 * BASIS_HTTP_PROBE_KEY_MAX
};

/* Names for lookup in a trusted application registry, never authority.
 * No paths, source programs, URLs, secrets, or callables belong here. */
typedef struct {
  char probe[BASIS_HTTP_PROBE_KEY_MAX + 1];
  char runner[BASIS_HTTP_PROBE_KEY_MAX + 1];
  char context[BASIS_HTTP_PROBE_KEY_MAX + 1];
} basis_http_probe_context;

typedef enum {
  BASIS_HTTP_PROBE_CONTEXT_OK = 0,
  BASIS_HTTP_PROBE_CONTEXT_INVALID = -1,
  BASIS_HTTP_PROBE_CONTEXT_CAPACITY = -2
} basis_http_probe_context_status;

/* Canonical v1: ASCII "PRB", byte 1, three one-byte lengths, then probe,
 * runner, and context in that order. Each key is a case-sensitive ASCII
 * alphanumeric followed by up to 62 ASCII alphanumerics, '_' or '-'.
 * Input and output objects must not overlap. No allocation or I/O.
 * All outputs are unchanged on failure. */
basis_http_probe_context_status
basis_http_probe_context_encode(const basis_http_probe_context *context,
                                unsigned char *output, size_t capacity,
                                size_t *length);

basis_http_probe_context_status
basis_http_probe_context_decode(const unsigned char *input, size_t length,
                                basis_http_probe_context *context);

#ifdef __cplusplus
}
#endif
#endif
