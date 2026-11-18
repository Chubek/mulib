/**
 * @file mulib-buildparse.h
 * @brief Bounded dependency extractors for common build-definition syntaxes.
 *
 * Define MULIB_IMPL_BUILDPARSE and MULIB_IMPL_BUILDPLAN once. These adapters
 * extract declared target relationships from Make, Ninja, CMake, Meson and
 * Bazel text. They deliberately do not evaluate variables, functions, scripts,
 * generators or configuration-time control flow.
 */
#ifndef MULIB_BUILDPARSE_H
#define MULIB_BUILDPARSE_H

#include "mulib-buildplan.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_buildparse_format {
  MULIB_BUILDPARSE_AUTO,
  MULIB_BUILDPARSE_MAKE,
  MULIB_BUILDPARSE_NINJA,
  MULIB_BUILDPARSE_CMAKE,
  MULIB_BUILDPARSE_MESON,
  MULIB_BUILDPARSE_BAZEL
} mulib_buildparse_format_t;

typedef enum mulib_buildparse_status {
  MULIB_BUILDPARSE_OK = 0,
  MULIB_BUILDPARSE_INVALID = -1,
  MULIB_BUILDPARSE_MEMORY = -2,
  MULIB_BUILDPARSE_LIMIT = -3,
  MULIB_BUILDPARSE_IO = -4
} mulib_buildparse_status_t;

typedef struct mulib_buildparse_options {
  size_t max_source_bytes; /**< Default: 16 MiB. */
  size_t max_targets;      /**< Includes referenced source/dependency nodes. */
  size_t max_dependencies;
  size_t max_token_bytes;  /**< Default: 4095. */
  size_t max_nesting;      /**< Parentheses/list nesting; default: 128. */
} mulib_buildparse_options_t;

typedef struct mulib_buildparse_error {
  size_t offset, line, column;
  const char *message; /**< Static string, or NULL on success. */
} mulib_buildparse_error_t;

/**
 * Parse exactly one source buffer into a new owned BuildPlan. *out must be NULL
 * and remains NULL on failure. AUTO uses conservative syntax heuristics.
 */
int mulib_buildparse(const char *source, size_t length,
                     mulib_buildparse_format_t format,
                     const mulib_buildparse_options_t *options,
                     mulib_buildplan_t **out,
                     mulib_buildparse_error_t *error);

/** Read a bounded file and parse it. AUTO also considers the filename suffix. */
int mulib_buildparse_file(const char *path, mulib_buildparse_format_t format,
                          const mulib_buildparse_options_t *options,
                          mulib_buildplan_t **out,
                          mulib_buildparse_error_t *error);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_BUILDPARSE) && !defined(MULIB_BUILDPARSE_IMPLEMENTED)
#define MULIB_BUILDPARSE_IMPLEMENTED

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mulib_buildparse_words_ {
  char **items;
  size_t count, capacity;
} mulib_buildparse_words_;

static mulib_buildparse_options_t
mulib_buildparse_options_(const mulib_buildparse_options_t *provided)
{
  mulib_buildparse_options_t options;
  options.max_source_bytes = 16u * 1024u * 1024u;
  options.max_targets = 4096;
  options.max_dependencies = 16384;
  options.max_token_bytes = 4095;
  options.max_nesting = 128;
  if (provided) {
    if (provided->max_source_bytes)
      options.max_source_bytes = provided->max_source_bytes;
    if (provided->max_targets)
      options.max_targets = provided->max_targets;
    if (provided->max_dependencies)
      options.max_dependencies = provided->max_dependencies;
    if (provided->max_token_bytes)
      options.max_token_bytes = provided->max_token_bytes;
    if (provided->max_nesting)
      options.max_nesting = provided->max_nesting;
  }
  return options;
}

static void
mulib_buildparse_error_(mulib_buildparse_error_t *error, const char *source,
                        size_t length, size_t offset, const char *message)
{
  size_t i;
  if (!error)
    return;
  if (offset > length)
    offset = length;
  error->offset = offset;
  error->line = 1;
  error->column = 1;
  error->message = message;
  for (i = 0; i < offset; ++i)
    if (source[i] == '\n') {
      ++error->line;
      error->column = 1;
    } else
      ++error->column;
}

