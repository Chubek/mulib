/**
 * @file mulib-container.h
 * @brief Owned process specifications and checked container lifecycle records.
 * Define MULIB_IMPL_CONTAINER once. Execution is supplied by container-runtime.
 */
#ifndef MULIB_CONTAINER_H
#define MULIB_CONTAINER_H
#include "include-mulib-container-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_container_state
{
  MULIB_CONTAINER_CREATED = 1, MULIB_CONTAINER_RUNNING,
  MULIB_CONTAINER_PAUSED, MULIB_CONTAINER_EXITED, MULIB_CONTAINER_DELETED
} mulib_container_state;
/** @brief Zero resource values mean unspecified, not unlimited enforcement. */
typedef struct mulib_container_limits
{
  uint64_t memory_bytes, cpu_budget, wall_deadline;
  uint32_t processes, open_files;
} mulib_container_limits;
/** @brief Owned argv/environment; initialize/free and never shallow-copy. */
typedef struct mulib_container_spec
{
  char id[MULIB_CT_ID_CAP], cwd[MULIB_CT_TEXT_CAP];
  char **argv, **env;
  size_t argc, envc, max_strings, max_string_bytes, string_bytes;
  uint32_t uid, gid;
  int read_only;
  mulib_container_limits limits;
} mulib_container_spec;
/** @brief Lifecycle record; spec is owned, backend resources live elsewhere. */
typedef struct mulib_container
{
  mulib_container_spec spec;
  mulib_container_state state;
  uint64_t generation, starts, started_at, exited_at;
  int exit_code;
} mulib_container;
/** @brief Initialize uninitialized output with ID and default string quotas. */
int mulib_container_spec_init (mulib_container_spec *spec, const char *id);
/** @brief Release all owned strings; NULL-safe. */
void mulib_container_spec_free (mulib_container_spec *spec);
/** @brief Append a copied argument; empty arguments are supported. */
int mulib_container_spec_arg (mulib_container_spec *spec, const char *argument);
/** @brief Set/replace a copied NAME=value pair; NAME uses shell identifier syntax. */
int mulib_container_spec_env (mulib_container_spec *spec, const char *name,
                              const char *value);
/** @brief Borrow an environment value, or NULL when absent. */
const char *mulib_container_spec_getenv (const mulib_container_spec *spec,
                                        const char *name);
/** @brief Set a canonical absolute working directory; rejects dot components. */
int mulib_container_spec_cwd (mulib_container_spec *spec, const char *path);
/** @brief Validate invariants, lengths, quotas and strings; argv may be empty. */
int mulib_container_spec_validate (const mulib_container_spec *spec);
/** @brief Deep-copy into uninitialized output; unchanged on failure. */
int mulib_container_spec_clone (const mulib_container_spec *spec,
                                mulib_container_spec *out);
/** @brief Deep-copy a spec into a CREATED container at generation one. */
int mulib_container_init (mulib_container *container,
                          const mulib_container_spec *spec);
/** @brief Free metadata only. Runtime resources must first be destroyed. */
void mulib_container_free (mulib_container *container);
/** @brief Check a lifecycle edge without mutation. Active states reserve two
 * generations for exit/deletion; EXITED reserves one for deletion. */
int mulib_container_can_transition (const mulib_container *container,
                                    mulib_container_state state);
/** @brief Commit a valid lifecycle edge and increment generation. Caller time
 * is opaque monotonic ticks; exit code is used only for EXITED. */
int mulib_container_transition (mulib_container *container,
                                mulib_container_state state, uint64_t now,
                                int exit_code);
