/* Read one ElviScript run on an inherited, supervisor-authorized Unix stream.
 * Build: make build/elvi-script-receiver
 * Usage: elvi-script-receiver INHERITED_FD
 */
#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_IPC_FRAMED
#define MULIB_IMPL_ELVI_SCRIPT_IPC
#include "mulib-elvi-script-ipc.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>

/* Render bytes as ASCII; untrusted UTF-8 never becomes terminal control text. */
static int field(const char *s)
{
  for (; *s; ++s) {
    unsigned char c = (unsigned char)*s;
    if (c == '\\' || c >= 0x7f) {
      if (printf("\\x%02X", (unsigned)c) < 0) return 0;
    } else if (putchar(c) == EOF) return 0;
  }
  return 1;
}

static int receive_run(int fd)
{
  mulib_elvi_script_result_t *results = calloc(MULIB_ELVI_SCRIPT_MAX_RESULTS, sizeof *results);
  mulib_elvi_ipc_t ipc;
  mulib_elvi_ipc_event_t e;
  unsigned count = 0;
  int status = 1;
  if (!results) return status;
  if (mulib_elvi_ipc_init(&ipc, fd, 5000)) goto done;
  for (;;) {
    if (mulib_elvi_ipc_receive(&ipc, &e)) {
      fputs("ElviScript stream ended without a valid completion\n", stderr);
      goto done;
    }
    if (e.kind == MULIB_ELVI_IPC_RESULT) results[count++] = e.result;
    else if (e.kind == MULIB_ELVI_IPC_FAILURE) {
      fprintf(stderr, "ElviScript failed (code %u)\n", e.value);
      goto done;
    } else if (e.kind == MULIB_ELVI_IPC_COMPLETE) break;
  }
  for (unsigned i = 0; i < count; ++i) {
    const mulib_elvi_script_result_t *r = &results[i];
    if (!field(r->name) || putchar('\t') == EOF ||
        !field(r->url) || putchar('\t') == EOF ||
        !field(r->price) || putchar('\t') == EOF ||
        !field(r->location) || putchar('\n') == EOF) goto done;
  }
  if (fflush(stdout) == 0) status = 0;
done:
  free(results);
  return status;
}

int main(int argc, char **argv)
{
  unsigned fd = 0;
  int status;
  if (argc != 2 || !argv[1][0]) goto usage;
  for (const char *p = argv[1]; *p; ++p) {
    unsigned digit = (unsigned char)*p - (unsigned)'0';
    if (digit > 9 || fd > ((unsigned)INT_MAX - digit) / 10u) goto usage;
    fd = fd * 10u + digit;
  }
  if (fd < 3) goto usage;
  status = receive_run((int)fd);
  close((int)fd);
  return status;
usage:
  fputs("usage: elvi-script-receiver INHERITED_UNIX_FD (>= 3)\n", stderr);
  return 2;
}
#else
int main(void)
{
  fputs("ElviScript receiver needs Unix streams\n", stderr);
  return 2;
}
#endif