static void
mulib_buildparse_words_free_(mulib_buildparse_words_ *words)
{
  size_t i;
  for (i = 0; i < words->count; ++i)
    free(words->items[i]);
  free(words->items);
  memset(words, 0, sizeof(*words));
}

static int
mulib_buildparse_add_word_(mulib_buildparse_words_ *words, const char *text,
                           size_t length,
                           const mulib_buildparse_options_t *options)
{
  char *copy;
  size_t limit;
  if (!length)
    return MULIB_BUILDPARSE_OK;
  if (length > options->max_token_bytes
      || options->max_targets > SIZE_MAX - options->max_dependencies - 16)
    return MULIB_BUILDPARSE_LIMIT;
  limit = options->max_targets + options->max_dependencies + 16;
  if (words->count == limit)
    return MULIB_BUILDPARSE_LIMIT;
  if (words->count == words->capacity) {
    size_t capacity = words->capacity ? words->capacity * 2 : 8;
    char **items;
    if (capacity <= words->capacity || capacity > limit)
      capacity = limit;
    if (capacity <= words->capacity
        || capacity > SIZE_MAX / sizeof(*items))
      return MULIB_BUILDPARSE_LIMIT;
    items = (char **)realloc(words->items, capacity * sizeof(*items));
    if (!items)
      return MULIB_BUILDPARSE_MEMORY;
    words->items = items;
    words->capacity = capacity;
  }
  copy = (char *)malloc(length + 1);
  if (!copy)
    return MULIB_BUILDPARSE_MEMORY;
  memcpy(copy, text, length);
  copy[length] = '\0';
  words->items[words->count++] = copy;
  return MULIB_BUILDPARSE_OK;
}

/* Make-like words: backslash quotes the next byte; quotes have no special role. */
static int
mulib_buildparse_make_words_(const char *text, size_t length, int ninja,
                             const mulib_buildparse_options_t *options,
                             mulib_buildparse_words_ *out)
{
  char *word;
  size_t i = 0, used = 0;
  int rc = MULIB_BUILDPARSE_OK;
  if (options->max_token_bytes == SIZE_MAX)
    return MULIB_BUILDPARSE_LIMIT;
  word = (char *)malloc(options->max_token_bytes + 1);
  if (!word)
    return MULIB_BUILDPARSE_MEMORY;
  while (i < length) {
    unsigned char c = (unsigned char)text[i++];
    if ((c == '\\' && !ninja) || (c == '$' && ninja)) {
      if (i < length) {
        c = (unsigned char)text[i++];
        if (ninja && c == '$')
          c = '$';
      }
    } else if (isspace(c)) {
      if (used) {
        rc = mulib_buildparse_add_word_(out, word, used, options);
        if (rc)
          break;
        used = 0;
      }
      continue;
    }
    if (used == options->max_token_bytes) {
      rc = MULIB_BUILDPARSE_LIMIT;
      break;
    }
    word[used++] = (char)c;
  }
  if (!rc && used)
    rc = mulib_buildparse_add_word_(out, word, used, options);
  free(word);
  return rc;
}

/* CMake-style shell words with quotes, escapes, commas and semicolons. */
static int
mulib_buildparse_shell_words_(const char *text, size_t length,
                              const mulib_buildparse_options_t *options,
                              mulib_buildparse_words_ *out)
{
  char *word;
  size_t i = 0, used = 0;
  int quote = 0, escaped = 0, rc = MULIB_BUILDPARSE_OK;
  if (options->max_token_bytes == SIZE_MAX)
    return MULIB_BUILDPARSE_LIMIT;
  word = (char *)malloc(options->max_token_bytes + 1);
  if (!word)
    return MULIB_BUILDPARSE_MEMORY;
  while (i < length) {
    unsigned char c = (unsigned char)text[i++];
    if (escaped) {
      escaped = 0;
    } else if (c == '\\') {
      escaped = 1;
      continue;
    } else if (quote) {
      if (c == (unsigned char)quote) {
        quote = 0;
        continue;
      }
    } else if (c == '\'' || c == '"') {
      quote = c;
      continue;
    } else if (isspace(c) || c == ';' || c == ',') {
      if (used) {
        rc = mulib_buildparse_add_word_(out, word, used, options);
        if (rc)
          break;
        used = 0;
      }
      continue;
    }
    if (used == options->max_token_bytes) {
      rc = MULIB_BUILDPARSE_LIMIT;
      break;
    }
    word[used++] = (char)c;
  }
  if (!rc && (quote || escaped))
    rc = MULIB_BUILDPARSE_INVALID;
  if (!rc && used)
    rc = mulib_buildparse_add_word_(out, word, used, options);
  free(word);
  return rc;
}

