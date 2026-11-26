#include "bridge.h"
#include "runtime.h"
#include "on1x_runtime.h"
#include <stdlib.h>
#include <string.h>

#ifndef ON1X_ELVI_MALLOC
#define ON1X_ELVI_MALLOC malloc
#endif
#ifndef ON1X_ELVI_FREE
#define ON1X_ELVI_FREE free
#endif

/* E<code><six digits of line><message>, N (absence), or O<success payload>.
 * Variable-length fields inside a record carry six decimal digits each. */
enum { digits = 6, max_hit_wire = digits * 2 + 511 + 2047,
       max_search_wire = 1 + digits + ON1X_ELVI_RESULT_LIMIT * max_hit_wire };
static const char invalid[] = "Ei000000invalid input or limit";
static const char wrong_mode[] = "Em000000operation requires search mode";
static const char resource[] = "Ea000000native allocation failed";
static const char render_failed[] = "Er000000could not render search URL";
static const char extract_failed[] = "Ex000000search extraction failed";
static const char resolve_failed[] = "Es000000stage path did not resolve";
static const char protocol[] = "Ep000000inconsistent native reply";

static size_t bounded_length(const char *s, size_t limit)
{
  size_t n = 0;
  if (!s)
    return limit + 1;
  while (n <= limit && s[n])
    ++n;
  return n;
}

static void write_number(char *out, size_t n)
{
  for (size_t i = digits; i; --i) {
    out[i - 1] = (char)('0' + n % 10);
    n /= 10;
  }
}

static const char *snapshot(char *reply, size_t size)
{
  reply[size] = '\0';
  return on1x_text_slice(reply, 0, (int64_t)size);
}

static const char *compile(const char *source, mulib_elvi_scrape_program_t *p)
{
  mulib_elvi_scrape_error_t error = {0};
  size_t n = bounded_length(source, ON1X_ELVI_SOURCE_LIMIT);
  char reply[2 + digits + sizeof error.message] = {'E', 'c'};
  if (n > ON1X_ELVI_SOURCE_LIMIT)
    return invalid;
  if (on1x_elvi_compile(source, n, p, &error))
    return NULL;
  n = bounded_length(error.message, sizeof error.message - 1);
  if (!n || n >= sizeof error.message || error.line > ON1X_ELVI_SOURCE_LIMIT)
    return protocol;
  write_number(reply + 2, error.line);
  memcpy(reply + 2 + digits, error.message, n);
  return snapshot(reply, 2 + digits + n);
}

static int write_field(char *out, size_t cap, size_t *used,
                       const char *field, size_t field_limit)
{
  size_t n = bounded_length(field, field_limit);
  if (n > field_limit || *used > cap || digits + n > cap - *used)
    return 0;
  write_number(out + *used, n);
  *used += digits;
  memcpy(out + *used, field, n);
  *used += n;
  return 1;
}

static mulib_elvi_stage_kind_t stage_kind(int64_t code)
{
  switch (code) {
  case 1: return MULIB_ELVI_STAGE_COOKIE_FILE;
  case 2: return MULIB_ELVI_STAGE_XPATH;
  case 3: return MULIB_ELVI_STAGE_DISCARD;
  case 4: return MULIB_ELVI_STAGE_LUA;
  case 5: return MULIB_ELVI_STAGE_IPC;
  case 6: return MULIB_ELVI_STAGE_CAPTURE;
  default: return MULIB_ELVI_STAGE_NONE;
  }
}

const char *on1x_elvi_compile_v1(const char *source)
{
  mulib_elvi_scrape_program_t p;
  const char *error = compile(source, &p);
  return error ? error : "O";
}

