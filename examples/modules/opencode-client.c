/* Offline by default: inspect the pinned catalog without opening a socket. */
#include "../../tests/fixtures/opencode-client-setup.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
  const mulib_cdd_opencode_operation_t *info =
    mulib_cdd_opencode_operation("server.info");
  if (!info) return 1;
  if (argc == 1) {
    printf("OpenCode V2: %zu catalog operations; discovery is %s %s\n",
           mulib_cdd_opencode_operation_count(), info->method, info->path);
    return 0;
  }
  /* The default client rejects cleartext. This sample does not read secrets. */
  if (argc != 2 || strncmp(argv[1], "https://", 8)) {
    fprintf(stderr, "usage: %s [https://server[:port]]\n", argv[0]);
    return 2;
  }
  mulib_cdd_opencode_client_t client = mulib_cdd_opencode_client(argv[1]);
  mulib_cdd_opencode_request_t request = {0};
  mulib_cdd_opencode_response_t response = {0};
  request.operation_id = info->id;
  mulib_cdd_opencode_status_t rc = mulib_cdd_opencode_request(&client, &request, &response);
  if (rc == MULIB_CDD_OPENCODE_OK) {
    if (response.http.message.body_length &&
        fwrite(response.http.message.body, 1, response.http.message.body_length, stdout) !=
          response.http.message.body_length) rc = MULIB_CDD_OPENCODE_TRANSPORT;
    if (rc == MULIB_CDD_OPENCODE_OK) putchar('\n');
  }
  if (rc != MULIB_CDD_OPENCODE_OK)
    fprintf(stderr, "OpenCode result %d (HTTP %u)\n", (int)rc, response.http.message.status);
  mulib_cdd_opencode_response_free(&response);
  return rc == MULIB_CDD_OPENCODE_OK ? 0 : 1;
}
