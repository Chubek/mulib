/* Host-only tool stages for the Termscript CLI. Tools and their arguments are
   trusted CLI configuration, not capabilities granted to a source program. */
#ifndef MULIB_TERMSCRIPT_CAPASM_TOOLS_H
#define MULIB_TERMSCRIPT_CAPASM_TOOLS_H

#include "mulib-termscript-capasm.h"
#include "mulib-strbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MULIB_XMACIR_TRS
#define MULIB_XMACIR_TRS "library/xmacir-trs"
#endif
#ifndef MULIB_CAPASM_TOOL_OUTPUT_MAX
#define MULIB_CAPASM_TOOL_OUTPUT_MAX (16u * 1024u * 1024u)
#endif
#ifndef MULIB_CAPASM_TOOL_TIMEOUT
#define MULIB_CAPASM_TOOL_TIMEOUT 30 /* wall-clock seconds per invocation */
#endif
#define MULIB_CAPASM_TOOL_ARG_MAX 64
#define MULIB_CAPASM_TOOL_ARGV_MAX (2 * MULIB_CAPASM_TOOL_ARG_MAX + 10)

typedef enum {
  CAPASM_TOOL_C90, CAPASM_TOOL_PREPROCESSED, CAPASM_TOOL_ASSEMBLY,
  CAPASM_TOOL_OBJECT, CAPASM_TOOL_EXECUTABLE
} capasm_tool_output_t;

typedef struct {
  const char *values[MULIB_CAPASM_TOOL_ARG_MAX];
  size_t count;
} capasm_tool_args_t;

typedef struct {
  capasm_tool_output_t output;
  const char *m4, *rewrite_path, *cpp, *cc, *as, *ld;
  capasm_tool_args_t rewrites, cpp_args, cc_args, as_args, ld_args, link_inputs, kernels;
} capasm_tools_t;

typedef struct {
  mulib_capasm_options_t options;
  mulib_capasm_tool_t passes[MULIB_CAPASM_TOOL_ARG_MAX], stages[4];
  const char *pass_args[MULIB_CAPASM_TOOL_ARG_MAX][3], *stage_args[4][MULIB_CAPASM_TOOL_ARGV_MAX];
  mulib_strbuf_t pass_paths[MULIB_CAPASM_TOOL_ARG_MAX], link_paths[MULIB_CAPASM_TOOL_ARG_MAX];
  mulib_strbuf_t dir, object, executable;
  int have_dir;
} capasm_tool_context_t;

static int
capasm_tool_arg(capasm_tool_args_t *args, const char *value)
{
  if (args->count == MULIB_CAPASM_TOOL_ARG_MAX) return 0;
  args->values[args->count++] = value;
  return 1;
}

static int
capasm_tool_rewrite_name(const char *name)
{
  size_t n = strlen(name);
  if (n > 6 && !strcmp(name + n - 6, ".xmtrs")) n -= 6;
  if (!n || n > 128) return 0;
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
  }
  return 1;
}

#if (defined(__unix__) || defined(__APPLE__)) && !defined(MULIB_CAPASM_TOOLS_NO_POSIX)
#define MULIB_CAPASM_TOOLS_POSIX
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Prefix relative paths so neither "-name" nor "@name" is a tool option. */
static int
capasm_tool_path(mulib_strbuf_t *out, const char *dir, size_t n, const char *name)
{
  return (n && dir[0] == '/' ? 1 : mulib_strbuf_append_n(out, "./", 2)) &&
    (!n || (mulib_strbuf_append_n(out, dir, n) &&
      (dir[n - 1] == '/' || mulib_strbuf_append_n(out, "/", 1)))) &&
    mulib_strbuf_append_n(out, name, strlen(name));
}

/* Colon-separated search roots, ignoring empty components. The directories
   (and any symlink targets) are explicitly within the host trust boundary. */
static int
capasm_tool_search_rewrite(const char *roots, const char *name, mulib_strbuf_t *path)
{
  for (const char *p = roots; p && *p;) {
    const char *end = strchr(p, ':');
    size_t n = end ? (size_t)(end - p) : strlen(p);
    if (n) {
      struct stat st;
      if (!capasm_tool_path(path, p, n, name)) return -1;
      if (!stat(path->data, &st)) return S_ISREG(st.st_mode) ? 1 : -1;
      if (errno != ENOENT && errno != ENOTDIR) return -1;
      mulib_strbuf_free(path);
    }
    if (!end) break;
    p = end + 1;
  }
  return 0;
}

