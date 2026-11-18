/**
 * @file mulib-depfile.h
 * @brief Bounded GCC/Ninja dependency-file parser.
 *
 * Define MULIB_IMPL_DEPFILE once. The parser performs no filesystem access
 * unless mulib_depfile_parse_file is called. Rule strings are borrowed only for
 * the callback. This is a depfile parser, not a general Make evaluator.
 */
#ifndef MULIB_DEPFILE_H
#define MULIB_DEPFILE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_depfile_status {
  MULIB_DEPFILE_OK = 0,
  MULIB_DEPFILE_INVALID = -1,
  MULIB_DEPFILE_MEMORY = -2,
  MULIB_DEPFILE_LIMIT = -3,
  MULIB_DEPFILE_IO = -4
} mulib_depfile_status_t;

typedef struct mulib_depfile_limits {
  size_t max_source_bytes; /**< Default: 16 MiB. */
  size_t max_rules;        /**< Default: 65536. */
  size_t max_words;        /**< Aggregate target/prerequisite default: 262144. */
  size_t max_word_bytes;   /**< Default: 4095. */
} mulib_depfile_limits_t;

typedef struct mulib_depfile_error {
  size_t offset, line, column;
  const char *message; /**< Static string, or NULL on success. */
} mulib_depfile_error_t;

typedef struct mulib_depfile_rule {
  const char *const *targets;
  size_t target_count;
  const char *const *dependencies;
  size_t dependency_count;
} mulib_depfile_rule_t;

/**
 * Consume one parsed logical rule. Returning nonzero stops parsing and that
 * value is propagated unchanged.
 */
typedef int (*mulib_depfile_rule_fn)(void *user,
                                     const mulib_depfile_rule_t *rule);

/**
 * Parse exactly length bytes. Embedded NUL bytes, missing separators and
 * unterminated continuations are rejected. Repeated targets are reported as
 * repeated rules so callers can choose their own merge policy.
 */
int mulib_depfile_parse(const char *source, size_t length,
                        const mulib_depfile_limits_t *limits,
                        mulib_depfile_rule_fn callback, void *user,
                        mulib_depfile_error_t *error);

/** Read and parse a file under max_source_bytes. */
int mulib_depfile_parse_file(const char *path,
                             const mulib_depfile_limits_t *limits,
                             mulib_depfile_rule_fn callback, void *user,
                             mulib_depfile_error_t *error);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_DEPFILE) && !defined(MULIB_DEPFILE_IMPLEMENTED)
#define MULIB_DEPFILE_IMPLEMENTED

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mulib_depfile_words_ {
  char **items;
  size_t count, capacity;
} mulib_depfile_words_;

static mulib_depfile_limits_t
mulib_depfile_limits_(const mulib_depfile_limits_t *provided)
{
  mulib_depfile_limits_t limits;
  limits.max_source_bytes = 16u * 1024u * 1024u;
  limits.max_rules = 65536;
  limits.max_words = 262144;
  limits.max_word_bytes = 4095;
  if (provided) {
    if (provided->max_source_bytes)
      limits.max_source_bytes = provided->max_source_bytes;
    if (provided->max_rules)
      limits.max_rules = provided->max_rules;
    if (provided->max_words)
      limits.max_words = provided->max_words;
    if (provided->max_word_bytes)
      limits.max_word_bytes = provided->max_word_bytes;
  }
  return limits;
}

