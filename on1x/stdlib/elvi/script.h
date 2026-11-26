#ifndef ON1X_STDLIB_ELVI_SCRIPT_H
#define ON1X_STDLIB_ELVI_SCRIPT_H

#include "mulib-elvi-script.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional Lua/discard/IPC integration for trusted native applications.
 * Operations are synchronous and retain nothing. All program/configuration
 * inputs are borrowed unchanged, and the caller still manages its descriptor.
 * See mulib-elvi-script.h for the kernel's completion and result codes. */
int on1x_elvi_run(const mulib_elvi_scrape_program_t *program,
    const char *approved_host, const mulib_elvi_script_run_config_t *config);
int on1x_elvi_run_https(const mulib_elvi_scrape_program_t *program,
    const char *approved_host, const mulib_elvi_script_run_config_t *config,
    const mulib_elvi_scrape_https_t *https, const char *approved_cookie_path);

#ifdef __cplusplus
}
#endif
#endif
