#define MULIB_IMPL_DAEMON
#define MULIB_IMPL_PROCESS
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-daemon.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main (int argc, char **argv) {
  const char source[] =
    "daemon_name('example')\n"
    "daemon_description('Example worker')\n"
    "daemon_command('/usr/bin/true')\n"
    "daemon_environment('LANG', 'C')\n";
  mulib_daemon_error_t error;
  mulib_daemon_t *daemon = mulib_daemon_load (source, sizeof (source)-1, &error);
  mulib_strbuf_t output;
  mulib_strbuf_init (&output);
  assert (daemon);
  if (argc == 2) {
    /* ./daemon library/ldaemon/systemd.lua > example.service */
    int rc = mulib_daemon_compile_file (daemon, argv[1], &output, &error);
    if (rc) { fprintf (stderr, "%s\n", error.message); mulib_daemon_destroy (daemon); return 1; }
  } else {
    const char profile[] = "return daemon.name .. ': ' .. daemon.argv[1] .. '\\n'";
    assert (!mulib_daemon_compile (daemon, profile, sizeof (profile)-1, &output, &error));
  }
  fputs (output.data, stdout);
  mulib_strbuf_free (&output); mulib_daemon_destroy (daemon);
  return 0;
}
