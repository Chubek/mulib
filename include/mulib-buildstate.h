/**
 * @file mulib-buildstate.h
 * @brief Canonical build-action fingerprints and invalidation decisions.
 *
 * Define MULIB_IMPL_BUILDSTATE and MULIB_IMPL_SHA once. Action descriptions are
 * borrowed and inert. Host metadata access is injected through probe callbacks;
 * only the explicit file hashing helper performs filesystem I/O.
 */
#ifndef MULIB_BUILDSTATE_H
#define MULIB_BUILDSTATE_H

#include "mulib-sha.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_buildstate_status {
  MULIB_BUILDSTATE_OK = 0,
  MULIB_BUILDSTATE_INVALID = -1,
  MULIB_BUILDSTATE_MEMORY = -2,
  MULIB_BUILDSTATE_IO = -3,
  MULIB_BUILDSTATE_LIMIT = -4
} mulib_buildstate_status_t;

typedef struct mulib_buildstate_env {
  const char *name;
  const char *value;
} mulib_buildstate_env_t;

/** Borrowed normalized action. argv[0], paths and environment names are nonempty. */
typedef struct mulib_buildstate_action {
  const char *const *argv;
  size_t argc;
  const char *cwd; /**< NULL or nonempty. */
  const mulib_buildstate_env_t *env;
  size_t env_count;
  const char *const *inputs;
  size_t input_count;
  const char *const *outputs;
  size_t output_count;
  uint64_t timeout_ms; /**< Zero means no deadline. */
  int priority;
} mulib_buildstate_action_t;

/** Validate counts, strings, duplicate environment names and duplicate paths. */
int mulib_buildstate_action_validate(const mulib_buildstate_action_t *action);
/** Canonical SHA-256; output is 64 lowercase hex characters plus NUL. */
int mulib_buildstate_action_fingerprint(
    const mulib_buildstate_action_t *action, char out_hex[65]);

/** Hash a regular byte stream opened through C stdio. Zero max_bytes is unlimited. */
int mulib_buildstate_file_fingerprint(const char *path, uint64_t max_bytes,
                                      char out_hex[65]);

/**
 * Return 1 and fill digest for a present input, 0 for a missing input, or a
 * negative MULIB_BUILDSTATE_* / application status on error.
 */
typedef int (*mulib_buildstate_digest_fn)(void *user, const char *path,
                                          unsigned char digest[32]);

/**
 * Fingerprint the canonical action plus sorted input paths and their content.
 * Missing inputs are represented explicitly and cannot collide with empty files.
 */
int mulib_buildstate_provenance_fingerprint(
    const mulib_buildstate_action_t *action,
    mulib_buildstate_digest_fn digest, void *user, char out_hex[65]);

/** True iff every action environment name occurs in allowed_env. */
int mulib_buildstate_hermetic(const mulib_buildstate_action_t *action,
                              const char *const *allowed_env,
                              size_t allowed_count);

typedef enum mulib_buildstate_strategy {
  MULIB_BUILDSTATE_TIMESTAMP,
  MULIB_BUILDSTATE_CONTENT,
  MULIB_BUILDSTATE_ALWAYS
} mulib_buildstate_strategy_t;

typedef struct mulib_buildstate_path {
  int exists;
  int regular;
  uint64_t mtime_ns;
  int has_digest;
  unsigned char digest[32];
} mulib_buildstate_path_t;

/** Probe one path without following any policy hidden from the caller. */
typedef int (*mulib_buildstate_probe_fn)(void *user, const char *path,
                                         mulib_buildstate_path_t *state);

/**
 * Decide whether outputs are dirty. Timestamp mode compares newest input with
 * oldest output. Content mode requires equal counts and pairwise file digests.
 * No outputs, missing paths, or unavailable required digests are dirty.
 */
