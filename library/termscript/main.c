/* Run a trusted Termscript source or emit a standalone C11 translation unit. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "implementation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#ifndef MULIB_TERMSCRIPT_STDLIB
#define MULIB_TERMSCRIPT_STDLIB "library/termscript"
#endif

static int
source_file(const char *path, mulib_strbuf_t *out)
{
  FILE *fp = !strcmp(path, "-") ? stdin : fopen(path, "rb");
  if (!fp) return 0;
  char bytes[4096];
  size_t got = 0;
  int ok = 1;
  while ((got = fread(bytes, 1, sizeof bytes, fp)) != 0) {
    if (got > MULIB_TERMSCRIPT_LIBRARY_FILE_MAX - out->length || !mulib_strbuf_append_n(out, bytes, got)) {
      ok = 0;
      break;
    }
  }
  if (ferror(fp)) ok = 0;
  if (fp != stdin && fclose(fp)) ok = 0;
  return ok;
}

static int
output(void *stream, const char *bytes, size_t n)
{
  FILE *fp = stream;
  return fwrite(bytes, 1, n, fp) == n && !fflush(fp);
}

static int
shell_command(void *context, const char *bytes, size_t n)
{
  (void)context;
  if (n > 64 * 1024 || memchr(bytes, 0, n)) return 0;
  mulib_strbuf_t command = { NULL, 0, 0 };
  int ok = mulib_strbuf_append_n(&command, bytes, n) && system(command.data) == 0;
  mulib_strbuf_free(&command);
  return ok;
}

static int
help(const char *exe, int ok)
{
  FILE *out = ok ? stdout : stderr;
  fprintf(out, "Usage: %s [--compile ENTRY | --ansi | --xterm | --termdb PATH]\n"
               "       [--profile PROFILE] [--library ROOT] [--allow-shell] [--tty] FILE\n"
               "FILE '-' reads stdin. Run only trusted source and library roots.\n", exe);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
main(int argc, char **argv)
{
  const char *entry = NULL, *path = NULL, *db_path = NULL, *profile = NULL;
  const char *root = MULIB_TERMSCRIPT_STDLIB;
  int xterm = 0, ansi = 0, shell = 0, tty = 0;
  for (int i = 1; i < argc; ++i) {
    const char *s = argv[i];
    if (!strcmp(s, "--help")) return help(argv[0], 1);
    if (!strcmp(s, "--compile") && i + 1 < argc) entry = argv[++i];
    else if (!strcmp(s, "--profile") && i + 1 < argc) profile = argv[++i];
    else if (!strcmp(s, "--termdb") && i + 1 < argc) db_path = argv[++i];
    else if (!strcmp(s, "--library") && i + 1 < argc) root = argv[++i];
    else if (!strcmp(s, "--ansi")) ansi = 1;
    else if (!strcmp(s, "--xterm")) ansi = xterm = 1;
    else if (!strcmp(s, "--allow-shell")) shell = 1;
    else if (!strcmp(s, "--tty")) tty = 1;
    else if (!path && (s[0] != '-' || !s[1])) path = s;
    else return help(argv[0], 0);
  }
  if (!path) return help(argv[0], 0);
  mulib_strbuf_t src = { NULL, 0, 0 };
  if (!source_file(path, &src)) {
    fprintf(stderr, "%s: cannot read bounded Termscript source\n", path);
    mulib_strbuf_free(&src);
    return EXIT_FAILURE;
  }

  mulib_termscript_error_t err = { 0, NULL };
  int ok = 0;
  if (entry) {
    char *c = NULL;
    ok = mulib_termscript_compile_c11(src.data, src.length, entry, &c, &err);
    if (ok) ok = output(stdout, c, strlen(c));
    free(c);
  } else {
    mulib_termscript_program_t program = { NULL, 0, NULL, 0, SIZE_MAX };
    mulib_termscript_vm_t vm;
    mulib_termscript_terminal_t terminal;
    mulib_terminfo_t info;
    mulib_termdb_t db = { 0 };
    int fd = -1;
#if defined(__unix__) || defined(__APPLE__)
    if (tty && isatty(STDIN_FILENO)) fd = STDIN_FILENO;
#endif
    if (tty && fd < 0) err.message = "--tty requires a POSIX terminal on standard input";
    else if (db_path && !mulib_termdb_open(&db, db_path)) err.message = "unable to open TermDB";
    else {
      mulib_terminfo_init(&info);
      if (ansi) mulib_terminfo_ansi(&info);
      mulib_termscript_terminal_init(&terminal, ansi ? &info : NULL,
        xterm ? MULIB_TERMFUNC_XTERM_MODES : 0, fd, output, stdout);
      mulib_termscript_host_t host = {
        &terminal, db_path ? &db : NULL, profile ? profile : (xterm ? "xterm" : NULL),
        NULL, root, shell ? shell_command : NULL, NULL
      };
      mulib_termscript_vm_init(&vm, NULL);
      mulib_termscript_host_attach(&vm, &host);
      const char *term = getenv("TERM");
      if (term && !mulib_termscript_vm_set_term(&vm, term, strlen(term)))
        err.message = "unable to copy TERM";
      else
        ok = mulib_termscript_run_source(&vm, src.data, src.length, &program, &err);
      if (mulib_termscript_terminal_restore(&terminal) != MULIB_TERMUTILS_OK) {
        fputs("Termscript: unable to restore tty attributes\n", stderr);
        ok = 0;
      }
      mulib_termscript_vm_clear(&vm);
      mulib_terminfo_free(&info);
    }
    mulib_termscript_program_free(&program);
    if (db_path) (void)mulib_termdb_close(&db);
  }
  mulib_strbuf_free(&src);
  if (!ok) fprintf(stderr, "%s:%zu: %s\n", path, err.offset,
                   err.message ? err.message : "Termscript I/O failed");
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
