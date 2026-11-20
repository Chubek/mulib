/* Run a trusted Termscript source or emit an ISO C90 translation unit. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "implementation.h"
#include "capasm-tools.h"
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
  return (!n || fwrite(bytes, 1, n, fp) == n) && !fflush(fp);
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
               "Compile only (artifact on stdout, C90 by default):\n"
               "  --rewrite NAME [--rewrite NAME ...] [--m4 PROGRAM] [--rewrite-path PATH]\n"
               "  --kernel NAME (repeat for declaration includes; selected headers may require C11)\n"
               "  --emit c90|preprocessed|asm|obj|exe\n"
               "  --cpp PROGRAM  --cc PROGRAM  --as PROGRAM  --ld DRIVER\n"
               "  --cpp-arg ARG  --cc-arg ARG   --as-arg ARG  --ld-arg ARG  --link-input FILE\n"
               "  Program arguments and link inputs may be repeated; exe requires explicit link inputs or arguments.\n"
               "FILE '-' reads stdin. Use only trusted tools and rewrite/library roots.\n", exe);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* A program is one executable pathname or PATH name, never a command line.
   Each argv option consumes exactly one argument, even one starting with '-'. */
static int
compile_option(capasm_tools_t *tools, int argc, char **argv, int *i)
{
  const char *name = argv[*i];
  const char **program = NULL;
  capasm_tool_args_t *args = NULL;
  if (!strcmp(name, "--m4")) program = &tools->m4;
  else if (!strcmp(name, "--rewrite-path")) program = &tools->rewrite_path;
  else if (!strcmp(name, "--cpp")) program = &tools->cpp;
  else if (!strcmp(name, "--cc")) program = &tools->cc;
  else if (!strcmp(name, "--as")) program = &tools->as;
  else if (!strcmp(name, "--ld")) program = &tools->ld;
  else if (!strcmp(name, "--rewrite")) args = &tools->rewrites;
  else if (!strcmp(name, "--kernel")) args = &tools->kernels;
  else if (!strcmp(name, "--cpp-arg")) args = &tools->cpp_args;
  else if (!strcmp(name, "--cc-arg")) args = &tools->cc_args;
  else if (!strcmp(name, "--as-arg")) args = &tools->as_args;
  else if (!strcmp(name, "--ld-arg")) args = &tools->ld_args;
  else if (!strcmp(name, "--link-input")) args = &tools->link_inputs;
  else if (strcmp(name, "--emit")) return 0;
  if (*i + 1 == argc) return -1;
  const char *value = argv[++*i];
  if (program) {
    if (!*value && program != &tools->rewrite_path) return -1;
    *program = value;
  } else if (args) {
    if ((args == &tools->rewrites && !capasm_tool_rewrite_name(value)) ||
        (args == &tools->link_inputs && !*value) || !capasm_tool_arg(args, value)) return -1;
  } else {
    if (!strcmp(value, "c90")) tools->output = CAPASM_TOOL_C90;
    else if (!strcmp(value, "preprocessed")) tools->output = CAPASM_TOOL_PREPROCESSED;
    else if (!strcmp(value, "asm")) tools->output = CAPASM_TOOL_ASSEMBLY;
    else if (!strcmp(value, "obj")) tools->output = CAPASM_TOOL_OBJECT;
    else if (!strcmp(value, "exe")) tools->output = CAPASM_TOOL_EXECUTABLE;
    else return -1;
  }
  return 1;
}

static const char *
compile_configuration(const capasm_tools_t *tools)
{
  if ((tools->m4 || tools->rewrite_path) && !tools->rewrites.count)
    return "--m4 and --rewrite-path require --rewrite";
  if (tools->output == CAPASM_TOOL_C90 && (tools->cpp || tools->cpp_args.count ||
                                         tools->cc || tools->cc_args.count))
    return "compiler and preprocessor options require a tool output via --emit";
  if (tools->output == CAPASM_TOOL_PREPROCESSED && tools->cc_args.count)
    return "use --cpp-arg for --emit preprocessed";
  if (tools->output == CAPASM_TOOL_PREPROCESSED && tools->cc && tools->cpp)
    return "--cc is unused with --emit preprocessed and --cpp";
  if (tools->output < CAPASM_TOOL_OBJECT && (tools->as || tools->as_args.count))
    return "assembler options require --emit obj or --emit exe";
  if (tools->output != CAPASM_TOOL_EXECUTABLE && (tools->ld || tools->ld_args.count || tools->link_inputs.count))
    return "linker options require --emit exe";
  if (tools->output == CAPASM_TOOL_EXECUTABLE && !tools->ld_args.count && !tools->link_inputs.count)
    return "--emit exe requires explicit --link-input or --ld-arg";
#ifndef MULIB_CAPASM_TOOLS_POSIX
  if (tools->rewrites.count || tools->output != CAPASM_TOOL_C90)
    return "external compile tools require POSIX process support";
#endif
  return NULL;
}

int
main(int argc, char **argv)
{
  const char *entry = NULL, *path = NULL, *db_path = NULL, *profile = NULL;
  const char *root = MULIB_TERMSCRIPT_STDLIB;
  capasm_tools_t tools = { 0 };
  int xterm = 0, ansi = 0, shell = 0, tty = 0, has_compile_option = 0;
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
    else {
      int opt = compile_option(&tools, argc, argv, &i);
      if (opt > 0) has_compile_option = 1;
      else if (!opt && !path && (s[0] != '-' || !s[1])) path = s;
      else {
        if (opt < 0) fprintf(stderr, "Termscript: invalid or excessive %s argument\n", s);
        return help(argv[0], 0);
      }
    }
  }
  if (!path) return help(argv[0], 0);
  const char *config_error = entry ? compile_configuration(&tools) :
    has_compile_option ? "tool and rewrite options require --compile ENTRY" : NULL;
  if (config_error) {
    fprintf(stderr, "Termscript: %s\n", config_error);
    return EXIT_FAILURE;
  }
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
    size_t length = 0;
    ok = capasm_tool_emit(&tools, src.data, src.length, entry, &c, &length, &err);
    if (ok) ok = output(stdout, c, length);
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
