/** @file mulib-termscript-library.h
 * @brief Filesystem-backed Termscript modules on explicitly configured roots.
 * Define MULIB_IMPL_TERMSCRIPT_LIBRARY plus the parser and strbuf implementations.
 */
#ifndef MULIB_TERMSCRIPT_LIBRARY_H
#define MULIB_TERMSCRIPT_LIBRARY_H
#include "mulib-termscript-ast.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_TERMSCRIPT_LIBRARY_PATH "TERMSCRIPT_LIBRARY_PATH"
#define MULIB_TERMSCRIPT_LIBRARY_FILE_MAX (1024u * 1024u)

/* Search a colon-separated list, left-to-right, then the optional standard
 * library root. NULL search_path reads TERMSCRIPT_LIBRARY_PATH; empty
 * components are ignored. "one.two" resolves to "one/two.term". Only
 * ASCII identifier-like components are allowed; path traversal is rejected.
 * Roots must be trusted by the embedding application. The first existing
 * candidate must parse; do not fall through on malformed or unreadable files.
 * Start with an empty initialized destination; free before reuse. Failure
 * leaves it unchanged. */
int mulib_termscript_library_load(const char *search_path, const char *stdlib_root,
                                   const char *module, size_t length,
                                   mulib_termscript_program_t *program,
                                   mulib_termscript_error_t *error);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSCRIPT_LIBRARY) && !defined(MULIB_TERMSCRIPT_LIBRARY_IMPLEMENTED)
#define MULIB_TERMSCRIPT_LIBRARY_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

static int
mulib_termscript_library_error_(mulib_termscript_error_t *error, const char *message)
{
  if (error) {
    error->offset = 0;
    error->message = message;
  }
  return 0;
}

static int
mulib_termscript_library_name_(const char *module, size_t n, char path[261])
{
  if (!module || !n || n > 255) return 0;
  int start = 1;
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)module[i];
    if (c == '.') {
      if (start) return 0;
      path[i] = '/';
      start = 1;
    } else {
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
            || (!start && ((c >= '0' && c <= '9') || c == '-')))) return 0;
      path[i] = (char)c;
      start = 0;
    }
  }
  if (start) return 0;
  memcpy(path + n, ".term", 6);
  return 1;
}

/* -1 is not found; 0 selected but unusable; 1 loaded. */
static int
mulib_termscript_library_try_(const char *root, size_t n, const char *file,
                               mulib_termscript_program_t *program, mulib_termscript_error_t *error)
{
  char path[4096], bytes[4096];
  size_t tail = strlen(file);
  if (!n) return -1;
  if (n > sizeof path - tail - 2)
    return mulib_termscript_library_error_(error, "library path too long");
  memcpy(path, root, n);
  path[n] = '/';
  memcpy(path + n + 1, file, tail + 1);
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    if (errno == ENOENT || errno == ENOTDIR) return -1;
    return mulib_termscript_library_error_(error, "unable to open library module");
  }
  mulib_strbuf_t src = { NULL, 0, 0 };
  const char *problem = NULL;
  for (;;) {
    size_t got = fread(bytes, 1, sizeof bytes, fp);
    if (got > MULIB_TERMSCRIPT_LIBRARY_FILE_MAX - src.length) {
      problem = "library module too large";
      break;
    }
    if (got && !mulib_strbuf_append_n(&src, bytes, got)) {
      problem = "out of memory";
      break;
    }
    if (got < sizeof bytes) {
      if (ferror(fp)) problem = "unable to read library module";
      break;
    }
  }
  if (fclose(fp) && !problem) problem = "unable to close library module";
  int result = problem ? mulib_termscript_library_error_(error, problem)
    : mulib_termscript_parse(src.data ? src.data : "", src.length, program, error);
  mulib_strbuf_free(&src);
  return result;
}

int
mulib_termscript_library_load(const char *path, const char *stdlib_root,
                              const char *module, size_t n, mulib_termscript_program_t *program,
                              mulib_termscript_error_t *error)
{
  char file[261];
  if (!program || program->source || program->nodes)
    return mulib_termscript_library_error_(error, "expected empty program storage");
  if (!mulib_termscript_library_name_(module, n, file))
    return mulib_termscript_library_error_(error, "invalid library module name");
  if (!path) path = getenv(MULIB_TERMSCRIPT_LIBRARY_PATH);
  while (path && *path) {
    const char *end = strchr(path, ':');
    int result = mulib_termscript_library_try_(path, end ? (size_t)(end - path) : strlen(path),
                                               file, program, error);
    if (result >= 0) return result;
    path = end ? end + 1 : NULL;
  }
  if (stdlib_root) {
    int result = mulib_termscript_library_try_(stdlib_root, strlen(stdlib_root), file, program, error);
    if (result >= 0) return result;
  }
  return mulib_termscript_library_error_(error, "library module not found");
}

#ifdef __cplusplus
}
#endif
#endif
