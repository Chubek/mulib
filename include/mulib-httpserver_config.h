/** @file mulib-httpserver_config.h
 * @brief Transport-independent HTTP server resource configuration.
 * Define MULIB_IMPL_HTTPSERVER_CONFIG. No listener creation. */
#ifndef MULIB_HTTPSERVER_CONFIG_H
#define MULIB_HTTPSERVER_CONFIG_H
#include "mulib-http.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_httpserver_config
  {
    mulib_http_options_t limits;
    size_t max_buffer_bytes, max_connections;
    unsigned idle_timeout_ms;
  } mulib_httpserver_config_t;
  /** Return explicit HTTP, buffer, connection and idle-time limits. */
  mulib_httpserver_config_t mulib_httpserver_config_default (void);
  /** Validate explicit nonzero limits, representable header storage and total
   * per-connection buffer budget. The buffer must fit headers plus decoded body;
   * chunk framing and pipelined data may require additional space. Flags must be
   * zero (EOF/response context are per-message, not server-wide). This function
   * does not allocate resources or enforce deadlines/connection limits. */
  int
  mulib_httpserver_config_validate (const mulib_httpserver_config_t *config);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTPSERVER_CONFIG)                                      \
    && !defined(MULIB_HTTPSERVER_CONFIG_IMPLEMENTED)
#define MULIB_HTTPSERVER_CONFIG_IMPLEMENTED
#include <stdint.h>
mulib_httpserver_config_t
mulib_httpserver_config_default (void)
{
  mulib_httpserver_config_t c = {
    { 32768, 100, 16U * 1024U * 1024U, 0 }, 17U * 1024U * 1024U, 1024, 30000
  };
  return c;
}
int
mulib_httpserver_config_validate (const mulib_httpserver_config_t *c)
{
  return c && c->limits.max_header_bytes && c->limits.max_headers
         && c->limits.max_body_bytes
         && c->limits.max_headers <= SIZE_MAX / sizeof (mulib_http_header_t)
         && c->limits.max_body_bytes <= SIZE_MAX - c->limits.max_header_bytes
         && c->max_buffer_bytes
                >= c->limits.max_header_bytes + c->limits.max_body_bytes
         && c->max_connections
         && c->max_connections <= SIZE_MAX / c->max_buffer_bytes
         && c->idle_timeout_ms && !c->limits.flags;
}
#endif
