/* Link with the legacy test-elvi-script implementation TU with its main
 * renamed. This verifies the optional adapter's real offline execution. */
#include "runtime.h"
#include "script.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const char source[] =
    "elvi fixture\nhost example.org\nhome https://example.org/\n"
    "mode scrape\nxpath provide{mulib-xpath}\nartifacts discard\n"
    "logic lua{$HOME/fixture.lua}\nforward ipc{/approved/fixture}\n";
static const char script[] =
    "function on_response(doc, url)\n"
    "  local links = xpath(doc, '//a')\n"
    "  emit(text(links[1]), url)\n"
    "end\n";

static int fetch(void *context, const mulib_elvi_script_request_t *request,
                 mulib_elvi_script_response_t *response)
{
  static const char body[] = "<html><body><a>Fixture</a></body></html>";
  unsigned *count = context;
  char wire[256];
  size_t consumed = 0;
  int n = snprintf(wire, sizeof wire, "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\nContent-Length: %zu\r\n\r\n%s",
      sizeof body - 1, body);
  assert(n > 0 && (size_t)n < sizeof wire);
  assert(!strcmp(request->url, "https://example.org/") && *count == 0);
  ++*count;
  assert(mulib_http_parse(wire, (size_t)n, 1, NULL, &response->message,
                          &consumed) == MULIB_HTTP_OK);
  assert(consumed == (size_t)n);
  memcpy(response->effective_url, request->url, strlen(request->url) + 1);
  return 1;
}

int main(void)
{
  mulib_elvi_scrape_program_t program;
  mulib_elvi_ipc_t receiver;
  mulib_elvi_ipc_event_t event;
  unsigned requests = 0;
  int stream[2];
  assert(on1x_elvi_compile(source, sizeof source - 1, &program, NULL));
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, stream) == 0);
  mulib_elvi_script_run_config_t config = {
    .approved_home = "/approved",
    .approved_logic_path = "/approved/fixture.lua",
    .approved_forward_path = "/approved/fixture",
    .logic_source = script,
    .logic_length = sizeof script - 1,
    .forward_fd = stream[0],
    .ipc_timeout_ms = 1000,
    .fetch = fetch,
    .fetch_user = &requests
  };
  assert(on1x_elvi_run(&program, "different.example.org", &config) == MULIB_SB_DENIED);
  assert(requests == 0);
  assert(mulib_elvi_ipc_init(&receiver, stream[1], 1000) == 0);
  assert(on1x_elvi_run(&program, "example.org", &config) == 0);
  assert(requests == 1);
  assert(mulib_elvi_ipc_receive(&receiver, &event) == 0);
  assert(event.kind == MULIB_ELVI_IPC_RESULT && !strcmp(event.result.name, "Fixture"));
  assert(!strcmp(event.result.url, "https://example.org/"));
  assert(mulib_elvi_ipc_receive(&receiver, &event) == 0);
  assert(event.kind == MULIB_ELVI_IPC_COMPLETE && event.value == 1);
  assert(close(stream[0]) == 0 && close(stream[1]) == 0);
  return 0;
}
