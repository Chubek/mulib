/* libFuzzer: strict scoped cookie JSON, no file reads or network. */
#include "fixtures/elvi-scrape-setup.h"
#define MULIB_IMPL_ELVI_SCRIPT_TRANSPORT
#include "mulib-elvi-script-transport.h"
#include <assert.h>

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
  static const char dsl[] =
    "elvi fixture\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/?q={query}\nanchor any\n";
  static const char prefix[] = "[{\"name\":\"fixture\",\"value\":\"";
  static const char suffix[] =
    "\",\"domain\":\"example.org\",\"path\":\"/\",\"secure\":true,\"sameSite\":\"Strict\"}]";
  mulib_elvi_scrape_program_t p;
  mulib_elvi_script_cookies_t *store;
  char wrapped[sizeof prefix + 4096 + sizeof suffix];
  if (size > MULIB_ELVI_SCRIPT_MAX_COOKIE_JSON) return 0;
  assert(mulib_elvi_scrape_compile(dsl, sizeof dsl - 1, &p, NULL));
  store = mulib_elvi_script_cookies_parse(&p, (const char *)data, size);
  mulib_elvi_script_cookies_destroy(store);
  if (size <= 4096) {
    const size_t start = sizeof prefix - 1;
    memcpy(wrapped, prefix, start);
    memcpy(wrapped + start, data, size);
    memcpy(wrapped + start + size, suffix, sizeof suffix);
    store = mulib_elvi_script_cookies_parse(&p, wrapped, start + size + sizeof suffix - 1);
    mulib_elvi_script_cookies_destroy(store);
  }
  return 0;
}
