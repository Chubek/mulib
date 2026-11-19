#define MULIB_IMPL_XML
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_CDD_SOAP
#include "fixtures/muw3-setup.h"
#include "mulib-soap.h"
#include <assert.h>
#include <stdio.h>

#define URL "http://fixture.test/soap"
#define NS11 "http://schemas.xmlsoap.org/soap/envelope/"
#define NS12 "http://www.w3.org/2003/05/soap-envelope"
#define E11(x) "<s:Envelope xmlns:s=\"" NS11 "\">" x "</s:Envelope>"
#define E12(x) "<s:Envelope xmlns:s=\"" NS12 "\">" x "</s:Envelope>"
#define F11(x) E11("<s:Body><s:Fault>" x "</s:Fault></s:Body>")
#define F12(x) E12("<s:Body><s:Fault>" x "</s:Fault></s:Body>")
#define CODE11 "<faultcode>s:Server</faultcode>"
#define CODE12 "<s:Code><s:Value>s:Sender</s:Value></s:Code>"
#define REASON12 "<s:Reason><s:Text xml:lang=\"en-US\">Denied</s:Text></s:Reason>"

static void expect_xml(int soap12, const char *s, int expected)
{
  int fault = -10;
  int rc = mulib_cdd_soap_inspect(soap12, s, strlen(s), 0, &fault);
  assert(rc == (expected < 0 ? expected : 1));
  assert(fault == (expected < 0 ? -10 : expected));
}