int mulib_buildstate_is_dirty(
    const char *const *inputs, size_t input_count,
    const char *const *outputs, size_t output_count,
    mulib_buildstate_strategy_t strategy, mulib_buildstate_probe_fn probe,
    void *user, int *dirty);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_BUILDSTATE) && !defined(MULIB_BUILDSTATE_IMPLEMENTED)
#define MULIB_BUILDSTATE_IMPLEMENTED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
mulib_buildstate_strings_(const char *const *items, size_t count,
                          int reject_duplicates)
{
  size_t i, j;
  if (count > 65536 || (!items && count))
    return 0;
  for (i = 0; i < count; ++i) {
    if (!items[i] || !*items[i])
      return 0;
    if (reject_duplicates)
      for (j = 0; j < i; ++j)
        if (!strcmp(items[i], items[j]))
          return 0;
  }
  return 1;
}

int
mulib_buildstate_action_validate(const mulib_buildstate_action_t *action)
{
  size_t i, j;
  if (!action || !action->argc || action->argc > 65536 || !action->argv
      || !action->argv[0] || !*action->argv[0]
      || (action->cwd && !*action->cwd)
      || action->env_count > 65536
      || (!action->env && action->env_count)
      || !mulib_buildstate_strings_(action->inputs, action->input_count, 1)
      || !mulib_buildstate_strings_(action->outputs, action->output_count, 1))
    return MULIB_BUILDSTATE_INVALID;
  for (i = 0; i < action->argc; ++i)
    if (!action->argv[i])
      return MULIB_BUILDSTATE_INVALID;
  for (i = 0; i < action->env_count; ++i) {
    if (!action->env[i].name || !*action->env[i].name
        || !action->env[i].value)
      return MULIB_BUILDSTATE_INVALID;
    for (j = 0; j < i; ++j)
      if (!strcmp(action->env[i].name, action->env[j].name))
        return MULIB_BUILDSTATE_INVALID;
  }
  for (i = 0; i < action->input_count; ++i)
    for (j = 0; j < action->output_count; ++j)
      if (!strcmp(action->inputs[i], action->outputs[j]))
        return MULIB_BUILDSTATE_INVALID;
  return MULIB_BUILDSTATE_OK;
}

static int
mulib_buildstate_u64_(mulib_sha_ctx *hash, uint64_t value)
{
  unsigned char bytes[8];
  unsigned i;
  for (i = 0; i < 8; ++i) {
    bytes[7 - i] = (unsigned char)value;
    value >>= 8;
  }
  return mulib_sha_update(hash, bytes, sizeof(bytes));
}

static int
mulib_buildstate_bytes_(mulib_sha_ctx *hash, const void *data, size_t length)
{
  return mulib_buildstate_u64_(hash, (uint64_t)length)
         && mulib_sha_update(hash, data, length);
}

static int
mulib_buildstate_text_(mulib_sha_ctx *hash, const char *text)
{
  return mulib_buildstate_bytes_(hash, text, strlen(text));
}

static void
mulib_buildstate_hex_(const unsigned char digest[32], char hex[65])
{
  static const char alphabet[] = "0123456789abcdef";
  size_t i;
  for (i = 0; i < 32; ++i) {
    hex[i * 2] = alphabet[digest[i] >> 4];
    hex[i * 2 + 1] = alphabet[digest[i] & 15];
  }
  hex[64] = '\0';
}

static int
mulib_buildstate_less_text_(const char *a, const char *b)
{
  return strcmp(a, b) < 0;
}

static int
mulib_buildstate_less_env_(const mulib_buildstate_env_t *env, size_t a,
                           size_t b)
{
  int order = strcmp(env[a].name, env[b].name);
  return order < 0 || (!order && strcmp(env[a].value, env[b].value) < 0);
}

