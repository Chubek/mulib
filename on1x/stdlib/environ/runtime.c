#define _POSIX_C_SOURCE 200809L
#include <on1x_runtime.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef ON1X_ENVIRON_MALLOC
#define ON1X_ENVIRON_MALLOC malloc
#endif

enum { ENVIRON_MAX_ENTRIES = 4096 };
extern char **environ;
static int start_argc;
static char **start_argv;

/* Called once, before the On1x entry point (and before the CSP scheduler). */
void on1x_environ_init(int argc, char **argv) {
  start_argc = argc;
  start_argv = argv;
}

int64_t on1x_environ_argc(void) {
  return start_argc;
}

const char *on1x_environ_arg(int64_t index) {
  if (start_argc > ENVIRON_MAX_ENTRIES) on1x_tool_fail("argv snapshot entry limit exceeded");
  if (index < 0 || index >= start_argc) on1x_tool_fail("argv index out of range");
  return start_argv[index];
}

int64_t on1x_environ_count(void) {
  int64_t count = 0;
  char **entries = environ;
  while (entries && entries[count]) {
    if (count == ENVIRON_MAX_ENTRIES) on1x_tool_fail("environ snapshot entry limit exceeded");
    ++count;
  }
  return count;
}

const char *on1x_environ_entry(int64_t index) {
  char **entries = environ;
  if (index >= 0 && index < ENVIRON_MAX_ENTRIES && entries) {
    for (int64_t i = 0; entries[i]; ++i) {
      if (i == index) return entries[i];
    }
  }
  on1x_tool_fail("environment index out of range");
  return "";
}

static const char *lookup(const char *name) {
  return name && *name && !strchr(name, '=') ? getenv(name) : NULL;
}

bool on1x_environ_has(const char *name) {
  return lookup(name) != NULL;
}

const char *on1x_environ_getenv(const char *name) {
  const char *value = lookup(name);
  return value ? on1x_text_slice(value, 0, on1x_text_len(value)) : "";
}

/* The first successful query is cached so all five scalar reads describe
   the same host snapshot. Failed queries can be retried. */
static struct utsname host;
static bool have_host;

bool on1x_environ_uname_ready(void) {
  if (!have_host) have_host = uname(&host) == 0;
  return have_host;
}

const char *on1x_environ_uname_field(int64_t index) {
  if (!have_host) on1x_tool_fail("uname fields requested without a snapshot");
  switch (index) {
    case 0: return host.sysname;
    case 1: return host.nodename;
    case 2: return host.release;
    case 3: return host.version;
    case 4: return host.machine;
    default: on1x_tool_fail("uname field out of range"); return "";
  }
}

int64_t on1x_environ_exec(const char *path, int64_t count, const char *wire) {
  if (!path || !*path || !wire || count < 0) return -EINVAL;
  if (count >= ENVIRON_MAX_ENTRIES) return -E2BIG;
  const size_t path_bytes = strlen(path) + 1;
  const size_t vector_bytes = ((size_t)count + 2) * sizeof(char *);
  size_t remaining = strlen(wire);
  if (path_bytes > SIZE_MAX - vector_bytes ||
      remaining > SIZE_MAX - vector_bytes - path_bytes) return -E2BIG;
  /* Each decimal header and its colon take more space than the NUL
     replacing them, so the encoded size bounds the decoded bytes. */
  char **argv = ON1X_ENVIRON_MALLOC(vector_bytes + path_bytes + remaining);
  if (!argv) return -ENOMEM;
  char *storage = (char *)argv + vector_bytes;
  argv[0] = storage;
  memcpy(storage, path, path_bytes);
  storage += path_bytes;
  int error = EINVAL;
  for (int64_t i = 1; i <= count; ++i) {
    size_t length = 0;
    if (!remaining || *wire < '0' || *wire > '9') goto done;
    while (remaining && *wire >= '0' && *wire <= '9') {
      size_t digit = (size_t)(*wire++ - '0');
      --remaining;
      if (length > (SIZE_MAX - digit) / 10) goto done;
      length = 10 * length + digit;
    }
    if (!remaining || *wire++ != ':') goto done;
    --remaining;
    if (length > remaining) goto done;
    argv[i] = storage;
    memcpy(storage, wire, length);
    storage[length] = '\0';
    storage += length + 1;
    wire += length;
    remaining -= length;
  }
  if (remaining) goto done;
  argv[count + 1] = NULL;
  execv(path, argv);
  error = errno;
done:
  free(argv);
  return -(int64_t)error;
}

int64_t on1x_environ_system(const char *command) {
  if (!command) return -EINVAL;
  int status = system(command);
  if (status == -1) return -(int64_t)errno;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + (int64_t)WTERMSIG(status);
  return -ECHILD;
}