static void xml_tests(void)
{
  size_t len;
  char *s = mulib_cdd_soap_escape_text("a<&>'\"\r\xc3\xa9", 9, 0);
  assert(s && !strcmp(s, "a&lt;&amp;&gt;&apos;&quot;&#13;\xc3\xa9"));
  free(s);
  assert(!mulib_cdd_soap_escape_text("&", 1, 4));
  s = mulib_cdd_soap_escape_text("&", 1, 5);
  assert(s && !strcmp(s, "&amp;"));
  free(s);
  s = mulib_cdd_soap_escape_text(NULL, 0, SIZE_MAX);
  assert(s && !*s);
  free(s);
  assert(!mulib_cdd_soap_escape_text(NULL, 1, 0));
  assert(!mulib_cdd_soap_escape_text("a\0b", 3, 0));
  assert(!mulib_cdd_soap_escape_text("\1", 1, 0));
  assert(!mulib_cdd_soap_escape_text("\xc0\xaf", 2, 0));
  assert(!mulib_cdd_soap_escape_text("\xed\xa0\x80", 3, 0));
  assert(!mulib_cdd_soap_escape_text("\xef\xbf\xbe", 3, 0));
  assert(!mulib_cdd_soap_escape_text("\xf0\x9f", 2, 0));
  s = mulib_cdd_soap_envelope(0, "<auth/>", "<m:read xmlns:m=\"urn:fixture\"/>", 0);
  assert(s && strstr(s, "<soap:Header><auth/></soap:Header>"));
  expect_xml(0, s, 0);
  len = strlen(s);
  free(s);
  assert(!mulib_cdd_soap_envelope(0, "<auth/>", "<m:read xmlns:m=\"urn:fixture\"/>", len - 1));
  s = mulib_cdd_soap_envelope(0, "<auth/>", "<m:read xmlns:m=\"urn:fixture\"/>", len);
  assert(s && strlen(s) == len);
  free(s);
  s = mulib_cdd_soap_envelope(1, NULL, "", 0);
  assert(s && strstr(s, NS12) && !strstr(s, "<soap:Header>"));
  free(s);
  assert(!mulib_cdd_soap_envelope(0, "<unclosed>", "<read/>", 0));
  assert(!mulib_cdd_soap_envelope(0, "<a/></soap:Header><soap:Header>", "<read/>", 0));
  assert(!mulib_cdd_soap_envelope(0, NULL, "<read/>junk", 0));
  assert(!mulib_cdd_soap_envelope(0, NULL, "<read>&xxe;</read>", 0));
  assert(!mulib_cdd_soap_envelope(0, NULL, "<read>\1</read>", 0));
  assert(!mulib_cdd_soap_envelope(0, NULL, NULL, 0));
  assert(mulib_cdd_soap_inspect(1, E12("<s:Body/>"), sizeof(E12("<s:Body/>")) - 1, 1, NULL) == -2);
  assert(mulib_cdd_soap_inspect(1, "<s:Envelope\0", 12, 0, NULL) == -1);
  expect_xml(1, "<Envelope xmlns=\"" NS12 "\"><Header/><Body><m:Fault xmlns:m=\"urn:app\"/></Body></Envelope>", 0);
  expect_xml(1, E12("<t:Body xmlns:t=\"" NS12 "\"/>"), 0);
  expect_xml(0, E12("<s:Body/>"), -1);
  expect_xml(1, E12("<s:Body xmlns:s=\"urn:wrong\"/>"), -1);
  expect_xml(1, E12("<s:Body/><s:Header/>"), -1);
  expect_xml(1, E12("<s:Body/><s:Body/>"), -1);
  expect_xml(1, E12("<s:Header/>"), -1);
  expect_xml(1, E12("<s:Body><unknown:x/></s:Body>"), -1);
  expect_xml(1, E12("<s:Body>not a payload element</s:Body>"), -1);
  expect_xml(0, "<!DOCTYPE s:Envelope [<!ENTITY xxe SYSTEM 'file:///example'>]>"
      E11("<s:Body><m xmlns=\"urn:m\">&xxe;</m></s:Body>"), -1);
  expect_xml(0, F11(CODE11 "<faultstring>Failed</faultstring><detail/>"), 1);
  expect_xml(0, F11("<faultcode>bad:Server</faultcode><faultstring>Failed</faultstring>"), -1);
  expect_xml(0, F11(CODE11), -1);
  expect_xml(0, F11(CODE11 "<faultstring><bad/></faultstring>"), -1);
  expect_xml(0, F11(CODE11 "<faultstring>Failed</faultstring><detail/><detail/>"), -1);
  expect_xml(1, F12(CODE12 REASON12), 1);
  expect_xml(1, F12("<s:Code><s:Value>s:Receiver</s:Value><s:Subcode><s:Value xmlns:m=\"urn:sub\">m:Cause</s:Value>"
      "</s:Subcode></s:Code>" REASON12 "<s:Detail><app/></s:Detail>"), 1);
  expect_xml(1, F12(REASON12 CODE12), -1);
  expect_xml(1, F12(CODE12 "<s:Reason><s:Text>No language</s:Text></s:Reason>"), -1);
  expect_xml(1, F12(CODE12 "<s:Reason><s:Text xml:lang='-'>Invalid language</s:Text></s:Reason>"), -1);
  expect_xml(1, F12("<s:Code><s:Value>s:NotStandard</s:Value></s:Code>" REASON12), -1);
  expect_xml(1, F12("<s:Code><s:Value xmlns:m=\"urn:bad\">m:Sender</s:Value></s:Code>" REASON12), -1);
  expect_xml(1, E12("<s:Body><s:Fault>" CODE12 REASON12 "</s:Fault><app/></s:Body>"), -1);
  {
    char xml[2048] = "<s:Envelope xmlns:s=\"" NS12 "\"><s:Body>";
    size_t offset = strlen(xml);
    for (size_t i = 0; i < MULIB_XML_DEPTH_LIMIT; i++) {
      memcpy(xml + offset, "<a>", 3);
      offset += 3;
    }
    for (size_t i = 0; i < MULIB_XML_DEPTH_LIMIT; i++) {
      memcpy(xml + offset, "</a>", 4);
      offset += 4;
    }
    strcpy(xml + offset, "</s:Body></s:Envelope>");
    expect_xml(1, xml, -2);
  }
  {
    char xml[2048] = "<s:Envelope xmlns:s=\"" NS12 "\"><s:Body><a";
    size_t offset = strlen(xml);
    for (unsigned i = 0; i < 129; i++) {
      int wrote = snprintf(xml + offset, sizeof(xml) - offset, " a%u='x>y'", i);
      assert(wrote > 0 && (size_t)wrote < sizeof(xml) - offset);
      offset += (size_t)wrote;
      if (i == 127) {
        strcpy(xml + offset, "/></s:Body></s:Envelope>");
        expect_xml(1, xml, 0);
      }
    }
    strcpy(xml + offset, "/></s:Body></s:Envelope>");
    expect_xml(1, xml, -2);
  }
  expect_xml(1, E12("<s:Body><a><![CDATA[<x a='z'>]]><!-- <b --></a></s:Body>"), 0);
}

typedef struct fixture {
  unsigned status, calls;
  const char *headers, *body, *request_body;
  const char *request_type, *request_action;
  int fail;
} fixture_t;

static int exchange(void *user, const mulib_uri_t *endpoint, const void *wire,
    size_t n, mulib_webclient_receive_fn receive, void *receiver)
{
  fixture_t *f = (fixture_t *)user;
  mulib_http_message_t request = {0};
  size_t used;
  char header[512];
  const char *value;
  int length;
  (void)endpoint;
  f->calls++;
  if (f->fail) return 0;
  assert(mulib_http_parse(wire, n, 0, NULL, &request, &used) == 1 && used == n);
  assert(!strcmp(request.method, "POST") && !strcmp(request.target, "/soap"));
  assert(!strcmp(mulib_http_header(&request, "Content-Type"), f->request_type));
  assert(!strcmp(mulib_http_header(&request, "Accept-Encoding"), "identity"));
  value = mulib_http_header(&request, "SOAPAction");
  assert(f->request_action ? value && !strcmp(value, f->request_action) : !value);
  assert(request.body_length == strlen(f->request_body) &&
      !memcmp(request.body, f->request_body, request.body_length));
  mulib_http_message_free(&request);
  length = snprintf(header, sizeof(header),
      "HTTP/1.1 %u Test\r\n%sContent-Length: %zu\r\n\r\n",
      f->status, f->headers, strlen(f->body));
  assert(length > 0 && (size_t)length < sizeof(header));
  return receive(receiver, header, (size_t)length) &&
      receive(receiver, f->body, strlen(f->body));
}

