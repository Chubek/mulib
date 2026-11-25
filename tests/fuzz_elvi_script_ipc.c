/* libFuzzer: feed one complete frame to the ElviScript IPC reader. No
 * named sockets, network services, or production data are used. */
#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_IPC_FRAMED
#define MULIB_IMPL_ELVI_SCRIPT_IPC
#include "mulib-elvi-script-ipc.h"
#include <assert.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
  unsigned char frame[MULIB_ELVI_IPC_MAX_FRAME];
  mulib_elvi_ipc_event_t event, before;
  mulib_elvi_ipc_t reader;
  mulib_ipc_framed_t writer;
  int fd[2], rc;
  if (size > sizeof frame) return 0;
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
  assert(mulib_ipc_framed_init(&writer, fd[0], sizeof frame) == 0);
  assert(mulib_elvi_ipc_init(&reader, fd[1], 3000) == 0);
  memcpy(frame, data, size);
  if (size && (frame[0] & 1)) frame[0] = MULIB_ELVI_IPC_VERSION;
  assert(mulib_ipc_send(&writer, frame, size, reader.deadline) == 0);
  memset(&event, 0xa5, sizeof event);
  memcpy(&before, &event, sizeof before);
  rc = mulib_elvi_ipc_receive(&reader, &event);
  if (rc) {
    assert(rc == MULIB_SB_PROTOCOL);
    assert(!memcmp(&event, &before, sizeof event));
    assert(mulib_elvi_ipc_receive(&reader, &event) == MULIB_SB_STATE);
  } else {
    switch (event.kind) {
      case MULIB_ELVI_IPC_RESULT:
        assert(reader.results == 1 && !reader.terminal && !reader.diagnostic);
        assert(event.result.name[0] && event.result.url[0]);
        assert(memchr(event.result.name, 0, sizeof event.result.name));
        assert(memchr(event.result.url, 0, sizeof event.result.url));
        break;
      case MULIB_ELVI_IPC_DIAGNOSTIC:
        assert(reader.diagnostic == event.value && !reader.results && !reader.terminal);
        break;
      case MULIB_ELVI_IPC_COMPLETE:
        assert(reader.terminal && event.value == 0);
        break;
      default:
        assert(0); /* FAILURE without its preceding diagnostic is invalid. */
    }
  }
  close(fd[0]);
  close(fd[1]);
  return 0;
}
