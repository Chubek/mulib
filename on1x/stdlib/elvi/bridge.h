#ifndef ON1X_STDLIB_ELVI_BRIDGE_H
#define ON1X_STDLIB_ELVI_BRIDGE_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internal generated-C ABI. Every string is borrowed, immutable and
 * NUL-terminated; replies are immutable On1x text snapshots or literals.
 * Call only on the owning On1x runtime thread. No I/O or callbacks. */
const char *on1x_elvi_compile_v1(const char *source);
const char *on1x_elvi_inspect_v1(const char *source);
const char *on1x_elvi_stage_v1(const char *source, int64_t kind);
const char *on1x_elvi_search_spec_v1(const char *source);
const char *on1x_elvi_render_search_v1(const char *source, const char *query,
    int64_t limit);
const char *on1x_elvi_extract_search_v1(const char *source, const char *body,
    int64_t limit);
const char *on1x_elvi_resolve_stage_v1(int64_t kind, const char *argument,
    int64_t line, const char *home);

#ifdef __cplusplus
}
#endif
#endif
