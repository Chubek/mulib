/* The optional Linux named endpoint host example; no remote network. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_IPC_FRAMED
#include "../examples/elvi-script-named-ipc.h"
#include <assert.h>
#include <stdio.h>
#if defined(__linux__)
#include <stdlib.h>

int main(void)
{
  char dir[] = "/tmp/mulib-elvi-ipc-XXXXXX";
  struct sockaddr_un address = {0};
  const char *path = address.sun_path;
  char alias[sizeof address.sun_path], traversal[sizeof address.sun_path], byte;
  mode_t previous;
  int listener, client, receiver;
  assert(mkdtemp(dir));
  address.sun_family = AF_UNIX;
  assert(snprintf(address.sun_path, sizeof address.sun_path, "%s/sock", dir) > 0);
  assert(snprintf(alias, sizeof alias, "%s/alias", dir) > 0);
  assert(snprintf(traversal, sizeof traversal, "%s/../%s/sock", dir, dir + 5) > 0);
  listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  assert(listener >= 0);
  previous = umask(0077);
  assert(bind(listener, (struct sockaddr *)&address, sizeof address) == 0);
  umask(previous);
  assert(listen(listener, 1) == 0);
  assert(elvi_script_example_connect(path, alias, 1000) == -1);
  assert(elvi_script_example_connect(traversal, traversal, 1000) == -1);
  assert(elvi_script_example_connect(path, path, 0) == -1);
  assert(symlink(path, alias) == 0);
  assert(elvi_script_example_connect(alias, alias, 1000) == -1);
  assert(chmod(path, 0777) == 0);
  assert(elvi_script_example_connect(path, path, 1000) == -1);
  assert(chmod(path, 0600) == 0);
  assert(chmod(dir, 0755) == 0);
  assert(elvi_script_example_connect(path, path, 1000) == -1);
  assert(chmod(dir, 0700) == 0);
  client = elvi_script_example_connect(path, path, 1000);
  assert(client >= 0);
  assert(fcntl(client, F_GETFD) & FD_CLOEXEC);
  assert(fcntl(client, F_GETFL) & O_NONBLOCK);
  receiver = accept(listener, NULL, NULL);
  assert(receiver >= 0);
  assert(write(client, "X", 1) == 1);
  assert(read(receiver, &byte, 1) == 1 && byte == 'X');
  close(receiver);
  close(client);
  close(listener);
  assert(elvi_script_example_connect(path, path, 1000) == -1);
  assert(unlink(alias) == 0);
  assert(unlink(path) == 0);
  assert(rmdir(dir) == 0);
  puts("ElviScript named IPC: OK");
  return 0;
}
#else
int main(void)
{
  puts("ElviScript named IPC: Linux only");
  return 0;
}
#endif