static int
mulib_buildstate_indices_(size_t count, size_t **out)
{
  size_t *indices, i;
  *out = NULL;
  if (!count)
    return MULIB_BUILDSTATE_OK;
  if (count > SIZE_MAX / sizeof(*indices))
    return MULIB_BUILDSTATE_LIMIT;
  indices = (size_t *)malloc(count * sizeof(*indices));
  if (!indices)
    return MULIB_BUILDSTATE_MEMORY;
  for (i = 0; i < count; ++i)
    indices[i] = i;
  *out = indices;
  return MULIB_BUILDSTATE_OK;
}

static int
mulib_buildstate_action_hash_(const mulib_buildstate_action_t *action,
                              mulib_sha_ctx *hash)
{
  static const char domain[] = "mulib.build.action.v1";
  size_t *indices = NULL, i, j;
  unsigned char present;
  int rc = mulib_buildstate_action_validate(action);
  if (rc)
    return rc;
  if (!mulib_sha_init(hash, MULIB_SHA256)
      || !mulib_buildstate_bytes_(hash, domain, sizeof(domain) - 1)
      || !mulib_buildstate_u64_(hash, action->argc))
    return MULIB_BUILDSTATE_INVALID;
  for (i = 0; i < action->argc; ++i)
    if (!mulib_buildstate_text_(hash, action->argv[i]))
      return MULIB_BUILDSTATE_INVALID;
  present = action->cwd != NULL;
  if (!mulib_sha_update(hash, &present, 1)
      || (action->cwd && !mulib_buildstate_text_(hash, action->cwd))
      || !mulib_buildstate_u64_(hash, action->env_count))
    return MULIB_BUILDSTATE_INVALID;
  rc = mulib_buildstate_indices_(action->env_count, &indices);
  if (rc)
    return rc;
  for (i = 1; i < action->env_count; ++i) {
    size_t value = indices[i];
    j = i;
    while (j && mulib_buildstate_less_env_(action->env, value,
                                            indices[j - 1])) {
      indices[j] = indices[j - 1];
      --j;
    }
    indices[j] = value;
  }
  for (i = 0; i < action->env_count; ++i)
    if (!mulib_buildstate_text_(hash, action->env[indices[i]].name)
        || !mulib_buildstate_text_(hash, action->env[indices[i]].value)) {
      free(indices);
      return MULIB_BUILDSTATE_INVALID;
    }
  free(indices);
  indices = NULL;
  if (!mulib_buildstate_u64_(hash, action->input_count))
    return MULIB_BUILDSTATE_INVALID;
  rc = mulib_buildstate_indices_(action->input_count, &indices);
  if (rc)
    return rc;
  for (i = 1; i < action->input_count; ++i) {
    size_t value = indices[i];
    j = i;
    while (j
           && mulib_buildstate_less_text_(action->inputs[value],
                                          action->inputs[indices[j - 1]])) {
      indices[j] = indices[j - 1];
      --j;
    }
    indices[j] = value;
  }
  for (i = 0; i < action->input_count; ++i)
    if (!mulib_buildstate_text_(hash, action->inputs[indices[i]])) {
      free(indices);
      return MULIB_BUILDSTATE_INVALID;
    }
  free(indices);
  indices = NULL;
  if (!mulib_buildstate_u64_(hash, action->output_count))
    return MULIB_BUILDSTATE_INVALID;
  rc = mulib_buildstate_indices_(action->output_count, &indices);
  if (rc)
    return rc;
  for (i = 1; i < action->output_count; ++i) {
    size_t value = indices[i];
    j = i;
    while (j
           && mulib_buildstate_less_text_(action->outputs[value],
                                          action->outputs[indices[j - 1]])) {
      indices[j] = indices[j - 1];
      --j;
    }
    indices[j] = value;
  }
  for (i = 0; i < action->output_count; ++i)
    if (!mulib_buildstate_text_(hash, action->outputs[indices[i]])) {
      free(indices);
      return MULIB_BUILDSTATE_INVALID;
    }
  free(indices);
  if (!mulib_buildstate_u64_(hash, action->timeout_ms)
      || !mulib_buildstate_u64_(hash, (uint64_t)(int64_t)action->priority))
    return MULIB_BUILDSTATE_INVALID;
  return MULIB_BUILDSTATE_OK;
}