const char *on1x_elvi_inspect_v1(const char *source)
{
  mulib_elvi_scrape_program_t p;
  const char *error = compile(source, &p);
  char reply[3 + 4 * digits + sizeof p.name + sizeof p.host + sizeof p.home];
  size_t used = 3 + digits;
  if (error)
    return error;
  reply[0] = 'O';
  switch (p.mode) {
  case MULIB_ELVI_MODE_SEARCH:
    reply[1] = 'S';
    if (p.format == MULIB_ELVI_FORMAT_HTML)
      reply[2] = 'H';
    else if (p.format == MULIB_ELVI_FORMAT_JSON)
      reply[2] = 'J';
    else
      return protocol;
    break;
  case MULIB_ELVI_MODE_SCRAPE: reply[1] = 'L'; reply[2] = '-'; break;
  case MULIB_ELVI_MODE_DOCUMENT: reply[1] = 'D'; reply[2] = '-'; break;
  default: return protocol;
  }
  if (!p.max_results || p.max_results > ON1X_ELVI_RESULT_LIMIT)
    return protocol;
  write_number(reply + 3, p.max_results);
  if (!write_field(reply, sizeof reply - 1, &used, p.name, sizeof p.name - 1) ||
      !write_field(reply, sizeof reply - 1, &used, p.host, sizeof p.host - 1) ||
      !write_field(reply, sizeof reply - 1, &used, p.home, sizeof p.home - 1))
    return protocol;
  return snapshot(reply, used);
}

const char *on1x_elvi_stage_v1(const char *source, int64_t code)
{
  mulib_elvi_scrape_program_t p;
  mulib_elvi_stage_kind_t kind = stage_kind(code);
  const mulib_elvi_stage_t *stage;
  const char *error;
  char reply[1 + digits + sizeof p.cookies.argument] = {'O'};
  size_t n;
  if (kind == MULIB_ELVI_STAGE_NONE)
    return invalid;
  error = compile(source, &p);
  if (error)
    return error;
  switch (kind) {
  case MULIB_ELVI_STAGE_COOKIE_FILE: stage = &p.cookies; break;
  case MULIB_ELVI_STAGE_XPATH: stage = &p.xpath; break;
  case MULIB_ELVI_STAGE_DISCARD:
  case MULIB_ELVI_STAGE_CAPTURE: stage = &p.artifacts; break;
  case MULIB_ELVI_STAGE_LUA: stage = &p.logic; break;
  case MULIB_ELVI_STAGE_IPC: stage = &p.forward; break;
  default: return protocol;
  }
  if (stage->kind != kind)
    return "N";
  n = bounded_length(stage->argument, sizeof stage->argument - 1);
  if (!stage->line || stage->line > ON1X_ELVI_SOURCE_LIMIT ||
      n >= sizeof stage->argument)
    return protocol;
  write_number(reply + 1, stage->line);
  memcpy(reply + 1 + digits, stage->argument, n);
  return snapshot(reply, 1 + digits + n);
}

const char *on1x_elvi_search_spec_v1(const char *source)
{
  mulib_elvi_scrape_program_t p;
  const char *error = compile(source, &p);
  char reply[3 + 14 * digits + sizeof p.search_template +
             sizeof p.json_results + sizeof p.json_title + sizeof p.json_url +
             sizeof p.excluded_hosts];
  size_t used = 3 + 2 * digits;
  const char *a, *b, *c;
  size_t a_limit, b_limit, c_limit;
  if (error)
    return error;
  if (p.mode != MULIB_ELVI_MODE_SEARCH)
    return "N";
  reply[0] = 'O';
  if (p.format == MULIB_ELVI_FORMAT_HTML) {
    switch (p.anchor_mode) {
    case MULIB_ELVI_ANCHOR_ANY: reply[1] = 'A'; break;
    case MULIB_ELVI_ANCHOR_CHILD: reply[1] = 'C'; break;
    case MULIB_ELVI_ANCHOR_PARENT: reply[1] = 'P'; break;
    default: return protocol;
    }
    a = p.anchor_tag;
    a_limit = sizeof p.anchor_tag - 1;
    b = p.anchor_class;
    b_limit = sizeof p.anchor_class - 1;
    c = "";
    c_limit = 0;
  } else if (p.format == MULIB_ELVI_FORMAT_JSON) {
    reply[1] = 'J';
    a = p.json_results;
    a_limit = sizeof p.json_results - 1;
    b = p.json_title;
    b_limit = sizeof p.json_title - 1;
    c = p.json_url;
    c_limit = sizeof p.json_url - 1;
  } else {
    return protocol;
  }
  switch (p.rewrite) {
  case MULIB_ELVI_REWRITE_DIRECT: reply[2] = 'D'; break;
  case MULIB_ELVI_REWRITE_GOOGLE: reply[2] = 'G'; break;
  case MULIB_ELVI_REWRITE_RELATIVE: reply[2] = 'R'; break;
  default: return protocol;
  }
  if (!p.max_results || p.max_results > ON1X_ELVI_RESULT_LIMIT ||
      p.excluded_count > sizeof p.excluded_hosts / sizeof p.excluded_hosts[0])
    return protocol;
  write_number(reply + 3, p.max_results);
  write_number(reply + 3 + digits, p.excluded_count);
  if (!write_field(reply, sizeof reply - 1, &used, p.search_template,
                   sizeof p.search_template - 1) ||
      !write_field(reply, sizeof reply - 1, &used, a, a_limit) ||
      !write_field(reply, sizeof reply - 1, &used, b, b_limit) ||
      !write_field(reply, sizeof reply - 1, &used, c, c_limit))
    return protocol;
  for (size_t i = 0; i < p.excluded_count; ++i) {
    if (!write_field(reply, sizeof reply - 1, &used, p.excluded_hosts[i],
                     sizeof p.excluded_hosts[i] - 1))
      return protocol;
  }
  return snapshot(reply, used);
}

