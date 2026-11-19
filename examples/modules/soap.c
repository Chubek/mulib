#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_TSL
#define MULIB_IMPL_HTTPS
#define MULIB_IMPL_SOCKET
#define MULIB_IMPL_MUW3
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_CDD_SOAP
#include "detail/mulib-tls-implementation.h"
#include "mulib-soap.h"
#include <stdio.h>

int main(void)
{
  static const char input[] = "5 < 7 & 9 > 2";
  char payload[256], *envelope;
  char *escaped = mulib_cdd_soap_escape_text(input, sizeof(input) - 1, 100);
  int is_fault, rc;
  if (!escaped) return 1;
  rc = snprintf(payload, sizeof(payload), "<m:Check xmlns:m=\"urn:demo\">%s</m:Check>", escaped);
  free(escaped);
  if (rc < 0 || (size_t)rc >= sizeof(payload)) return 1;
  envelope = mulib_cdd_soap_envelope(1, NULL, payload, 1024);
  if (!envelope) return 1;
  rc = mulib_cdd_soap_inspect(1, envelope, strlen(envelope), 1024, &is_fault);
  if (rc == 1 && !is_fault) puts(envelope);
  free(envelope);
  return rc == 1 && !is_fault ? 0 : 1;
}
