#include "runtime.h"
#include <string.h>

int on1x_elvi_compile(const char *source, size_t length,
    mulib_elvi_scrape_program_t *out, mulib_elvi_scrape_error_t *error)
{
  return mulib_elvi_scrape_compile(source, length, out, error);
}

int on1x_elvi_load(const char *path, const char *approved_path,
    mulib_elvi_scrape_program_t *out, mulib_elvi_scrape_error_t *error)
{
  size_t n = 0;
  if (path && approved_path && out && path[0] == '/' && path[1]) {
    while (n < ON1X_ELVI_PATH_CAPACITY && path[n] &&
           (unsigned char)path[n] >= 32 && (unsigned char)path[n] != 127 &&
           path[n] == approved_path[n])
      ++n;
    if (n < ON1X_ELVI_PATH_CAPACITY && !path[n] && !approved_path[n])
      return mulib_elvi_scrape_load(path, out, error);
  }
  if (error)
    *error = (mulib_elvi_scrape_error_t){0, "profile path not approved"};
  return 0;
}

int on1x_elvi_resolve_stage(const mulib_elvi_stage_t *stage,
    const char *approved_home, char *out, size_t capacity)
{
  if (!stage || !stage->line ||
      !memchr(stage->argument, 0, sizeof stage->argument))
    return 0;
  switch (stage->kind) {
  case MULIB_ELVI_STAGE_COOKIE_FILE:
  case MULIB_ELVI_STAGE_LUA:
  case MULIB_ELVI_STAGE_IPC:
    if (capacity > ON1X_ELVI_PATH_CAPACITY)
      capacity = ON1X_ELVI_PATH_CAPACITY;
    return mulib_elvi_script_resolve(stage->argument, approved_home,
                                     out, capacity);
  default:
    return 0;
  }
}

int on1x_elvi_host_matches(const mulib_elvi_scrape_program_t *program,
    const char *approved_host)
{
  return program && approved_host && program->host[0] &&
      memchr(program->host, 0, sizeof program->host) &&
      !strcmp(program->host, approved_host);
}

int on1x_elvi_render_search(const mulib_elvi_scrape_program_t *program,
    const char *query, unsigned limit, char *out, size_t capacity)
{
  return mulib_elvi_scrape_url(program, query, limit, out, capacity);
}

int on1x_elvi_extract_search(const mulib_elvi_scrape_program_t *program,
    const char *body, size_t body_length, mulib_elvi_scrape_result_t *results,
    size_t capacity, size_t *count)
{
  return mulib_elvi_scrape_parse(program, body, body_length, results, capacity,
                                count);
}

int on1x_elvi_search(const mulib_elvi_scrape_program_t *program,
    const char *approved_host, const mulib_fetch_client_t *client,
    const char *query, unsigned limit, mulib_elvi_scrape_result_t *results,
    size_t capacity, size_t *count, unsigned *http_status)
{
  return on1x_elvi_host_matches(program, approved_host) &&
      mulib_elvi_scrape_search(program, client, query, limit, results, capacity,
                               count, http_status);
}
