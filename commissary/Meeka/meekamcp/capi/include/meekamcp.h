/* MeekaMCP C API — build MCP servers from C/C++ on top of the Go library.
 *
 * Build the shared library:
 *   go build -buildmode=c-shared -o libmeekamcp.so ./capi
 *
 * Link with: -lmeekamcp
 *
 * Threading: all functions are safe to call from any thread. Handles are
 * opaque integers; 0 is never a valid handle.
 *
 * Memory: every char* returned by this library must be released with
 * meekamcp_string_free(). Strings returned by user callbacks must be allocated
 * with malloc(3); the library takes ownership and frees them.
 */
#ifndef MEEKAMCP_H
#define MEEKAMCP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long long meekamcp_server_t;

/* Tool callback. args_json is the JSON-encoded argument object. Return a
 * malloc'd NUL-terminated string with the text result, or NULL to signal a
 * tool execution error. */
typedef char* (*meekamcp_tool_fn)(const char* args_json, void* user_data);

/* Resource callback. uri is the concrete URI requested; params_json is the
 * JSON-encoded map of URI-template parameters. Same return convention as
 * meekamcp_tool_fn; NULL maps to a resource-not-found error. */
typedef char* (*meekamcp_resource_fn)(const char* uri, const char* params_json,
                                    void* user_data);

/* Create a server. Returns a handle, or 0 on failure. */
meekamcp_server_t meekamcp_server_new(const char* name, const char* version);

/* Destroy a server and release all resources associated with the handle. */
void meekamcp_server_free(meekamcp_server_t server);

/* Returns the last error message for the handle, or NULL when there is none.
 * Free the result with meekamcp_string_free(). */
char* meekamcp_last_error(meekamcp_server_t server);

/* Free any string returned by this library. */
void meekamcp_string_free(char* str);

/* Register a tool. schema_json is an optional JSON Schema for the input
 * object; pass NULL for a permissive object schema. Returns 0 on success,
 * -1 on error (see meekamcp_last_error). */
int meekamcp_add_tool(meekamcp_server_t server, const char* name,
                    const char* description, const char* schema_json,
                    meekamcp_tool_fn callback, void* user_data);

/* Register a resource. uri may contain {param} RFC 6570 template segments.
 * name, description, and mime_type may be NULL. Returns 0 on success. */
int meekamcp_add_resource(meekamcp_server_t server, const char* uri,
                        const char* name, const char* description,
                        const char* mime_type, meekamcp_resource_fn callback,
                        void* user_data);

/* Synchronously process one JSON-RPC frame. Returns the malloc'd response
 * frame (free with meekamcp_string_free), or NULL for notifications and
 * errors (check meekamcp_last_error to distinguish). Use this to embed MeekaMCP
 * into an existing event loop or custom transport. */
char* meekamcp_handle_message(meekamcp_server_t server, const char* request_json);

/* Serve newline-delimited JSON-RPC over stdin/stdout. Blocks until EOF or
 * meekamcp_shutdown() is called from another thread. Returns 0 on clean exit. */
int meekamcp_serve_stdio(meekamcp_server_t server);

/* Gracefully drain in-flight requests and stop serving. Returns 0 on
 * success. */
int meekamcp_shutdown(meekamcp_server_t server);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MEEKAMCP_H */