int
mulib_buildstate_action_fingerprint(const mulib_buildstate_action_t *action,
                                    char out_hex[65])
{
  mulib_sha_ctx hash;
  unsigned char digest[32] = {0};
  int rc;
  if (!out_hex)
    return MULIB_BUILDSTATE_INVALID;
  rc = mulib_buildstate_action_hash_(action, &hash);
  if (rc)
    return rc;
  if (!mulib_sha_final(&hash, digest, sizeof(digest)))
    return MULIB_BUILDSTATE_INVALID;
  mulib_buildstate_hex_(digest, out_hex);
  return MULIB_BUILDSTATE_OK;
}

int
mulib_buildstate_file_fingerprint(const char *path, uint64_t max_bytes,
                                  char out_hex[65])
{
  FILE *stream;
  mulib_sha_ctx hash;
  unsigned char buffer[16384], digest[32] = {0};
  uint64_t total = 0;
  if (!path || !*path || !out_hex)
    return MULIB_BUILDSTATE_INVALID;
  stream = fopen(path, "rb");
  if (!stream)
    return MULIB_BUILDSTATE_IO;
  if (!mulib_sha_init(&hash, MULIB_SHA256)) {
    fclose(stream);
    return MULIB_BUILDSTATE_INVALID;
  }
  for (;;) {
    size_t got = fread(buffer, 1, sizeof(buffer), stream);
    if (got) {
      if ((uint64_t)got > UINT64_MAX - total
          || (max_bytes && (uint64_t)got > max_bytes - total)) {
        fclose(stream);
        return MULIB_BUILDSTATE_LIMIT;
      }
      total += (uint64_t)got;
      if (!mulib_sha_update(&hash, buffer, got)) {
        fclose(stream);
        return MULIB_BUILDSTATE_LIMIT;
      }
    }
    if (got != sizeof(buffer)) {
      if (ferror(stream)) {
        fclose(stream);
        return MULIB_BUILDSTATE_IO;
      }
      break;
    }
  }
  if (fclose(stream))
    return MULIB_BUILDSTATE_IO;
  if (!mulib_sha_final(&hash, digest, sizeof(digest)))
    return MULIB_BUILDSTATE_INVALID;
  mulib_buildstate_hex_(digest, out_hex);
  return MULIB_BUILDSTATE_OK;
}

int
mulib_buildstate_provenance_fingerprint(
    const mulib_buildstate_action_t *action,
    mulib_buildstate_digest_fn digest_fn, void *user, char out_hex[65])
{
  static const char domain[] = "mulib.build.provenance.v1";
  mulib_sha_ctx action_hash, hash;
  unsigned char action_digest[32] = {0}, input_digest[32] = {0}, present;
  size_t *indices = NULL, i, j;
  int rc;
  if (!digest_fn || !out_hex)
    return MULIB_BUILDSTATE_INVALID;
  rc = mulib_buildstate_action_hash_(action, &action_hash);
  if (rc)
    return rc;
  if (!mulib_sha_final(&action_hash, action_digest, sizeof(action_digest))
      || !mulib_sha_init(&hash, MULIB_SHA256)
      || !mulib_buildstate_bytes_(&hash, domain, sizeof(domain) - 1)
      || !mulib_sha_update(&hash, action_digest, sizeof(action_digest))
      || !mulib_buildstate_u64_(&hash, action->input_count))
    return MULIB_BUILDSTATE_INVALID;
  rc = mulib_buildstate_indices_(action->input_count, &indices);
  if (rc)
    return rc;
  for (i = 1; i < action->input_count; ++i) {
    size_t value = indices[i];
    j = i;
    while (j
           && mulib_buildstate_less_text_(action->inputs[value],
                                          action->inputs[indices[j - 1]])) {
      indices[j] = indices[j - 1];
      --j;
    }
    indices[j] = value;
  }
  for (i = 0; i < action->input_count; ++i) {
    const char *path = action->inputs[indices[i]];
    rc = digest_fn(user, path, input_digest);
    if (rc < 0) {
      free(indices);
      return rc;
    }
    present = rc != 0;
    if (!mulib_buildstate_text_(&hash, path)
        || !mulib_sha_update(&hash, &present, 1)
        || (present
            && !mulib_sha_update(&hash, input_digest,
                                  sizeof(input_digest)))) {
      free(indices);
      return MULIB_BUILDSTATE_INVALID;
    }
  }
  free(indices);
  if (!mulib_sha_final(&hash, action_digest, sizeof(action_digest)))
    return MULIB_BUILDSTATE_INVALID;
  mulib_buildstate_hex_(action_digest, out_hex);
  return MULIB_BUILDSTATE_OK;
}