static int
mulib_buildparse_plan_status_(int rc)
{
  if (rc == MULIB_BUILDPLAN_MEMORY)
    return MULIB_BUILDPARSE_MEMORY;
  if (rc == MULIB_BUILDPLAN_FULL || rc == MULIB_BUILDPLAN_OVERFLOW)
    return MULIB_BUILDPARSE_LIMIT;
  return rc ? MULIB_BUILDPARSE_INVALID : MULIB_BUILDPARSE_OK;
}

static int
mulib_buildparse_rule_(mulib_buildplan_t *plan, const char *target,
                       char *const *dependencies, size_t dependency_count,
                       const char *tag)
{
  size_t target_id, dependency_id, i;
  int rc = mulib_buildplan_target(plan, target, &target_id);
  if (rc)
    return mulib_buildparse_plan_status_(rc);
  if (tag) {
    rc = mulib_buildplan_set_tag(plan, target_id, tag);
    if (rc)
      return mulib_buildparse_plan_status_(rc);
  }
  for (i = 0; i < dependency_count; ++i) {
    if (!strcmp(dependencies[i], "|") || !strcmp(dependencies[i], "||")
        || !strcmp(dependencies[i], "|@"))
      continue;
    rc = mulib_buildplan_target(plan, dependencies[i], &dependency_id);
    if (!rc)
      rc = mulib_buildplan_depend(plan, dependency_id, target_id);
    if (rc)
      return mulib_buildparse_plan_status_(rc);
  }
  return MULIB_BUILDPARSE_OK;
}

static size_t
mulib_buildparse_comment_(const char *line, size_t length)
{
  size_t i, slashes = 0;
  for (i = 0; i < length; ++i) {
    if (line[i] == '#' && !(slashes & 1u))
      return i;
    if (line[i] == '\\')
      ++slashes;
    else
      slashes = 0;
  }
  return length;
}

static int
mulib_buildparse_lines_(const char *source, size_t length, int ninja,
                        const mulib_buildparse_options_t *options,
                        mulib_buildplan_t *plan, size_t *bad_offset)
{
  char *logical;
  size_t i = 0, used = 0, line_start = 0;
  int rc = MULIB_BUILDPARSE_OK;
  if (length == SIZE_MAX)
    return MULIB_BUILDPARSE_LIMIT;
  logical = (char *)malloc(length + 1);
  if (!logical)
    return MULIB_BUILDPARSE_MEMORY;
  while (i <= length) {
    size_t raw_start = i, raw_end, content_end, left, colon, j, trailing = 0;
    char continuation = ninja ? '$' : '\\';
    mulib_buildparse_words_ targets = {0}, fields = {0};
    if (i == length && !used)
      break;
    raw_end = i;
    while (raw_end < length && source[raw_end] != '\n'
           && source[raw_end] != '\r')
      ++raw_end;
    content_end = raw_end;
    while (content_end > raw_start
           && (source[content_end - 1] == ' '
               || source[content_end - 1] == '\t'))
      --content_end;
    while (content_end > raw_start + trailing
           && source[content_end - trailing - 1] == continuation)
      ++trailing;
    if (trailing & 1u) {
      size_t piece = content_end - raw_start - 1;
      if (piece > length - used) {
        rc = MULIB_BUILDPARSE_LIMIT;
        goto line_done;
      }
      memcpy(logical + used, source + raw_start, piece);
      used += piece;
      logical[used++] = ' ';
      i = raw_end;
      if (i < length && source[i] == '\r')
        ++i;
      if (i < length && source[i] == '\n')
        ++i;
      continue;
    }
    if (raw_end - raw_start > length - used) {
      rc = MULIB_BUILDPARSE_LIMIT;
      goto line_done;
    }
    memcpy(logical + used, source + raw_start, raw_end - raw_start);
    used += raw_end - raw_start;
    logical[used] = '\0';
    left = 0;
    while (left < used && isspace((unsigned char)logical[left]))
      ++left;
    if (left < used && (logical[0] == '\t'
                        || (ninja && left > 0)))
      goto line_done;
    content_end = mulib_buildparse_comment_(logical + left, used - left)
                  + left;
    while (content_end > left
           && isspace((unsigned char)logical[content_end - 1]))
      --content_end;
    if (content_end == left)
      goto line_done;
    if (ninja) {
      if (content_end - left < 6
          || memcmp(logical + left, "build ", 6))
        goto line_done;
      left += 6;
    }
    colon = SIZE_MAX;
    for (j = left; j < content_end; ++j)
      if (logical[j] == ':') {
        colon = j;
        break;
      }
    if (colon == SIZE_MAX)
      goto line_done;
    if (!ninja)
      for (j = left; j < colon; ++j)
        if (logical[j] == '=')
          goto line_done;
    rc = mulib_buildparse_make_words_(logical + left, colon - left, ninja,
                                      options, &targets);
    if (!rc)
      rc = mulib_buildparse_make_words_(logical + colon + 1,
                                        content_end - colon - 1, ninja,
                                        options, &fields);
    if (rc)
      goto line_done;
    if (ninja) {
      const char *tag;
      if (!targets.count || !fields.count)
        goto line_done;
      tag = fields.items[0];
      for (j = 0; j < targets.count; ++j) {
        if (!strcmp(targets.items[j], "|"))
          continue;
        rc = mulib_buildparse_rule_(plan, targets.items[j],
                                    fields.items + 1, fields.count - 1, tag);
        if (rc)
          goto line_done;
      }
    } else {
      for (j = 0; j < targets.count; ++j) {
        rc = mulib_buildparse_rule_(plan, targets.items[j], fields.items,
                                    fields.count, "make");
        if (rc)
          goto line_done;
      }
    }
line_done:
    mulib_buildparse_words_free_(&targets);
    mulib_buildparse_words_free_(&fields);
    if (rc) {
      *bad_offset = line_start;
      break;
    }
    used = 0;
    i = raw_end;
    if (i < length && source[i] == '\r')
      ++i;
    if (i < length && source[i] == '\n')
      ++i;
    line_start = i;
  }
  free(logical);
  return rc;
}