const char *on1x_elvi_render_search_v1(const char *source, const char *query,
    int64_t limit)
{
  mulib_elvi_scrape_program_t p;
  const char *error;
  char reply[1 + ON1X_ELVI_URL_CAPACITY] = {'O'};
  if (limit < 1 || limit > ON1X_ELVI_RESULT_LIMIT ||
      bounded_length(query, ON1X_ELVI_QUERY_LIMIT) > ON1X_ELVI_QUERY_LIMIT)
    return invalid;
  error = compile(source, &p);
  if (error)
    return error;
  if (p.mode != MULIB_ELVI_MODE_SEARCH)
    return wrong_mode;
  if (limit > p.max_results)
    return invalid;
  if (!on1x_elvi_render_search(&p, query, (unsigned)limit, reply + 1,
                               sizeof reply - 1))
    return render_failed;
  return snapshot(reply, strlen(reply));
}

const char *on1x_elvi_extract_search_v1(const char *source, const char *body,
    int64_t limit)
{
  mulib_elvi_scrape_program_t p;
  const char *error;
  mulib_elvi_scrape_result_t *hits;
  char reply[max_search_wire + 1] = {'O'};
  size_t count = 0, used = 1 + digits;
  size_t n = bounded_length(body, ON1X_ELVI_BODY_LIMIT);
  if (limit < 1 || limit > ON1X_ELVI_RESULT_LIMIT || n > ON1X_ELVI_BODY_LIMIT)
    return invalid;
  error = compile(source, &p);
  if (error)
    return error;
  if (p.mode != MULIB_ELVI_MODE_SEARCH)
    return wrong_mode;
  if (limit > p.max_results)
    return invalid;
  hits = ON1X_ELVI_MALLOC((size_t)limit * sizeof *hits);
  if (!hits)
    return resource;
  if (!on1x_elvi_extract_search(&p, body, n, hits, (size_t)limit, &count))
    error = extract_failed;
  else if (count > (size_t)limit)
    error = protocol;
  else {
    write_number(reply + 1, count);
    for (size_t i = 0; i < count; ++i) {
      if (!write_field(reply, sizeof reply - 1, &used,
                       hits[i].title, sizeof hits[i].title - 1) ||
          !write_field(reply, sizeof reply - 1, &used,
                       hits[i].url, sizeof hits[i].url - 1)) {
        error = protocol;
        break;
      }
    }
  }
  ON1X_ELVI_FREE(hits);
  /* Runtime text allocation is fatal; release native allocations first. */
  return error ? error : snapshot(reply, used);
}

const char *on1x_elvi_resolve_stage_v1(int64_t code, const char *argument,
    int64_t line, const char *home)
{
  mulib_elvi_stage_t stage = {0};
  char reply[1 + ON1X_ELVI_PATH_CAPACITY] = {'O'};
  size_t n = bounded_length(argument, sizeof stage.argument - 1);
  stage.kind = stage_kind(code);
  if (!n || n >= sizeof stage.argument || line < 1 ||
      line > ON1X_ELVI_SOURCE_LIMIT ||
      bounded_length(home, ON1X_ELVI_PATH_CAPACITY - 1) >= ON1X_ELVI_PATH_CAPACITY)
    return invalid;
  stage.line = (size_t)line;
  memcpy(stage.argument, argument, n + 1);
  if (!on1x_elvi_resolve_stage(&stage, *home ? home : NULL, reply + 1,
                              sizeof reply - 1))
    return resolve_failed;
  return snapshot(reply, strlen(reply));
}
