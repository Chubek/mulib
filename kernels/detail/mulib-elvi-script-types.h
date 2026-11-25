/** @file mulib-elvi-script-types.h
 * @brief Shared request, response and result types for MuLIB ElviScript.
 */
#ifndef MULIB_ELVI_SCRIPT_TYPES_H
#define MULIB_ELVI_SCRIPT_TYPES_H
#include "../mulib-http.h"

#define MULIB_ELVI_SCRIPT_MAX_REQUESTS 8
#define MULIB_ELVI_SCRIPT_MAX_BODY (2u * 1024u * 1024u)
#define MULIB_ELVI_SCRIPT_MAX_RESULTS 64

/* Requests are GET only. URL is absolute, public HTTPS, same exact host as
 * the compiled profile. Logical requests do not carry arbitrary headers. */
typedef struct mulib_elvi_script_request {
  char url[8192];
} mulib_elvi_script_request_t;

/* Successful transport transfers message ownership; a zeroed response is
 * empty. Destroy with mulib_http_message_free(&response.message).
 * Body, headers and trailers are borrowed by the consumer only during its
 * callback. An effective URL obeys the request URL policy. */
typedef struct mulib_elvi_script_response {
  mulib_http_message_t message;
  char effective_url[8192];
} mulib_elvi_script_response_t;

/* Valid UTF-8, NUL-terminated strings. Name and URL are required; optional
 * fields are empty when unavailable. Price is display text, not a number. */
typedef struct mulib_elvi_script_result {
  char name[512], url[2048], price[128], location[256];
} mulib_elvi_script_result_t;

/* All callbacks are synchronous, return 1 on success or 0 on failure and
 * borrow user and their inputs. A rejection must fail the current stage. */
typedef int (*mulib_elvi_script_fetch_fn)(void *user,
    const mulib_elvi_script_request_t *, mulib_elvi_script_response_t *);
typedef int (*mulib_elvi_script_enqueue_fn)(void *user,
    const mulib_elvi_script_request_t *);
typedef int (*mulib_elvi_script_emit_fn)(void *user,
    const mulib_elvi_script_result_t *);
#endif