static int
capasm_tool_find_rewrite(const capasm_tools_t *tools, const char *name, mulib_strbuf_t *path)
{
  char filename[128 + sizeof ".xmtrs"];
  size_t n = strlen(name);
  if (!capasm_tool_rewrite_name(name)) return 0;
  (void)snprintf(filename, sizeof filename, "%s%s", name,
    n > 6 && !strcmp(name + n - 6, ".xmtrs") ? "" : ".xmtrs");
  if (tools->rewrite_path)
    return capasm_tool_search_rewrite(tools->rewrite_path, filename, path) == 1;
  int result = capasm_tool_search_rewrite(getenv("XMACIR_REWRITE_PATH"), filename, path);
  return result ? result == 1 :
    capasm_tool_search_rewrite(MULIB_XMACIR_TRS, filename, path) == 1;
}

/* Like PATH lookup, but never reinterpret a file without a valid executable
   format as shell input (the ENOEXEC fallback of execvp). */
static void
capasm_tool_exec(const char *const args[])
{
  if (strchr(args[0], '/')) {
    execv(args[0], (char *const *)args);
    return;
  }
  const char *p = getenv("PATH");
  if (!p) p = "/bin:/usr/bin";
  int inaccessible = 0;
  for (;;) {
    const char *end = strchr(p, ':');
    size_t n = end ? (size_t)(end - p) : strlen(p);
    mulib_strbuf_t candidate = { 0 };
    if (!capasm_tool_path(&candidate, p, n, args[0])) {
      mulib_strbuf_free(&candidate);
      errno = ENOMEM;
      return;
    }
    execv(candidate.data, (char *const *)args);
    int failure = errno;
    mulib_strbuf_free(&candidate);
    if (failure == EACCES) inaccessible = 1;
    else if (failure != ENOENT && failure != ENOTDIR) {
      errno = failure;
      return;
    }
    if (!end) break;
    p = end + 1;
  }
  errno = inaccessible ? EACCES : ENOENT;
}

/* Regular file captures avoid pipe backpressure. The child and tool drivers'
   descendants inherit a file size limit; timeout also kills the process group.
   This is a resource bound for trusted tools, not a sandbox. */
static int
capasm_tool_wait(pid_t pid, int *status)
{
  struct timespec start, now;
  const struct timespec delay = { 0, 10 * 1000 * 1000 };
  int failure = 0, exited = 0;
  if (!clock_gettime(CLOCK_MONOTONIC, &start)) {
    for (;;) {
      siginfo_t info = { 0 };
      if (!waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG | WNOWAIT)) {
        if (info.si_pid == pid) {
          exited = 1;
          break;
        }
      } else if (errno != EINTR) {
        if (errno == ECHILD) return 0;
        failure = errno;
        break;
      }
      if (clock_gettime(CLOCK_MONOTONIC, &now)) {
        failure = errno;
        break;
      }
      time_t seconds = now.tv_sec - start.tv_sec - (now.tv_nsec < start.tv_nsec);
      if (seconds >= MULIB_CAPASM_TOOL_TIMEOUT) {
        failure = ETIMEDOUT;
        break;
      }
      (void)nanosleep(&delay, NULL);
    }
  } else failure = errno;
  /* Keep the leader unreaped until signaling its group, so neither its PID nor
     its process group ID can have been reused. Successful stage tools must
     have completed their own helpers before exiting. */
  (void)kill(-pid, SIGKILL);
  if (!exited) (void)kill(pid, SIGKILL);
  pid_t ended;
  do ended = waitpid(pid, status, 0); while (ended < 0 && errno == EINTR);
  if (failure) errno = failure;
  return exited && ended == pid;
}