static void
mulib_depfile_error_(mulib_depfile_error_t *error, const char *source,
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
mulib_depfile_words_free_(mulib_depfile_words_ *words)
{
  size_t i;
  if (!words)
    return;
  for (i = 0; i < words->count; ++i)
    free(words->items[i]);
  free(words->items);
  memset(words, 0, sizeof(*words));
}

static int
mulib_depfile_word_add_(mulib_depfile_words_ *words, const char *word,
                        size_t length, size_t max_words, size_t max_word)
{
  char *copy;
  if (!length)
    return MULIB_DEPFILE_OK;
  if (length > max_word || words->count == max_words)
    return MULIB_DEPFILE_LIMIT;
  if (words->count == words->capacity) {
    size_t capacity = words->capacity ? words->capacity * 2 : 8;
    char **items;
    if (capacity <= words->capacity || capacity > max_words)
      capacity = max_words;
    if (capacity <= words->capacity
        || capacity > SIZE_MAX / sizeof(*items))
      return MULIB_DEPFILE_LIMIT;
    items = (char **)realloc(words->items, capacity * sizeof(*items));
    if (!items)
      return MULIB_DEPFILE_MEMORY;
    words->items = items;
    words->capacity = capacity;
  }
  copy = (char *)malloc(length + 1);
  if (!copy)
    return MULIB_DEPFILE_MEMORY;
  memcpy(copy, word, length);
  copy[length] = '\0';
  words->items[words->count++] = copy;
  return MULIB_DEPFILE_OK;
}

static int
mulib_depfile_split_words_(const char *text, size_t length,
                           const mulib_depfile_limits_t *limits,
                           size_t remaining_words,
                           mulib_depfile_words_ *out)
{
  char *word;
  size_t i = 0, used = 0;
  int escaped = 0, rc = MULIB_DEPFILE_OK;
  if (!remaining_words || limits->max_word_bytes == SIZE_MAX)
    return length ? MULIB_DEPFILE_LIMIT : MULIB_DEPFILE_OK;
  word = (char *)malloc(limits->max_word_bytes + 1);
  if (!word)
    return MULIB_DEPFILE_MEMORY;
  while (i < length) {
    unsigned char c = (unsigned char)text[i];
    if (escaped) {
      if (used == limits->max_word_bytes) {
        rc = MULIB_DEPFILE_LIMIT;
        break;
      }
      word[used++] = (char)c;
      escaped = 0;
    } else if (c == '\\') {
      escaped = 1;
    } else if (isspace(c)) {
      if (used) {
        rc = mulib_depfile_word_add_(out, word, used, remaining_words,
                                     limits->max_word_bytes);
        if (rc)
          break;
        --remaining_words;
        used = 0;
      }
    } else {
      if (used == limits->max_word_bytes) {
        rc = MULIB_DEPFILE_LIMIT;
        break;
      }
      word[used++] = (char)c;
    }
    ++i;
  }
  if (!rc && escaped) {
    if (used == limits->max_word_bytes)
      rc = MULIB_DEPFILE_LIMIT;
    else
      word[used++] = '\\';
  }
  if (!rc && used)
    rc = mulib_depfile_word_add_(out, word, used, remaining_words,
                                 limits->max_word_bytes);
  free(word);
  return rc;
}

static size_t
mulib_depfile_comment_(const char *line, size_t length)
{
  size_t i, slashes = 0;
  for (i = 0; i < length; ++i) {
    if (line[i] == '#') {
      if (!(slashes & 1u))
        return i;
      slashes = 0;
    } else if (line[i] == '\\')
      ++slashes;
    else
      slashes = 0;
  }
  return length;
}

static size_t
mulib_depfile_colon_(const char *line, size_t length)
{
  size_t i, slashes = 0;
  for (i = 0; i < length; ++i) {
    unsigned char c = (unsigned char)line[i];
    if (c == ':' && !(slashes & 1u)) {
      if (i == 1
          && ((line[0] >= 'A' && line[0] <= 'Z')
              || (line[0] >= 'a' && line[0] <= 'z'))) {
        slashes = 0;
        continue;
      }
      return i;
    }
    if (c == '\\')
      ++slashes;
    else
      slashes = 0;
  }
  return SIZE_MAX;
}

static int
mulib_depfile_normalize_(const char *source, size_t length, char **out,
                         size_t *out_length)
{
  char *text;
  size_t i = 0, used = 0, trailing = 0;
  if (length == SIZE_MAX)
    return MULIB_DEPFILE_LIMIT;
  text = (char *)malloc(length + 1);
  if (!text)
    return MULIB_DEPFILE_MEMORY;
  while (i < length) {
    char c = source[i++];
    if (!c) {
      free(text);
      return MULIB_DEPFILE_INVALID;
    }
    if (c == '\r' || c == '\n') {
      size_t newline = i - 1;
      if (c == '\r' && i < length && source[i] == '\n')
        ++i;
      if (trailing & 1u) {
        --used;
        text[used++] = ' ';
      } else
        text[used++] = '\n';
      trailing = 0;
      (void)newline;
    } else {
      text[used++] = c;
      if (c == '\\')
        ++trailing;
      else
        trailing = 0;
    }
  }
  if (trailing & 1u) {
    free(text);
    return MULIB_DEPFILE_INVALID;
  }
  text[used] = '\0';
  *out = text;
  *out_length = used;
  return MULIB_DEPFILE_OK;
}

int
mulib_depfile_parse(const char *source, size_t length,
                    const mulib_depfile_limits_t *provided,
                    mulib_depfile_rule_fn callback, void *user,
                    mulib_depfile_error_t *error)
{
  mulib_depfile_limits_t limits = mulib_depfile_limits_(provided);
  char *text = NULL;
  size_t text_length = 0, start = 0, rules = 0, words = 0;
  int rc;
  if (error)
    memset(error, 0, sizeof(*error));
  if ((!source && length) || !callback || length > limits.max_source_bytes) {
    mulib_depfile_error_(error, source ? source : "", source ? length : 0, 0,
                         "invalid depfile input or limits");
    return length > limits.max_source_bytes ? MULIB_DEPFILE_LIMIT
                                            : MULIB_DEPFILE_INVALID;
  }
  rc = mulib_depfile_normalize_(source ? source : "", length, &text,
                                &text_length);
  if (rc) {
    mulib_depfile_error_(error, source ? source : "", source ? length : 0,
                         length, rc == MULIB_DEPFILE_INVALID
                                     ? "embedded NUL or unterminated continuation"
                                     : "unable to normalize depfile");
    return rc;
  }
  while (start <= text_length) {
    size_t end = start, left, right, colon, comment;
    mulib_depfile_words_ targets = {0}, dependencies = {0};
    mulib_depfile_rule_t rule;
    while (end < text_length && text[end] != '\n')
      ++end;
    left = start;
    right = end;
    while (left < right && isspace((unsigned char)text[left]))
      ++left;
    while (right > left && isspace((unsigned char)text[right - 1]))
      --right;
    comment = mulib_depfile_comment_(text + left, right - left);
    right = left + comment;
    while (right > left && isspace((unsigned char)text[right - 1]))
      --right;
    if (right > left) {
      if (rules == limits.max_rules) {
        rc = MULIB_DEPFILE_LIMIT;
        mulib_depfile_error_(error, text, text_length, left,
                             "depfile rule limit exceeded");
        goto done;
      }
      colon = mulib_depfile_colon_(text + left, right - left);
      if (colon == SIZE_MAX) {
        rc = MULIB_DEPFILE_INVALID;
        mulib_depfile_error_(error, text, text_length, left,
                             "dependency rule has no ':' separator");
        goto done;
      }
      rc = mulib_depfile_split_words_(text + left, colon, &limits,
                                      limits.max_words - words, &targets);
      if (!rc)
        rc = mulib_depfile_split_words_(
            text + left + colon + 1, right - left - colon - 1, &limits,
            limits.max_words - words - targets.count, &dependencies);
      if (rc) {
        mulib_depfile_words_free_(&targets);
        mulib_depfile_words_free_(&dependencies);
        mulib_depfile_error_(error, text, text_length, left,
                             rc == MULIB_DEPFILE_LIMIT
                                 ? "depfile word limit exceeded"
                                 : "unable to allocate depfile words");
        goto done;
      }
      if (!targets.count) {
        mulib_depfile_words_free_(&targets);
        mulib_depfile_words_free_(&dependencies);
        rc = MULIB_DEPFILE_INVALID;
        mulib_depfile_error_(error, text, text_length, left,
                             "dependency rule has no target");
        goto done;
      }
      words += targets.count + dependencies.count;
      ++rules;
      rule.targets = (const char *const *)targets.items;
      rule.target_count = targets.count;
      rule.dependencies = (const char *const *)dependencies.items;
      rule.dependency_count = dependencies.count;
      rc = callback(user, &rule);
      mulib_depfile_words_free_(&targets);
      mulib_depfile_words_free_(&dependencies);
      if (rc) {
        mulib_depfile_error_(error, text, text_length, left,
                             "depfile callback stopped parsing");
        goto done;
      }
    }
    if (end == text_length)
      break;
    start = end + 1;
  }
  rc = MULIB_DEPFILE_OK;
done:
  free(text);
  return rc;
}

int
mulib_depfile_parse_file(const char *path,
                         const mulib_depfile_limits_t *provided,
                         mulib_depfile_rule_fn callback, void *user,
                         mulib_depfile_error_t *error)
{
  mulib_depfile_limits_t limits = mulib_depfile_limits_(provided);
  FILE *stream;
  char *data = NULL;
  size_t length = 0, capacity = 0;
  int rc;
  if (error)
    memset(error, 0, sizeof(*error));
  if (!path || !*path || !callback)
    return MULIB_DEPFILE_INVALID;
  stream = fopen(path, "rb");
  if (!stream) {
    if (error)
      error->message = "unable to open depfile";
    return MULIB_DEPFILE_IO;
  }
  for (;;) {
    size_t got;
    if (length == capacity) {
      size_t next = capacity ? capacity * 2 : 4096;
      char *grown;
      if (next <= capacity || next > limits.max_source_bytes)
        next = limits.max_source_bytes;
      if (next <= capacity) {
        int byte = fgetc(stream);
        if (byte != EOF) {
          free(data);
          fclose(stream);
          if (error)
            error->message = "depfile source limit exceeded";
          return MULIB_DEPFILE_LIMIT;
        }
        if (ferror(stream)) {
          free(data);
          fclose(stream);
          if (error)
            error->message = "unable to read depfile";
          return MULIB_DEPFILE_IO;
        }
        break;
      }
      grown = (char *)realloc(data, next);
      if (!grown) {
        free(data);
        fclose(stream);
        return MULIB_DEPFILE_MEMORY;
      }
      data = grown;
      capacity = next;
    }
    got = fread(data + length, 1, capacity - length, stream);
    length += got;
    if (got == 0) {
      if (ferror(stream)) {
        free(data);
        fclose(stream);
        if (error)
          error->message = "unable to read depfile";
        return MULIB_DEPFILE_IO;
      }
      break;
    }
  }
  if (fclose(stream)) {
    free(data);
    if (error)
      error->message = "unable to close depfile";
    return MULIB_DEPFILE_IO;
  }
  rc = mulib_depfile_parse(data, length, &limits, callback, user, error);
  free(data);
  return rc;
}

#endif
