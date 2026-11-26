#include "script.h"
#include "runtime.h"

static int eligible(const mulib_elvi_scrape_program_t *program,
                    const char *approved_host)
{
  return on1x_elvi_host_matches(program, approved_host) &&
      program->mode == MULIB_ELVI_MODE_SCRAPE &&
      program->artifacts.kind == MULIB_ELVI_STAGE_DISCARD;
}

int on1x_elvi_run(const mulib_elvi_scrape_program_t *program,
    const char *approved_host, const mulib_elvi_script_run_config_t *config)
{
  if (!eligible(program, approved_host))
    return MULIB_SB_DENIED;
  return mulib_elvi_script_run(program, config);
}

int on1x_elvi_run_https(const mulib_elvi_scrape_program_t *program,
    const char *approved_host, const mulib_elvi_script_run_config_t *config,
    const mulib_elvi_scrape_https_t *https, const char *approved_cookie_path)
{
  if (!eligible(program, approved_host))
    return MULIB_SB_DENIED;
  return mulib_elvi_script_run_https(program, config, https, approved_cookie_path);
}