int
mulib_buildstate_hermetic(const mulib_buildstate_action_t *action,
                          const char *const *allowed_env,
                          size_t allowed_count)
{
  size_t i, j;
  if (mulib_buildstate_action_validate(action)
      || !mulib_buildstate_strings_(allowed_env, allowed_count, 1))
    return 0;
  for (i = 0; i < action->env_count; ++i) {
    int found = 0;
    for (j = 0; j < allowed_count; ++j)
      if (!strcmp(action->env[i].name, allowed_env[j])) {
        found = 1;
        break;
      }
    if (!found)
      return 0;
  }
  return 1;
}

int
mulib_buildstate_is_dirty(
    const char *const *inputs, size_t input_count,
    const char *const *outputs, size_t output_count,
    mulib_buildstate_strategy_t strategy, mulib_buildstate_probe_fn probe,
    void *user, int *dirty)
{
  mulib_buildstate_path_t *in = NULL, *out = NULL;
  size_t i;
  int rc = MULIB_BUILDSTATE_OK;
  if (!dirty || !probe
      || strategy < MULIB_BUILDSTATE_TIMESTAMP
      || strategy > MULIB_BUILDSTATE_ALWAYS
      || !mulib_buildstate_strings_(inputs, input_count, 0)
      || !mulib_buildstate_strings_(outputs, output_count, 0))
    return MULIB_BUILDSTATE_INVALID;
  *dirty = 1;
  if (strategy == MULIB_BUILDSTATE_ALWAYS || !output_count)
    return MULIB_BUILDSTATE_OK;
  in = (mulib_buildstate_path_t *)calloc(input_count ? input_count : 1,
                                          sizeof(*in));
  out = (mulib_buildstate_path_t *)calloc(output_count, sizeof(*out));
  if (!in || !out) {
    free(in);
    free(out);
    return MULIB_BUILDSTATE_MEMORY;
  }
  for (i = 0; i < input_count; ++i) {
    rc = probe(user, inputs[i], in + i);
    if (rc)
      goto done;
    if (!in[i].exists)
      goto done;
  }
  for (i = 0; i < output_count; ++i) {
    rc = probe(user, outputs[i], out + i);
    if (rc)
      goto done;
    if (!out[i].exists)
      goto done;
  }
  if (strategy == MULIB_BUILDSTATE_TIMESTAMP) {
    uint64_t oldest = out[0].mtime_ns;
    for (i = 1; i < output_count; ++i)
      if (out[i].mtime_ns < oldest)
        oldest = out[i].mtime_ns;
    for (i = 0; i < input_count; ++i)
      if (in[i].mtime_ns > oldest)
        goto done;
    *dirty = 0;
  } else {
    if (input_count != output_count)
      goto done;
    for (i = 0; i < input_count; ++i)
      if (!in[i].regular || !out[i].regular || !in[i].has_digest
          || !out[i].has_digest
          || memcmp(in[i].digest, out[i].digest, 32))
        goto done;
    *dirty = 0;
  }
done:
  free(in);
  free(out);
  return rc;
}

#endif
