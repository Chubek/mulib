#ifndef ON1X_STDLIB_ELVI_RUNTIME_H
#define ON1X_STDLIB_ELVI_RUNTIME_H

#include "mulib-elvi-scrape.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_ELVI_SOURCE_LIMIT 65536u
#define ON1X_ELVI_QUERY_LIMIT 2048u
#define ON1X_ELVI_BODY_LIMIT 2097152u
#define ON1X_ELVI_URL_CAPACITY 8192u
#define ON1X_ELVI_PATH_CAPACITY 4096u
#define ON1X_ELVI_RESULT_LIMIT 64u

/* Portable native companion, not a foreign import contract. Programs are
 * caller-owned and may be used only as immutable successful compile outputs.
 * Inputs and output objects must not overlap. 1 = success, 0 = failure. */
int on1x_elvi_compile(const char *source, size_t length,
    mulib_elvi_scrape_program_t *out, mulib_elvi_scrape_error_t *error);

/* Trusted host only. Opens exactly the independently approved absolute path.
 * Approval must account for the actual filesystem object (including aliases,
 * symlinks and replacement). A host with a secure preopened-file policy
 * should read authorized bytes itself and call on1x_elvi_compile instead. */
int on1x_elvi_load(const char *path, const char *approved_path,
    mulib_elvi_scrape_program_t *out, mulib_elvi_scrape_error_t *error);

/* Stage descriptions do not authorize I/O. For file and IPC arguments only.
 * NULL permits literal paths but does not supply a HOME. */
int on1x_elvi_resolve_stage(const mulib_elvi_stage_t *stage,
    const char *approved_home, char *out, size_t capacity);

/* Exact match against independently supplied canonical (lowercase) host.
 * This is only a lexical check; it does not authorize a resolved address. */
int on1x_elvi_host_matches(const mulib_elvi_scrape_program_t *program,
    const char *approved_host);

int on1x_elvi_render_search(const mulib_elvi_scrape_program_t *program,
    const char *query, unsigned limit, char *out, size_t capacity);
int on1x_elvi_extract_search(const mulib_elvi_scrape_program_t *program,
    const char *body, size_t body_length, mulib_elvi_scrape_result_t *results,
    size_t capacity, size_t *count);

/* Synchronous native search: explicit authorization and injected transport.
 * The transport is trusted to enforce destination/TLS checks and finite I/O,
 * and to never redirect/retry silently. The result count and array change only
 * on success; optional HTTP status is set if the kernel obtained a response. */
int on1x_elvi_search(const mulib_elvi_scrape_program_t *program,
    const char *approved_host, const mulib_fetch_client_t *client,
    const char *query, unsigned limit, mulib_elvi_scrape_result_t *results,
    size_t capacity, size_t *count, unsigned *http_status);

#ifdef __cplusplus
}
#endif
#endif