/** @brief Static state name, or "invalid". */
const char *mulib_container_state_name (mulib_container_state state);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER) && !defined(MULIB_CONTAINER_IMPLEMENTED)
#define MULIB_CONTAINER_IMPLEMENTED
static int
mulib_container_name_ (const char *s)
{
  size_t i, n;
  if (mulib_ct_text_length (s, 255, &n) || !n) return 0;
  for (i = 0; i < n; ++i)
    if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z')
          || s[i] == '_' || (i && s[i] >= '0' && s[i] <= '9'))) return 0;
  return 1;
}
static int
mulib_container_path_ (const char *s)
{
  size_t n, i, start;
  if (mulib_ct_text_length (s, MULIB_CT_TEXT_CAP - 1, &n)
      || !n || s[0] != '/') return 0;
  if (n == 1) return 1;
  for (i = start = 1; i <= n; ++i)
    {
      if (i < n && ((unsigned char)s[i] < 32 || s[i] == '\\'
                    || (unsigned char)s[i] == 127)) return 0;
      if (i == n || s[i] == '/')
        {
          size_t len = i - start;
          if (!len || (len == 1 && s[start] == '.')
              || (len == 2 && s[start] == '.' && s[start + 1] == '.')) return 0;
          start = i + 1;
        }
    }
  return 1;
}
int
mulib_container_spec_init (mulib_container_spec *s, const char *id)
{
  if (!s || !mulib_ct_id_valid (id)) return MULIB_CT_EINVAL;
  memset (s, 0, sizeof (*s));
  memcpy (s->id, id, strlen (id) + 1);
  s->cwd[0] = '/';
  s->max_strings = 256;
  s->max_string_bytes = 65536;
  return 0;
}
void
mulib_container_spec_free (mulib_container_spec *s)
{
  size_t i;
  if (!s) return;
  for (i = 0; i < s->argc; ++i) MULIB_CT_FREE (s->argv[i]);
  for (i = 0; i < s->envc; ++i) MULIB_CT_FREE (s->env[i]);
  MULIB_CT_FREE (s->argv);
  MULIB_CT_FREE (s->env);
  memset (s, 0, sizeof (*s));
}
static int
mulib_container_string_ (mulib_container_spec *s, char ***table, size_t *count,
                         const char *text)
{
  size_t n;
  char *copy, **next;
  int rc;
  if (!s || !s->max_strings || s->argc > s->max_strings
      || s->envc >= s->max_strings - s->argc) return MULIB_CT_ELIMIT;
  rc = mulib_ct_text_length (text, s->max_string_bytes, &n);
  if (rc) return rc;
  if (s->string_bytes >= s->max_string_bytes
      || n >= s->max_string_bytes - s->string_bytes
      || *count > SIZE_MAX / sizeof (*next) - 2) return MULIB_CT_ELIMIT;
  copy = (char *)MULIB_CT_MALLOC (n + 1);
  if (!copy) return MULIB_CT_ENOMEM;
  memcpy (copy, text, n + 1);
  next = (char **)MULIB_CT_REALLOC (*table, (*count + 2) * sizeof (*next));
  if (!next) { MULIB_CT_FREE (copy); return MULIB_CT_ENOMEM; }
  next[*count] = copy;
  next[++*count] = NULL;
  *table = next;
  s->string_bytes += n + 1;
  return 0;
}
int
mulib_container_spec_arg (mulib_container_spec *s, const char *arg)
{
  if (!s || !arg) return MULIB_CT_EINVAL;
  return mulib_container_string_ (s, &s->argv, &s->argc, arg);
}
int
mulib_container_spec_env (mulib_container_spec *s, const char *name,
                          const char *value)
{
  size_t a, b, i, old;
  char *pair;
  int rc;
  if (!s || !mulib_container_name_ (name)) return MULIB_CT_EINVAL;
  a = strlen (name);
  rc = mulib_ct_text_length (value, s->max_string_bytes, &b);
  if (rc) return rc;
  if (a > SIZE_MAX - 2 || b > SIZE_MAX - a - 2) return MULIB_CT_ELIMIT;
  for (i = 0; i < s->envc; ++i)
    if (!strncmp (s->env[i], name, a) && s->env[i][a] == '=') break;
  old = i < s->envc ? strlen (s->env[i]) + 1 : 0;
  if (old > s->string_bytes || s->string_bytes - old > s->max_string_bytes
      || a + b + 2 > s->max_string_bytes - (s->string_bytes - old))
    return MULIB_CT_ELIMIT;
  pair = (char *)MULIB_CT_MALLOC (a + b + 2);
  if (!pair) return MULIB_CT_ENOMEM;
  memcpy (pair, name, a); pair[a] = '=';
  memcpy (pair + a + 1, value, b + 1);
  if (i < s->envc)
    {
      MULIB_CT_FREE (s->env[i]); s->env[i] = pair;
      s->string_bytes = s->string_bytes - old + a + b + 2;
      return 0;
    }
  rc = mulib_container_string_ (s, &s->env, &s->envc, pair);
  MULIB_CT_FREE (pair);
  return rc;
}
const char *
mulib_container_spec_getenv (const mulib_container_spec *s, const char *name)
{
  size_t i, n;
  if (!s || !mulib_container_name_ (name)) return NULL;
  n = strlen (name);
  for (i = 0; i < s->envc; ++i)
    if (!strncmp (s->env[i], name, n) && s->env[i][n] == '=')
      return s->env[i] + n + 1;
  return NULL;
}
int
mulib_container_spec_cwd (mulib_container_spec *s, const char *path)
{
  if (!s || !mulib_container_path_ (path)) return MULIB_CT_EINVAL;
  return mulib_ct_text_copy (s->cwd, sizeof (s->cwd), path);
}
int
mulib_container_spec_validate (const mulib_container_spec *s)
{
  size_t i, j, n, total = 0;
  if (!s || !mulib_ct_id_valid (s->id) || !mulib_container_path_ (s->cwd)
      || s->argc > s->max_strings || s->envc > s->max_strings - s->argc
      || (s->argc && !s->argv) || (s->envc && !s->env)
      || (s->read_only != 0 && s->read_only != 1)) return MULIB_CT_EINVAL;
  for (i = 0; i < s->argc + s->envc; ++i)
    {
      const char *p = i < s->argc ? s->argv[i] : s->env[i - s->argc];
      if (mulib_ct_text_length (p, s->max_string_bytes, &n)
          || total >= s->max_string_bytes
          || n >= s->max_string_bytes - total) return MULIB_CT_ELIMIT;
      total += n + 1;
      if (i >= s->argc)
        {
          const char *eq = strchr (p, '=');
          char name[256];
          if (!eq || (size_t)(eq - p) >= sizeof (name)) return MULIB_CT_EINVAL;
          memcpy (name, p, (size_t)(eq - p)); name[eq - p] = 0;
          if (!mulib_container_name_ (name)) return MULIB_CT_EINVAL;
          for (j = 0; j < i - s->argc; ++j)
            if (!strncmp (s->env[j], p, (size_t)(eq - p) + 1))
              return MULIB_CT_EEXIST;
        }
    }
  return total == s->string_bytes ? 0 : MULIB_CT_EINVAL;
}
int
mulib_container_spec_clone (const mulib_container_spec *s, mulib_container_spec *out)
{
  mulib_container_spec copy;
  size_t i;
  int rc = mulib_container_spec_validate (s);
  if (!out || out == s) return MULIB_CT_EINVAL;
  if (rc) return rc;
  copy = *s; copy.argv = copy.env = NULL;
  copy.argc = copy.envc = copy.string_bytes = 0;
  for (i = 0; i < s->argc && !rc; ++i)
    rc = mulib_container_spec_arg (&copy, s->argv[i]);
  for (i = 0; i < s->envc && !rc; ++i)
    rc = mulib_container_string_ (&copy, &copy.env, &copy.envc, s->env[i]);
  if (rc) { mulib_container_spec_free (&copy); return rc; }
  *out = copy;
  return 0;
}
int
mulib_container_init (mulib_container *c, const mulib_container_spec *s)
{
  mulib_container next;
  int rc;
  if (!c) return MULIB_CT_EINVAL;
  memset (&next, 0, sizeof (next));
  rc = mulib_container_spec_clone (s, &next.spec);
  if (rc) return rc;
  next.state = MULIB_CONTAINER_CREATED; next.generation = 1;
  *c = next;
  return 0;
}
void
mulib_container_free (mulib_container *c)
{
  if (c) { mulib_container_spec_free (&c->spec); memset (c, 0, sizeof (*c)); }
}
int
mulib_container_can_transition (const mulib_container *c, mulib_container_state state)
{
  int valid = 0;
  if (!c) return MULIB_CT_EINVAL;
  switch (c->state)
    {
    case MULIB_CONTAINER_CREATED:
      valid = state == MULIB_CONTAINER_RUNNING || state == MULIB_CONTAINER_DELETED;
      break;
    case MULIB_CONTAINER_RUNNING:
      valid = state == MULIB_CONTAINER_PAUSED || state == MULIB_CONTAINER_EXITED;
      break;
    case MULIB_CONTAINER_PAUSED:
      valid = state == MULIB_CONTAINER_RUNNING || state == MULIB_CONTAINER_EXITED;
      break;
    case MULIB_CONTAINER_EXITED:
      valid = state == MULIB_CONTAINER_RUNNING || state == MULIB_CONTAINER_DELETED;
      break;
    default: break;
    }
  if (!valid) return MULIB_CT_ESTATE;
  if (c->generation == UINT64_MAX
      || ((state == MULIB_CONTAINER_RUNNING || state == MULIB_CONTAINER_PAUSED)
          && c->generation > UINT64_MAX - 3)
      || (state == MULIB_CONTAINER_EXITED && c->generation > UINT64_MAX - 2))
    return MULIB_CT_ELIMIT;
  return 0;
}
int
mulib_container_transition (mulib_container *c, mulib_container_state state,
                            uint64_t now, int code)
{
  int rc = mulib_container_can_transition (c, state);
  if (rc) return rc;
  if (state == MULIB_CONTAINER_RUNNING && c->state != MULIB_CONTAINER_PAUSED)
    {
      c->starts = mulib_ct_counter_add (c->starts, 1);
      c->started_at = now; c->exited_at = 0; c->exit_code = 0;
    }
  if (state == MULIB_CONTAINER_EXITED) { c->exited_at = now; c->exit_code = code; }
  c->state = state; ++c->generation;
  return 0;
}
const char *
mulib_container_state_name (mulib_container_state state)
{
  switch (state)
    {
    case MULIB_CONTAINER_CREATED: return "created";
    case MULIB_CONTAINER_RUNNING: return "running";
    case MULIB_CONTAINER_PAUSED: return "paused";
    case MULIB_CONTAINER_EXITED: return "exited";
    case MULIB_CONTAINER_DELETED: return "deleted";
    default: return "invalid";
    }
}
#endif