static int
capasm_tool_capture(FILE *fp, mulib_strbuf_t *out)
{
  struct stat st;
  if (fstat(fileno(fp), &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
      st.st_size > MULIB_CAPASM_TOOL_OUTPUT_MAX || fseek(fp, 0, SEEK_SET)) return 0;
  char bytes[4096];
  size_t n;
  while ((n = fread(bytes, 1, sizeof bytes, fp)) != 0) {
    if (n > MULIB_CAPASM_TOOL_OUTPUT_MAX - out->length ||
        !mulib_strbuf_append_n(out, bytes, n)) return 0;
  }
  return !ferror(fp);
}

static void
capasm_tool_diagnostics(FILE *fp)
{
  char bytes[4096];
  if (!fseek(fp, 0, SEEK_SET)) {
    size_t n = fread(bytes, 1, sizeof bytes, fp);
    if (n) (void)fwrite(bytes, 1, n, stderr);
    if (n == sizeof bytes && fgetc(fp) != EOF)
      fputs("\nTermscript: tool diagnostics truncated\n", stderr);
  }
}

static int
capasm_tool_run(const char *stage, const char *const args[],
  const char *input, size_t length, const char *artifact, mulib_strbuf_t *out)
{
  FILE *in = tmpfile(), *capture = tmpfile(), *diag = tmpfile(), *result = NULL;
  mulib_strbuf_t bytes = { 0 };
  int ok = 0;
  if (!in || !capture || !diag || fileno(in) <= 2 || fileno(capture) <= 2 || fileno(diag) <= 2 ||
      (length && fwrite(input, 1, length, in) != length) || fflush(in) || fseek(in, 0, SEEK_SET)) {
    fprintf(stderr, "Termscript: %s: cannot prepare tool temporary files\n", stage);
    goto done;
  }
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "Termscript: %s: cannot start %s: %s\n", stage, args[0], strerror(errno));
    goto done;
  }
  if (!pid) {
    struct rlimit limit;
    if (setpgid(0, 0) || dup2(fileno(in), STDIN_FILENO) < 0 ||
        dup2(fileno(capture), STDOUT_FILENO) < 0 || dup2(fileno(diag), STDERR_FILENO) < 0)
      _exit(125);
    (void)close(fileno(in));
    (void)close(fileno(capture));
    (void)close(fileno(diag));
    if (getrlimit(RLIMIT_FSIZE, &limit)) _exit(125);
    if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > MULIB_CAPASM_TOOL_OUTPUT_MAX + 1u) {
      limit.rlim_cur = MULIB_CAPASM_TOOL_OUTPUT_MAX + 1u;
      if (setrlimit(RLIMIT_FSIZE, &limit)) _exit(125);
    }
    capasm_tool_exec(args);
    int failure = errno;
    (void)dprintf(STDERR_FILENO, "cannot execute %s: %s\n", args[0], strerror(failure));
    _exit(failure == ENOENT ? 127 : 126);
  }
  (void)setpgid(pid, pid);
  int status;
  if (!capasm_tool_wait(pid, &status)) {
    int failure = errno;
    fprintf(stderr, "Termscript: %s: %s\n", stage,
      failure == ETIMEDOUT ? "tool timed out" : "could not wait for tool");
    capasm_tool_diagnostics(diag);
    goto done;
  }
  capasm_tool_diagnostics(diag);
  if (!WIFEXITED(status) || WEXITSTATUS(status)) {
    if (WIFEXITED(status))
      fprintf(stderr, "Termscript: %s: %s exited with status %d\n", stage, args[0], WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
      fprintf(stderr, "Termscript: %s: %s terminated by signal %d\n", stage, args[0], WTERMSIG(status));
    goto done;
  }
  if (artifact) {
    int fd = open(artifact, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (fd >= 0) {
      result = fdopen(fd, "rb");
      if (!result) (void)close(fd);
    }
  }
  if (!(artifact ? result && capasm_tool_capture(result, &bytes) && bytes.length :
                   capasm_tool_capture(capture, &bytes))) {
    fprintf(stderr, "Termscript: %s: unreadable, missing or oversized tool output\n", stage);
    goto done;
  }
  mulib_strbuf_free(out);
  *out = bytes;
  bytes = (mulib_strbuf_t){ 0 };
  ok = 1;
done:
  if (in) (void)fclose(in);
  if (capture) (void)fclose(capture);
  if (diag) (void)fclose(diag);
  if (result) (void)fclose(result);
  mulib_strbuf_free(&bytes);
  return ok;
}

static int
capasm_tool_callback(void *user, mulib_capasm_stage_t stage, const mulib_capasm_tool_t *tool,
  const char *input, size_t length, char **output, size_t *output_length,
  mulib_xmacir_error_t *error)
{
  capasm_tool_context_t *ctx = user;
  const char *args[MULIB_CAPASM_TOOL_ARGV_MAX], *artifact = NULL;
  mulib_strbuf_t data = { 0 };
  const char *problem = "external CAPASM tool failed";
  if (stage == MULIB_CAPASM_REWRITE) {
    /* CAPASM supplies canonical XMacIR, whose validated identifiers cannot
       start with m4_. Literal source bytes are hex data, not m4 tokens. */
    if (length > MULIB_CAPASM_TOOL_OUTPUT_MAX || (length && memchr(input, 0, length))) {
      problem = "rewrite input exceeds the host text limit or contains NUL";
      goto fail;
    }
  } else if (stage == MULIB_CAPASM_ASSEMBLE) {
    artifact = ctx->object.data;
  } else if (stage == MULIB_CAPASM_LINK) {
    /* The validated object was already produced at the private pathname
       by the preceding assembler stage. A driver takes filenames, not an
       object stream on stdin. Other inputs are trusted explicit options. */
    artifact = ctx->executable.data;
    input = NULL;
    length = 0;
  }
  args[0] = tool->executable;
  for (size_t i = 0; i < tool->argc; ++i) args[i + 1] = tool->argv[i];
  args[tool->argc + 1] = NULL;
  if (!capasm_tool_run(tool->name, args, input, length, artifact, &data)) goto fail;
  if (!data.length) {
    problem = "external CAPASM tool produced empty output";
    goto fail;
  }
  *output = data.data;
  *output_length = data.length;
  return 1;
fail:
  mulib_strbuf_free(&data);
  if (error) {
    error->offset = 0;
    error->message = problem;
  }
  return 0;
}

static size_t
capasm_tool_copy_args(const char **dest, const capasm_tool_args_t *from)
{
  for (size_t i = 0; i < from->count; ++i) dest[i] = from->values[i];
  return from->count;
}

static int
capasm_tool_options(const capasm_tools_t *tools, capasm_tool_context_t *ctx)
{
  const char *cc = tools->cc ? tools->cc : "cc";
  const char **args;
  size_t n;
  ctx->options.run = capasm_tool_callback;
  ctx->options.user = ctx;
  ctx->options.passes = ctx->passes;
  ctx->options.pass_count = tools->rewrites.count;
  for (size_t i = 0; i < tools->rewrites.count; ++i) {
    const char *name = tools->rewrites.values[i];
    if (!capasm_tool_find_rewrite(tools, name, &ctx->pass_paths[i])) {
      fprintf(stderr, "Termscript: cannot resolve rewrite '%s' in trusted rewrite directories\n", name);
      return 0;
    }
    args = ctx->pass_args[i];
    args[0] = "-P";
    args[1] = ctx->pass_paths[i].data;
    args[2] = "-";
    ctx->passes[i] = (mulib_capasm_tool_t){ name, tools->m4 ? tools->m4 : "m4", args, 3 };
  }
  if (tools->output == CAPASM_TOOL_C90) return 1;
  if (tools->output == CAPASM_TOOL_PREPROCESSED || tools->cpp || tools->cpp_args.count) {
    args = ctx->stage_args[0];
    n = capasm_tool_copy_args(args, &tools->cpp_args);
    args[n++] = "-E";
    args[n++] = "-x";
    args[n++] = "c";
    args[n++] = tools->kernels.count ? "-std=c11" : "-std=c90";
    args[n++] = "-";
    ctx->stages[0] = (mulib_capasm_tool_t){ "preprocess", tools->cpp ? tools->cpp : cc, args, n };
    ctx->options.preprocessor = &ctx->stages[0];
  }
  if (tools->output == CAPASM_TOOL_PREPROCESSED) return 1;
  args = ctx->stage_args[1];
  n = capasm_tool_copy_args(args, &tools->cc_args);
  args[n++] = "-S";
  args[n++] = tools->kernels.count ? "-std=c11" : "-std=c90";
  args[n++] = "-x";
  args[n++] = ctx->options.preprocessor ? "cpp-output" : "c";
  args[n++] = "-";
  args[n++] = "-o";
  args[n++] = "-";
  ctx->stages[1] = (mulib_capasm_tool_t){ "compile to assembly", cc, args, n };
  ctx->options.compiler = &ctx->stages[1];
  if (tools->output == CAPASM_TOOL_ASSEMBLY) return 1;

  const char *dir = getenv("TMPDIR");
  if (!dir || !*dir) dir = "/tmp";
  if (!capasm_tool_path(&ctx->dir, dir, strlen(dir), "mulib-capasm-XXXXXX") || !mkdtemp(ctx->dir.data)) {
    fputs("Termscript: cannot create private staging directory\n", stderr);
    return 0;
  }
  ctx->have_dir = 1;
  if (!capasm_tool_path(&ctx->object, ctx->dir.data, ctx->dir.length, "generated.o")) return 0;
  args = ctx->stage_args[2];
  n = capasm_tool_copy_args(args, &tools->as_args);
  args[n++] = "-o";
  args[n++] = ctx->object.data;
  args[n++] = "-";
  ctx->stages[2] = (mulib_capasm_tool_t){ "assemble", tools->as ? tools->as : "as", args, n };
  ctx->options.assembler = &ctx->stages[2];
  if (tools->output == CAPASM_TOOL_OBJECT) return 1;
  if (!capasm_tool_path(&ctx->executable, ctx->dir.data, ctx->dir.length, "linked")) return 0;
  args = ctx->stage_args[3];
  n = 0;
  args[n++] = ctx->object.data;
  for (size_t i = 0; i < tools->link_inputs.count; ++i) {
    const char *path = tools->link_inputs.values[i];
    if (path[0] == '/') args[n++] = path;
    else {
      if (!capasm_tool_path(&ctx->link_paths[i], NULL, 0, path)) return 0;
      args[n++] = ctx->link_paths[i].data;
    }
  }
  n += capasm_tool_copy_args(args + n, &tools->ld_args);
  args[n++] = "-o";
  args[n++] = ctx->executable.data;
  ctx->stages[3] = (mulib_capasm_tool_t){ "link", tools->ld ? tools->ld : cc, args, n };
  ctx->options.linker = &ctx->stages[3];
  return 1;
}
#endif

static void
capasm_tool_clear(capasm_tool_context_t *ctx)
{
#ifdef MULIB_CAPASM_TOOLS_POSIX
  if (ctx->executable.data) (void)unlink(ctx->executable.data);
  if (ctx->object.data) (void)unlink(ctx->object.data);
  if (ctx->have_dir) (void)rmdir(ctx->dir.data);
#endif
  for (size_t i = 0; i < MULIB_CAPASM_TOOL_ARG_MAX; ++i) {
    mulib_strbuf_free(&ctx->pass_paths[i]);
    mulib_strbuf_free(&ctx->link_paths[i]);
  }
  mulib_strbuf_free(&ctx->dir);
  mulib_strbuf_free(&ctx->object);
  mulib_strbuf_free(&ctx->executable);
}

static int
capasm_tool_emit(const capasm_tools_t *tools, const char *source, size_t length, const char *entry,
  char **artifact, size_t *artifact_length, mulib_termscript_error_t *error)
{
  if (!tools->rewrites.count && tools->output == CAPASM_TOOL_C90) {
    const mulib_capasm_options_t options = {
      .kernels = tools->kernels.values, .kernel_count = tools->kernels.count
    };
    return mulib_termscript_compile(source, length, entry, &options, artifact, artifact_length, error);
  }
  capasm_tool_context_t ctx = { 0 };
  ctx.options.kernels = tools->kernels.values;
  ctx.options.kernel_count = tools->kernels.count;
  int ok = 0;
#ifdef MULIB_CAPASM_TOOLS_POSIX
  if (capasm_tool_options(tools, &ctx))
    ok = mulib_termscript_compile(source, length, entry, &ctx.options, artifact, artifact_length, error);
  else if (error) *error = (mulib_termscript_error_t){ 0, "unable to configure CAPASM tools" };
#else
  if (error) *error = (mulib_termscript_error_t){ 0, "external compile tools require POSIX process support" };
#endif
  capasm_tool_clear(&ctx);
  return ok;
}
#endif