static void call_tests(void)
{
  const char *normal = E12("<s:Body><m:Read xmlns:m=\"urn:x\"/></s:Body>");
  static const char *const bad_headers[] = {
    "",
    "Content-Type: text/xml\r\n",
    "Content-Type: application/soap+xml ; charset=utf-16\r\n",
    "Content-Type: application/soap+xml; charset=utf-8; charset=utf-8\r\n",
    "Content-Type: application/soap+xml; charset=\"utf-8\r\n",
    "Content-Type: application/soap+xml; action=\r\n",
    "Content-Type: application/soap+xml ;\r\n",
    "Content-Type: application/soap+xml\r\ncontent-type: application/soap+xml\r\n",
    "Content-Type: application/soap+xml\r\nContent-Encoding: gzip\r\n",
    "Content-Type: application/soap+xml\r\nContent-Encoding: identity\r\ncontent-encoding: identity\r\n"
  };
  mulib_muw3_client_t c = mulib_muw3_client_default();
  fixture_t f = {200, 0, "Content-Type: Application/SOAP+XML; CHARSET=\"UTF-8\"\r\n"
    "Content-Encoding: Identity\r\n", normal, normal,
    "application/soap+xml; charset=utf-8; action=\"urn:read\"", NULL, 0};
  mulib_muw3_response_t response = {0};
  int fault = 17;
  c.http = exchange;
  c.http_user = &f;
  assert(mulib_cdd_soap_call(&c, URL, 1, "urn:read", normal, 0, &response, &fault) == 1);
  assert(response.message.status == 200 && !fault && f.calls == 1);
  mulib_muw3_response_free(&response);
  response.redirects = 19;
  fault = 17;
  assert(mulib_cdd_soap_call(&c, URL, 1, "bad\\", normal, 0, &response, &fault) == -1);
  assert(mulib_cdd_soap_call(&c, URL, 1, "bad\r\nInjected: yes", normal, 0, &response, &fault) == -1);
  assert(mulib_cdd_soap_call(&c, URL, 1, "bad\"", normal, 0, &response, &fault) == -1);
  assert(mulib_cdd_soap_call(&c, URL, 1, "", normal, 0, &response, &fault) == -1);
  assert(mulib_cdd_soap_call(&c, URL, 1, "urn:read", normal, 50, &response, &fault) == -2);
  assert(mulib_cdd_soap_call(&c, URL, 1, "urn:read", "<notSoap/>", 0, &response, &fault) == -1);
  assert(f.calls == 1 && response.redirects == 19 && fault == 17);
  for (size_t i = 0; i < sizeof(bad_headers) / sizeof(*bad_headers); i++) {
    f.headers = bad_headers[i];
    assert(mulib_cdd_soap_call(&c, URL, 1, "urn:read", normal, 0, &response, &fault) == -1);
    assert(response.redirects == 19 && !response.url && fault == 17);
  }
  f.headers = "Content-Type: application/soap+xml\r\n";
  f.status = 500;
  assert(mulib_cdd_soap_call(&c, URL, 1, "urn:read", normal, 0, &response, &fault) == -1);
  f.body = F12(CODE12 REASON12);
  assert(mulib_cdd_soap_call(&c, URL, 1, "urn:read", normal, strlen(normal), &response, &fault) == -2);
  f.status = 200;
  assert(mulib_cdd_soap_call(&c, URL, 1, "urn:read", normal, 0, &response, &fault) == -1);
  f.status = 400;
  assert(mulib_cdd_soap_call(&c, URL, 1, "urn:read", normal, 0, &response, &fault) == 1);
  assert(fault == 1 && response.message.status == 400);
  mulib_muw3_response_free(&response);
  f.status = 500;
  f.headers = "Content-Type: text/xml; charset=utf-8\r\n";
  f.body = F11(CODE11 "<faultstring>Failed</faultstring>");
  f.request_type = "text/xml; charset=utf-8";
  f.request_action = "\"\"";
  f.request_body = E11("<s:Body/>");
  assert(mulib_cdd_soap_call(&c, URL, 0, NULL, f.request_body, 0, &response, &fault) == 1);
  assert(fault == 1 && response.message.status == 500);
  mulib_muw3_response_free(&response);
  f.headers = "Content-Type: text/plain\r\n";
  f.body = "Error";
  f.request_action = NULL;
  assert(mulib_soap_call(&c, URL, 0, NULL, f.request_body, &response) == 1);
  assert(response.message.status == 500);
  mulib_muw3_response_free(&response);
  f.fail = 1;
  assert(mulib_cdd_soap_call(&c, URL, 0, NULL, f.request_body, 0, &response, &fault) == -4);
}

int main(void)
{
  xml_tests();
  call_tests();
  puts("SOAP: XML escaping, envelope/fault structure, HTTP media and status validation passed");
  return 0;
}