static int
mulib_buildparse_identifier_(unsigned char c, int first)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
         || c == '_' || (!first && c >= '0' && c <= '9');
}

/* Find the matching ')' while respecting strings and line comments. */
static int
mulib_buildparse_call_end_(const char *source, size_t length, size_t open,
                           size_t max_nesting, size_t *close)
{
  size_t i, depth = 1;
  int quote = 0, escaped = 0;
  for (i = open + 1; i < length; ++i) {
    unsigned char c = (unsigned char)source[i];
    if (escaped) {
      escaped = 0;
      continue;
    }
    if (quote) {
      if (c == '\\')
        escaped = 1;
      else if (c == (unsigned char)quote)
        quote = 0;
      continue;
    }
    if (c == '\'' || c == '"') {
      quote = c;
      continue;
    }
    if (c == '#') {
      while (i < length && source[i] != '\n')
        ++i;
      continue;
    }
    if (c == '(') {
      if (++depth > max_nesting)
        return MULIB_BUILDPARSE_LIMIT;
    } else if (c == ')' && !--depth) {
      *close = i;
      return MULIB_BUILDPARSE_OK;
    }
  }
  return MULIB_BUILDPARSE_INVALID;
}

static int
mulib_buildparse_case_equal_(const char *text, size_t length,
                             const char *word)
{
  size_t i;
  if (strlen(word) != length)
    return 0;
  for (i = 0; i < length; ++i)
    if (tolower((unsigned char)text[i])
        != tolower((unsigned char)word[i]))
      return 0;
  return 1;
}

static int
mulib_buildparse_cmake_mode_(const char *word)
{
  static const char *const modes[] = {
      "STATIC", "SHARED", "MODULE", "OBJECT", "UNKNOWN", "INTERFACE",
      "IMPORTED", "ALIAS", "EXCLUDE_FROM_ALL", "WIN32", "MACOSX_BUNDLE",
      "PRIVATE", "PUBLIC"};
  size_t i;
  for (i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
    if (!strcmp(word, modes[i]))
      return 1;
  return 0;
}

static int
mulib_buildparse_cmake_(const char *source, size_t length,
                        const mulib_buildparse_options_t *options,
                        mulib_buildplan_t *plan, size_t *bad_offset)
{
  size_t i = 0;
  while (i < length) {
    size_t name_start, name_end, open, close, j;
    mulib_buildparse_words_ words = {0};
    int recognized, rc;
    if (source[i] == '#') {
      while (i < length && source[i] != '\n')
        ++i;
      continue;
    }
    if (!mulib_buildparse_identifier_((unsigned char)source[i], 1)) {
      ++i;
      continue;
    }
    name_start = i++;
    while (i < length
           && mulib_buildparse_identifier_((unsigned char)source[i], 0))
      ++i;
    name_end = i;
    while (i < length && isspace((unsigned char)source[i]))
      ++i;
    if (i == length || source[i] != '(')
      continue;
    open = i;
    recognized =
        mulib_buildparse_case_equal_(source + name_start,
                                     name_end - name_start,
                                     "add_executable")
        || mulib_buildparse_case_equal_(source + name_start,
                                        name_end - name_start,
                                        "add_library")
        || mulib_buildparse_case_equal_(source + name_start,
                                        name_end - name_start,
                                        "add_dependencies")
        || mulib_buildparse_case_equal_(source + name_start,
                                        name_end - name_start,
                                        "target_sources");
    rc = mulib_buildparse_call_end_(source, length, open,
                                    options->max_nesting, &close);
    if (rc) {
      *bad_offset = open;
      return rc;
    }
    if (!recognized) {
      i = close + 1;
      continue;
    }
    rc = mulib_buildparse_shell_words_(source + open + 1,
                                       close - open - 1, options, &words);
    if (rc) {
      mulib_buildparse_words_free_(&words);
      *bad_offset = open;
      return rc;
    }
    if (words.count) {
      if (mulib_buildparse_case_equal_(source + name_start,
                                       name_end - name_start,
                                       "add_dependencies")) {
        rc = mulib_buildparse_rule_(plan, words.items[0], words.items + 1,
                                    words.count - 1, "cmake:dependencies");
      } else {
        size_t write = 1;
        const char *tag =
            mulib_buildparse_case_equal_(source + name_start,
                                         name_end - name_start,
                                         "add_executable")
                ? "cmake:executable"
                : mulib_buildparse_case_equal_(source + name_start,
                                                name_end - name_start,
                                                "add_library")
                      ? "cmake:library"
                      : "cmake:sources";
        for (j = 1; j < words.count; ++j)
          if (!mulib_buildparse_cmake_mode_(words.items[j]))
            words.items[write++] = words.items[j];
          else {
            free(words.items[j]);
            words.items[j] = NULL;
          }
        words.count = write;
        rc = mulib_buildparse_rule_(plan, words.items[0], words.items + 1,
                                    words.count - 1, tag);
      }
      if (rc) {
        mulib_buildparse_words_free_(&words);
        *bad_offset = open;
        return rc;
      }
    }
    mulib_buildparse_words_free_(&words);
    i = close + 1;
  }
  return MULIB_BUILDPARSE_OK;
}

static int
mulib_buildparse_quoted_(const char *text, size_t length, size_t start,
                         const mulib_buildparse_options_t *options,
                         char **out, size_t *after)
{
  char quote, *value;
  size_t i, used = 0;
  if (start >= length || (text[start] != '\'' && text[start] != '"')
      || options->max_token_bytes == SIZE_MAX)
    return MULIB_BUILDPARSE_INVALID;
  quote = text[start++];
  value = (char *)malloc(options->max_token_bytes + 1);
  if (!value)
    return MULIB_BUILDPARSE_MEMORY;
  for (i = start; i < length; ++i) {
    char c = text[i];
    if (c == quote) {
      value[used] = '\0';
      *out = value;
      *after = i + 1;
      return used ? MULIB_BUILDPARSE_OK : MULIB_BUILDPARSE_INVALID;
    }
    if (c == '\\' && i + 1 < length)
      c = text[++i];
    if (used == options->max_token_bytes) {
      free(value);
      return MULIB_BUILDPARSE_LIMIT;
    }
    value[used++] = c;
  }
  free(value);
  return MULIB_BUILDPARSE_INVALID;
}

static int
mulib_buildparse_find_attribute_(const char *body, size_t length,
                                 const char *name, int equals,
                                 size_t *value)
{
  size_t i = 0, name_length = strlen(name);
  while (i + name_length <= length) {
    if ((i == 0
         || !mulib_buildparse_identifier_((unsigned char)body[i - 1], 0))
        && !memcmp(body + i, name, name_length)
        && (i + name_length == length
            || !mulib_buildparse_identifier_(
                (unsigned char)body[i + name_length], 0))) {
      size_t j = i + name_length;
      while (j < length && isspace((unsigned char)body[j]))
        ++j;
      if (j < length && body[j] == (equals ? '=' : ':')) {
        ++j;
        while (j < length && isspace((unsigned char)body[j]))
          ++j;
        *value = j;
        return 1;
      }
    }
    ++i;
  }
  return 0;
}

static int
mulib_buildparse_string_list_(const char *body, size_t length, size_t start,
                              const mulib_buildparse_options_t *options,
                              mulib_buildparse_words_ *words)
{
  size_t i;
  int depth = 0;
  if (start >= length || body[start] != '[')
    return MULIB_BUILDPARSE_OK; /* Dynamic/nonliteral attribute: not resolved. */
  for (i = start; i < length; ++i) {
    if (body[i] == '[') {
      if ((size_t)++depth > options->max_nesting)
        return MULIB_BUILDPARSE_LIMIT;
    } else if (body[i] == ']') {
      if (!--depth)
        return MULIB_BUILDPARSE_OK;
    } else if (body[i] == '\'' || body[i] == '"') {
      char *value = NULL;
      size_t after;
      int rc = mulib_buildparse_quoted_(body, length, i, options, &value,
                                        &after);
      if (rc)
        return rc;
      rc = mulib_buildparse_add_word_(words, value, strlen(value), options);
      free(value);
      if (rc)
        return rc;
      i = after - 1;
    }
  }
  return MULIB_BUILDPARSE_INVALID;
}

static int
mulib_buildparse_meson_or_bazel_(
    const char *source, size_t length,
    const mulib_buildparse_options_t *options, mulib_buildplan_t *plan,
    size_t *bad_offset, int bazel)
{
  size_t i = 0;
  while (i < length) {
    size_t name_start, name_end, open, close, value_at, after, k;
    char *target = NULL, *tag = NULL;
    mulib_buildparse_words_ dependencies = {0};
    int relevant = bazel, rc;
    if (source[i] == '#') {
      while (i < length && source[i] != '\n')
        ++i;
      continue;
    }
    if (!mulib_buildparse_identifier_((unsigned char)source[i], 1)) {
      ++i;
      continue;
    }
    name_start = i++;
    while (i < length
           && mulib_buildparse_identifier_((unsigned char)source[i], 0))
      ++i;
    name_end = i;
    while (i < length && isspace((unsigned char)source[i]))
      ++i;
    if (i == length || source[i] != '(')
      continue;
    open = i;
    if (!bazel)
      relevant = mulib_buildparse_case_equal_(
                     source + name_start, name_end - name_start, "executable")
                 || mulib_buildparse_case_equal_(
                     source + name_start, name_end - name_start, "library")
                 || mulib_buildparse_case_equal_(
                     source + name_start, name_end - name_start,
                     "shared_library")
                 || mulib_buildparse_case_equal_(
                     source + name_start, name_end - name_start,
                     "static_library");
    rc = mulib_buildparse_call_end_(source, length, open,
                                    options->max_nesting, &close);
    if (rc) {
      *bad_offset = open;
      return rc;
    }
    if (!relevant) {
      i = close + 1;
      continue;
    }
    tag = (char *)malloc(name_end - name_start + 7);
    if (!tag)
      return MULIB_BUILDPARSE_MEMORY;
    memcpy(tag, bazel ? "bazel:" : "meson:", 6);
    memcpy(tag + 6, source + name_start, name_end - name_start);
    tag[name_end - name_start + 6] = '\0';
    if (bazel) {
      if (!mulib_buildparse_find_attribute_(
              source + open + 1, close - open - 1, "name", 1, &value_at))
        goto call_done;
    } else {
      value_at = 0;
      while (value_at < close - open - 1
             && isspace((unsigned char)source[open + 1 + value_at]))
        ++value_at;
    }
    rc = mulib_buildparse_quoted_(
        source + open + 1, close - open - 1, value_at, options, &target,
        &after);
    if (rc)
      goto call_fail;
    if (bazel) {
      static const char *const attributes[] = {"deps", "srcs"};
      for (k = 0; k < sizeof(attributes) / sizeof(attributes[0]); ++k)
        if (mulib_buildparse_find_attribute_(
                source + open + 1, close - open - 1, attributes[k], 1,
                &value_at)) {
          rc = mulib_buildparse_string_list_(
              source + open + 1, close - open - 1, value_at, options,
              &dependencies);
          if (rc)
            goto call_fail;
        }
    } else {
      static const char *const attributes[] = {
          "dependencies", "link_with", "link_whole"};
      for (k = 0; k < sizeof(attributes) / sizeof(attributes[0]); ++k)
        if (mulib_buildparse_find_attribute_(
                source + open + 1, close - open - 1, attributes[k], 0,
                &value_at)) {
          rc = mulib_buildparse_string_list_(
              source + open + 1, close - open - 1, value_at, options,
              &dependencies);
          if (rc)
            goto call_fail;
        }
    }
    rc = mulib_buildparse_rule_(plan, target, dependencies.items,
                                dependencies.count, tag);
    if (rc)
      goto call_fail;
call_done:
    free(target);
    free(tag);
    mulib_buildparse_words_free_(&dependencies);
    i = close + 1;
    continue;
call_fail:
    free(target);
    free(tag);
    mulib_buildparse_words_free_(&dependencies);
    *bad_offset = open;
    return rc;
  }
  return MULIB_BUILDPARSE_OK;
}

static mulib_buildparse_format_t
mulib_buildparse_detect_(const char *source, size_t length)
{
  size_t i;
  if (strstr(source, "add_executable(") || strstr(source, "add_library(")
      || strstr(source, "cmake_minimum_required("))
    return MULIB_BUILDPARSE_CMAKE;
  if (strstr(source, "cc_library(") || strstr(source, "cc_binary(")
      || strstr(source, "py_library("))
    return MULIB_BUILDPARSE_BAZEL;
  if (strstr(source, "executable(") || strstr(source, "shared_library(")
      || strstr(source, "static_library("))
    return MULIB_BUILDPARSE_MESON;
  for (i = 0; i + 6 <= length; ++i)
    if ((i == 0 || source[i - 1] == '\n')
        && !memcmp(source + i, "build ", 6))
      return MULIB_BUILDPARSE_NINJA;
  return MULIB_BUILDPARSE_MAKE;
}

int
mulib_buildparse(const char *source, size_t length,
                 mulib_buildparse_format_t format,
                 const mulib_buildparse_options_t *provided,
                 mulib_buildplan_t **out, mulib_buildparse_error_t *error)
{
  mulib_buildparse_options_t options = mulib_buildparse_options_(provided);
  mulib_buildplan_t *plan;
  char *terminated = NULL;
  size_t bad_offset = 0;
  int rc;
  if (error)
    memset(error, 0, sizeof(*error));
  if (!out || *out || (!source && length)
      || format < MULIB_BUILDPARSE_AUTO || format > MULIB_BUILDPARSE_BAZEL
      || length > options.max_source_bytes || !options.max_token_bytes
      || !options.max_nesting
      || options.max_targets > SIZE_MAX / sizeof(size_t)
      || options.max_dependencies > SIZE_MAX / (2 * sizeof(size_t))) {
    if (error)
      error->message = "invalid build parser input or limits";
    return length > options.max_source_bytes ? MULIB_BUILDPARSE_LIMIT
                                              : MULIB_BUILDPARSE_INVALID;
  }
  if (length == SIZE_MAX)
    return MULIB_BUILDPARSE_LIMIT;
  terminated = (char *)malloc(length + 1);
  if (!terminated)
    return MULIB_BUILDPARSE_MEMORY;
  if (length)
    memcpy(terminated, source, length);
  terminated[length] = '\0';
  if (memchr(terminated, '\0', length)) {
    free(terminated);
    if (error)
      error->message = "embedded NUL in build definition";
    return MULIB_BUILDPARSE_INVALID;
  }
  if (format == MULIB_BUILDPARSE_AUTO)
    format = mulib_buildparse_detect_(terminated, length);
  plan = mulib_buildplan_create(options.max_targets,
                                options.max_dependencies);
  if (!plan) {
    free(terminated);
    return MULIB_BUILDPARSE_MEMORY;
  }
  if (format == MULIB_BUILDPARSE_MAKE || format == MULIB_BUILDPARSE_NINJA)
    rc = mulib_buildparse_lines_(terminated, length,
                                 format == MULIB_BUILDPARSE_NINJA, &options,
                                 plan, &bad_offset);
  else if (format == MULIB_BUILDPARSE_CMAKE)
    rc = mulib_buildparse_cmake_(terminated, length, &options, plan,
                                 &bad_offset);
  else
    rc = mulib_buildparse_meson_or_bazel_(
        terminated, length, &options, plan, &bad_offset,
        format == MULIB_BUILDPARSE_BAZEL);
  if (rc) {
    mulib_buildparse_error_(
        error, terminated, length, bad_offset,
        rc == MULIB_BUILDPARSE_MEMORY
            ? "unable to allocate build graph"
            : rc == MULIB_BUILDPARSE_LIMIT
                  ? "build definition exceeds configured limits"
                  : "malformed supported build-definition construct");
    mulib_buildplan_destroy(plan);
    free(terminated);
    return rc;
  }
  free(terminated);
  *out = plan;
  return MULIB_BUILDPARSE_OK;
}

static mulib_buildparse_format_t
mulib_buildparse_path_format_(const char *path)
{
  const char *base = strrchr(path, '/');
  const char *dot;
  base = base ? base + 1 : path;
  dot = strrchr(base, '.');
  if (!strcmp(base, "CMakeLists.txt") || (dot && !strcmp(dot, ".cmake")))
    return MULIB_BUILDPARSE_CMAKE;
  if (!strcmp(base, "meson.build"))
    return MULIB_BUILDPARSE_MESON;
  if (!strcmp(base, "BUILD") || !strcmp(base, "BUILD.bazel")
      || (dot && !strcmp(dot, ".bzl")))
    return MULIB_BUILDPARSE_BAZEL;
  if (dot && !strcmp(dot, ".ninja"))
    return MULIB_BUILDPARSE_NINJA;
  return MULIB_BUILDPARSE_MAKE;
}

int
mulib_buildparse_file(const char *path, mulib_buildparse_format_t format,
                      const mulib_buildparse_options_t *provided,
                      mulib_buildplan_t **out,
                      mulib_buildparse_error_t *error)
{
  mulib_buildparse_options_t options = mulib_buildparse_options_(provided);
  FILE *stream;
  char *data = NULL;
  size_t length = 0, capacity = 0;
  int rc;
  if (error)
    memset(error, 0, sizeof(*error));
  if (!path || !*path || !out || *out)
    return MULIB_BUILDPARSE_INVALID;
  stream = fopen(path, "rb");
  if (!stream) {
    if (error)
      error->message = "unable to open build definition";
    return MULIB_BUILDPARSE_IO;
  }
  for (;;) {
    size_t got;
    if (length == capacity) {
      size_t next = capacity ? capacity * 2 : 4096;
      char *grown;
      if (next <= capacity || next > options.max_source_bytes)
        next = options.max_source_bytes;
      if (next <= capacity) {
        int byte = fgetc(stream);
        if (byte != EOF) {
          free(data);
          fclose(stream);
          if (error)
            error->message = "build definition source limit exceeded";
          return MULIB_BUILDPARSE_LIMIT;
        }
        if (ferror(stream)) {
          free(data);
          fclose(stream);
          if (error)
            error->message = "unable to read build definition";
          return MULIB_BUILDPARSE_IO;
        }
        break;
      }
      grown = (char *)realloc(data, next);
      if (!grown) {
        free(data);
        fclose(stream);
        return MULIB_BUILDPARSE_MEMORY;
      }
      data = grown;
      capacity = next;
    }
    got = fread(data + length, 1, capacity - length, stream);
    length += got;
    if (!got) {
      if (ferror(stream)) {
        free(data);
        fclose(stream);
        return MULIB_BUILDPARSE_IO;
      }
      break;
    }
  }
  if (fclose(stream)) {
    free(data);
    return MULIB_BUILDPARSE_IO;
  }
  if (format == MULIB_BUILDPARSE_AUTO)
    format = mulib_buildparse_path_format_(path);
  rc = mulib_buildparse(data, length, format, &options, out, error);
  free(data);
  return rc;
}

#endif
