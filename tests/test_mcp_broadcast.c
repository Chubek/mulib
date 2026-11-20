#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_SSE
#define MULIB_IMPL_CDD_MCP_CLIENT
#define MULIB_IMPL_CDD_MCP_SERVER
#define MULIB_IMPL_CDD_MCP_HTTP
#define MULIB_IMPL_MCP_BROADCAST
#include "mulib-mcp-broadcast.h"

/* Optional fixture: use function sections and linker GC to link only the
 * pure JSON-RPC builder, not mulib-jsonrpc's REST/network client functions. */
#ifdef MULIB_MCP_BROADCAST_TEST_JSONRPC
#define MULIB_IMPL_JSONRPC
#include "mulib-jsonrpc.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOST "tools.example:8443"
#define PATH "/connectors/mcp"
#define RESOURCE "https://" HOST PATH
#define METADATA "/.well-known/oauth-protected-resource" PATH
#define CHALLENGE "Bearer resource_metadata=\"https://" HOST METADATA "\""
#define ISSUER "https://accounts.example"
#define ISSUER_TENANT "https://tenant.example/issuer"
#define TOKEN "fixture-ALLOWED~+/=="
#define TOKEN_LIMITED "fixture-limited"
#define AUTH "Authorization: Bearer " TOKEN "\r\n"

struct fixture {
    unsigned origins, verifies, authorizations, validates, invokes;
    int full, limited;
};

static mulib_json_t *parse(const char *s)
{
    mulib_json_t *v = mulib_json_parse(s, strlen(s), NULL);
    assert(v);
    return v;
}

static const void *verify(void *user, const char *resource, const char *token, size_t n)
{
    struct fixture *f = user;
    assert(n == strlen(token));
    assert(!strcmp(resource, RESOURCE) || !strcmp(resource, "https://" HOST "/"));
    ++f->verifies;
    if (n == sizeof TOKEN - 1 && !memcmp(token, TOKEN, n))
        return &f->full;
    if (n == sizeof TOKEN_LIMITED - 1 && !memcmp(token, TOKEN_LIMITED, n))
        return &f->limited;
    return NULL;
}

static int origin(void *user, const char *value)
{
    struct fixture *f = user;
    ++f->origins;
    return !strcmp(value, "https://ui.example");
}

static int authorize(void *user, const mulib_cdd_mcp_request_view_t *request)
{
    struct fixture *f = user;
    ++f->authorizations;
    assert(request->principal == &f->full || request->principal == &f->limited);
    assert(request->modern);
    if (!strcmp(request->method, "server/discover") || !strcmp(request->method, "tools/list"))
        return 1;
    return request->principal == &f->full && !strcmp(request->method, "tools/call")
        && request->target && !strcmp(request->target, "echo");
}

static int validate(void *user, const mulib_json_t *schema, const mulib_json_t *arguments)
{
    struct fixture *f = user;
    (void)schema;
    ++f->validates;
    return mulib_json_text(mulib_json_get(arguments, "label"), NULL) != NULL;
}

static mulib_cdd_mcp_handler_status_t invoke(
    void *user, const mulib_cdd_mcp_request_view_t *request, mulib_json_t **out)
{
    struct fixture *f = user;
    assert(request->principal == &f->full && !strcmp(request->target, "echo"));
    assert(!strcmp(mulib_json_text(mulib_json_get(request->arguments, "label"), NULL), "fixture-admin"));
    ++f->invokes;
    *out = parse("{\"content\":[{\"type\":\"text\",\"text\":\"echo fixture\"}]}");
    return MULIB_CDD_MCP_HANDLER_OK;
}

static size_t make_http(char *out, size_t cap, const char *method, const char *target,
                        const char *host, const char *headers, const char *body)
{
    int n = snprintf(out, cap, "%s %s HTTP/1.1\r\nHost: %s\r\n%sContent-Length: %zu\r\n\r\n%s",
                     method, target, host, headers, strlen(body), body);
    assert(n > 0 && (size_t)n < cap);
    return (size_t)n;
}

static mulib_http_message_t exchange(const mulib_cdd_mcp_catalog_t *catalog,
    const mulib_mcp_broadcast_config_t *config, const char *route, const void *wire,
    size_t n, unsigned expected_status)
{
    unsigned char *reply = NULL;
    size_t length = 0, used;
    mulib_http_message_t response;
    assert(mulib_mcp_broadcast_http(catalog, config, HOST, route, wire, n, &reply, &length)
           == MULIB_CDD_MCP_OK);
    assert(mulib_http_parse(reply, length, 1, NULL, &response, &used) == MULIB_HTTP_OK);
    assert(length == used && response.status == expected_status);
    assert(!strcmp(mulib_http_header(&response, "Cache-Control"), "no-store"));
    /* Credentials are never reflected, including in error and metadata responses. */
    assert(!strstr((const char *)reply, TOKEN) && !strstr((const char *)reply, TOKEN_LIMITED));
    free(reply);
    return response;
}

static void expect_http(const mulib_cdd_mcp_catalog_t *catalog,
    const mulib_mcp_broadcast_config_t *config, const char *route, const void *wire,
    size_t n, unsigned code)
{
    mulib_http_message_t response = exchange(catalog, config, route, wire, n, code);
    if (code == 401)
        assert(!strcmp(mulib_http_header(&response, "WWW-Authenticate"),
                       config->authorization_server_count ? CHALLENGE : "Bearer"));
    mulib_http_message_free(&response);
}

static void expect_error(const mulib_cdd_mcp_catalog_t *catalog,
    const mulib_mcp_broadcast_config_t *config, const char *authority, const char *target,
    const void *wire, size_t n, mulib_cdd_mcp_status_t expected)
{
    unsigned char sentinel, *out = &sentinel;
    size_t length = 1234;
    assert(mulib_mcp_broadcast_http(catalog, config, authority, target, wire, n, &out, &length)
           == expected);
    assert(out == &sentinel && length == 1234);
}

#define CALL_HEADERS "Content-Type: application/json\r\n" \
    "Accept: application/json, text/event-stream\r\n" \
    "MCP-Protocol-Version: " MULIB_CDD_MCP_VERSION "\r\n" \
    "Mcp-Method: tools/call\r\nMcp-Name: echo\r\n" \
    "Mcp-Param-Label: fixture-admin\r\n"

#define META_JSON "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"" \
    MULIB_CDD_MCP_VERSION "\",\"io.modelcontextprotocol/clientCapabilities\":{}," \
    "\"io.modelcontextprotocol/clientInfo\":{\"name\":\"fixture-admin\",\"version\":\"1\"}}"
#define CALL_PARAMS "{" META_JSON ",\"name\":\"echo\",\"arguments\":{\"label\":\"fixture-admin\"}}"
#define CALL_BODY "{\"jsonrpc\":\"2.0\",\"id\":17,\"method\":\"tools/call\",\"params\":" CALL_PARAMS "}"

static void test_dispatch(const mulib_cdd_mcp_catalog_t *catalog,
    mulib_mcp_broadcast_config_t *config, const mulib_json_t *schema)
{
    struct fixture *f = config->user;
    mulib_cdd_mcp_client_t client;
    mulib_json_t *result = NULL, *arguments = parse("{\"label\":\"fixture-admin\"}");
    const mulib_cdd_mcp_operation_t methods[] = {MULIB_CDD_MCP_DISCOVER, MULIB_CDD_MCP_TOOLS_LIST,
                                                 MULIB_CDD_MCP_TOOLS_CALL, MULIB_CDD_MCP_TOOLS_CALL};
    assert(mulib_cdd_mcp_client_init(&client, "regular client", "1") == MULIB_CDD_MCP_OK);
    for (size_t i = 0; i < sizeof methods / sizeof *methods; ++i) {
        int is_call = methods[i] == MULIB_CDD_MCP_TOOLS_CALL;
        unsigned char *wire = NULL, *reply = NULL;
        char *frame = NULL;
        size_t n, length;
        config->sse = i == 3;
        assert(mulib_cdd_mcp_client_request(&client, methods[i], is_call ? "echo" : NULL,
                                          is_call ? arguments : NULL, &frame) == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_http_request(HOST, PATH, "Bearer " TOKEN, frame, strlen(frame),
                                               is_call ? schema : NULL, &wire, &n) == MULIB_CDD_MCP_OK);
        assert(mulib_mcp_broadcast_http(catalog, config, "TOOLS.example:8443", PATH, wire,
                                       n, &reply, &length) == MULIB_CDD_MCP_OK);
        assert(strstr((const char *)reply,
                      config->sse ? "Content-Type: text/event-stream" : "Content-Type: application/json"));
        assert(mulib_cdd_mcp_client_http_receive(&client, reply, length, &result) == MULIB_CDD_MCP_OK);
        if (i == 0)
            assert(mulib_json_size(mulib_json_get(mulib_json_get(result, "result"), "supportedVersions")) == 1);
        if (i == 1)
            assert(mulib_json_size(mulib_json_get(mulib_json_get(result, "result"), "tools")) == 1);
        mulib_json_destroy(result);
        free(reply);
        free(wire);
        free(frame);
    }
    config->sse = 0;
    assert(f->verifies == 4 && f->authorizations == 4 && f->validates == 2 && f->invokes == 2);
    mulib_json_destroy(arguments);
}

static void test_gate(const mulib_cdd_mcp_catalog_t *catalog, mulib_mcp_broadcast_config_t *config)
{
    struct fixture *f = config->user;
    char wire[12000], headers[9000];
    size_t n;
    unsigned verifies = f->verifies, origins = f->origins, authz = f->authorizations;
    unsigned validates = f->validates, invokes = f->invokes;
    const char *malformed[] = { "", "Authorization:\r\n", "Authorization: Basic abc\r\n",
        "Authorization: Bearer\r\n", "Authorization: Bearer\tabc\r\n", "Authorization: Bearer ===\r\n",
        "Authorization: Bearer ab=c\r\n", "Authorization: Bearer ab:c\r\n",
        "Authorization: Bearer \"abc\"\r\n", "Authorization: Bearer abc, Bearer def\r\n",
        "Authorization: Bearer abc def\r\n", "Authorization: Bearer ab\tc\r\n",
        "Proxy-Authorization: Bearer " TOKEN "\r\n", "X-Forwarded-Authorization: Bearer " TOKEN "\r\n" };
    for (size_t i = 0; i < sizeof malformed / sizeof *malformed; ++i) {
        int z = snprintf(headers, sizeof headers, "%s%s", CALL_HEADERS, malformed[i]);
        assert(z > 0 && (size_t)z < sizeof headers);
        n = make_http(wire, sizeof wire, "POST", PATH, HOST, headers, CALL_BODY);
        expect_http(catalog, config, PATH, wire, n, 401);
    }
    memcpy(headers, CALL_HEADERS "Authorization: Bearer ", sizeof CALL_HEADERS "Authorization: Bearer " - 1);
    n = sizeof CALL_HEADERS "Authorization: Bearer " - 1;
    memset(headers + n, 'a', MULIB_MCP_BROADCAST_MAX_TOKEN + 1);
    strcpy(headers + n + MULIB_MCP_BROADCAST_MAX_TOKEN + 1, "\r\n");
    n = make_http(wire, sizeof wire, "POST", PATH, HOST, headers, CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 401);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  CALL_HEADERS AUTH "aUtHoRiZaTiOn: Bearer " TOKEN "\r\n", CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 400);
    n = make_http(wire, sizeof wire, "POST", PATH, "wrong.example:8443",
                  CALL_HEADERS AUTH "X-Forwarded-Host: " HOST "\r\n", CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 400);
    n = make_http(wire, sizeof wire, "POST", PATH, "tools.example", CALL_HEADERS AUTH, CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 400);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  CALL_HEADERS AUTH "Host: " HOST "\r\n", CALL_BODY);
    expect_error(catalog, config, HOST, PATH, wire, n, MULIB_CDD_MCP_INVALID);
    n = make_http(wire, sizeof wire, "POST", PATH "?access_token=" TOKEN, HOST,
                  CALL_HEADERS AUTH, CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 404);
    n = make_http(wire, sizeof wire, "POST", RESOURCE, HOST, CALL_HEADERS AUTH, CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 404);
    n = make_http(wire, sizeof wire, "POST", PATH "/", HOST, CALL_HEADERS AUTH, CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 404);
    expect_error(catalog, config, "wrong.example:8443", PATH, wire, n, MULIB_CDD_MCP_INVALID);
    expect_error(catalog, config, HOST, "/wrong", wire, n, MULIB_CDD_MCP_INVALID);
    n = make_http(wire, sizeof wire, "GET", PATH, HOST, CALL_HEADERS AUTH, "");
    expect_http(catalog, config, PATH, wire, n, 405);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  CALL_HEADERS AUTH "Origin: https://ui.example\r\n", CALL_BODY);
    expect_error(catalog, config, HOST, PATH, wire, n - 1, MULIB_CDD_MCP_INVALID);
    expect_error(catalog, config, HOST, PATH, wire, n + 1, MULIB_CDD_MCP_INVALID);
    expect_http(catalog, config, PATH, wire, n, 403); /* No browser allowlist configured. */
    config->origin = origin;
    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  CALL_HEADERS AUTH "Origin: https://ui.example\r\nOrigin: https://ui.example\r\n", CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 403);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  CALL_HEADERS AUTH "Origin: https://ui.example https://wrong.example\r\n", CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 403);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  CALL_HEADERS AUTH "Origin: https://wrong.example\r\n", CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 403);
    assert(f->origins == origins + 1 && f->verifies == verifies);
    assert(f->authorizations == authz && f->validates == validates && f->invokes == invokes);

    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  CALL_HEADERS "Authorization: Bearer fixture-invalid\r\n", CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 401);
    assert(f->verifies == ++verifies && f->authorizations == authz);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  CALL_HEADERS "Authorization: Bearer " TOKEN_LIMITED "\r\n", CALL_BODY);
    expect_http(catalog, config, PATH, wire, n, 403);
    assert(f->verifies == ++verifies && f->authorizations == ++authz);
    assert(f->validates == validates && f->invokes == invokes);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST, CALL_HEADERS AUTH,
                  "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":" CALL_PARAMS "}");
    expect_http(catalog, config, PATH, wire, n, 400); /* No invocation for notifications. */
    assert(f->verifies == ++verifies && f->authorizations == authz);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST, CALL_HEADERS AUTH, "[" CALL_BODY "]");
    expect_http(catalog, config, PATH, wire, n, 400);
    assert(f->verifies == ++verifies && f->authorizations == authz);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST, CALL_HEADERS AUTH,
                  "{\"jsonrpc\":\"2.0\",\"id\":17,\"id\":18,\"method\":\"tools/call\",\"params\":" CALL_PARAMS "}");
    expect_http(catalog, config, PATH, wire, n, 400);
    assert(f->verifies == ++verifies && f->authorizations == authz);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST, CALL_HEADERS AUTH,
                  "{\"jsonrpc\":\"2.0\",\"id\":17,\"method\":\"tools/call\",\"params\":{"
                  META_JSON ",\"name\":\"echo\",\"arguments\":{\"label\":\"different\"}}}");
    expect_http(catalog, config, PATH, wire, n, 400); /* Mirror mismatch. */
    assert(f->verifies == ++verifies && f->authorizations == authz);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                  "Content-Type: application/json\r\nAccept: application/json, text/event-stream\r\n"
                  "MCP-Protocol-Version: " MULIB_CDD_MCP_LEGACY_VERSION "\r\nMcp-Method: initialize\r\n" AUTH,
                  "{\"jsonrpc\":\"2.0\",\"id\":17,\"method\":\"initialize\",\"params\":{\"_meta\":{"
                  "\"io.modelcontextprotocol/protocolVersion\":\"" MULIB_CDD_MCP_LEGACY_VERSION "\"}}}");
    expect_http(catalog, config, PATH, wire, n, 400);
    assert(f->verifies == ++verifies && f->authorizations == authz);
    assert(f->validates == validates && f->invokes == invokes);

    const char *frame = CALL_BODY;
#ifdef MULIB_MCP_BROADCAST_TEST_JSONRPC
    mulib_json_t *params = parse(CALL_PARAMS), *id = mulib_json_number(17);
    mulib_json_t *rpc = mulib_jsonrpc_make("tools/call", params, id);
    assert(rpc);
    frame = mulib_json_stringify(rpc, 0);
    assert(frame);
    mulib_json_destroy(rpc);
    mulib_json_destroy(id);
    mulib_json_destroy(params);
#endif
    n = make_http(wire, sizeof wire, "POST", PATH, "TOOLS.EXAMPLE:8443",
                  CALL_HEADERS "authorization: bEaReR   " TOKEN "\r\nOrigin: https://ui.example\r\n", frame);
    expect_http(catalog, config, PATH, wire, n, 200);
#ifdef MULIB_MCP_BROADCAST_TEST_JSONRPC
    free((char *)frame);
#endif
    assert(f->origins == origins + 2 && f->verifies == verifies + 1);
    assert(f->authorizations == authz + 1 && f->validates == validates + 1 && f->invokes == invokes + 1);
}

static void test_metadata(const mulib_cdd_mcp_catalog_t *catalog, const mulib_mcp_broadcast_config_t *config)
{
    struct fixture *f = config->user;
    mulib_mcp_broadcast_config_t bare = *config, root = *config;
    unsigned verifies = f->verifies, authorizations = f->authorizations, invokes = f->invokes;
    mulib_http_message_t response;
    mulib_json_t *doc;
    char wire[1024];
    size_t n;
    n = make_http(wire, sizeof wire, "GET", METADATA, HOST, "", "");
    response = exchange(catalog, config, METADATA, wire, n, 200);
    assert(!strcmp(mulib_http_header(&response, "Content-Type"), "application/json"));
    doc = mulib_json_parse((const char *)response.body, response.body_length, NULL);
    assert(doc && !strcmp(mulib_json_text(mulib_json_get(doc, "resource"), NULL), RESOURCE));
    const mulib_json_t *issuers = mulib_json_get(doc, "authorization_servers");
    const mulib_json_t *methods = mulib_json_get(doc, "bearer_methods_supported");
    assert(mulib_json_size(issuers) == 2 && mulib_json_size(methods) == 1);
    assert(!strcmp(mulib_json_text(mulib_json_at(issuers, 0), NULL), ISSUER));
    assert(!strcmp(mulib_json_text(mulib_json_at(issuers, 1), NULL), ISSUER_TENANT));
    assert(!strcmp(mulib_json_text(mulib_json_at(methods, 0), NULL), "header"));
    mulib_json_destroy(doc);
    mulib_http_message_free(&response);
    expect_error(catalog, config, HOST, METADATA, wire, n + 1, MULIB_CDD_MCP_INVALID);
    expect_http(catalog, config, PATH, wire, n, 404); /* Cannot enter metadata via MCP route. */
    bare.authorization_server_count = 0;
    expect_error(catalog, &bare, HOST, METADATA, wire, n, MULIB_CDD_MCP_INVALID);
    static const char no_host[] = "GET " METADATA " HTTP/1.1\r\n\r\n";
    expect_error(catalog, config, HOST, METADATA, no_host, sizeof no_host - 1, MULIB_CDD_MCP_INVALID);
    n = make_http(wire, sizeof wire, "GET", METADATA, HOST,
                  "hOsT: " HOST "\r\nOrigin: https://ui.example\r\n", "");
    expect_error(catalog, config, HOST, METADATA, wire, n, MULIB_CDD_MCP_INVALID);
    n = make_http(wire, sizeof wire, "POST", PATH, HOST, CALL_HEADERS AUTH, CALL_BODY);
    expect_http(catalog, config, METADATA, wire, n, 404); /* No MCP dispatch from the metadata route. */
    n = make_http(wire, sizeof wire, "POST", PATH, HOST, "", "");
    expect_http(catalog, &bare, PATH, wire, n, 401);
    n = make_http(wire, sizeof wire, "POST", METADATA, HOST, "", "");
    response = exchange(catalog, config, METADATA, wire, n, 405);
    assert(!strcmp(mulib_http_header(&response, "Allow"), "GET"));
    mulib_http_message_free(&response);
    n = make_http(wire, sizeof wire, "GET", METADATA, HOST, "", "not empty");
    expect_http(catalog, config, METADATA, wire, n, 400);
    n = make_http(wire, sizeof wire, "GET", METADATA "?extra", HOST, "", "");
    expect_http(catalog, config, METADATA, wire, n, 404);
    n = make_http(wire, sizeof wire, "GET", METADATA, "bad.example", "", "");
    expect_http(catalog, config, METADATA, wire, n, 400);
    n = make_http(wire, sizeof wire, "GET", METADATA, HOST, "Origin: https://bad.example\r\n", "");
    expect_http(catalog, config, METADATA, wire, n, 403);
    n = make_http(wire, sizeof wire, "GET", METADATA, HOST, AUTH "Origin: https://ui.example\r\n", "");
    expect_http(catalog, config, METADATA, wire, n, 200);
    root.resource_url = "https://" HOST "/";
    n = make_http(wire, sizeof wire, "POST", "/", HOST, "", "");
    response = exchange(catalog, &root, "/", wire, n, 401);
    assert(!strcmp(mulib_http_header(&response, "WWW-Authenticate"),
                   "Bearer resource_metadata=\"https://" HOST "/.well-known/oauth-protected-resource\""));
    mulib_http_message_free(&response);
    n = make_http(wire, sizeof wire, "GET", "/.well-known/oauth-protected-resource", HOST, "", "");
    expect_http(catalog, &root, "/.well-known/oauth-protected-resource", wire, n, 200);
    assert(f->verifies == verifies && f->authorizations == authorizations && f->invokes == invokes);
}

static void test_config(const mulib_cdd_mcp_catalog_t *catalog, const mulib_mcp_broadcast_config_t *config)
{
    const char *bad_urls[] = { NULL, "", "http://" HOST PATH, "HTTPS://" HOST PATH,
        "https://" HOST, "https://", "https:///mcp", "https://user@" HOST PATH,
        "https://" HOST PATH "#fragment", "https://" HOST PATH "?arg=1",
        "https://tools.example:0/mcp", "https://tools.example:/mcp", "https://tools.example:88888/mcp",
        "https://tools.example:0443/mcp", "https://tools%2Eexample/mcp", "https://127.1/mcp",
        "https://0x7f000001/mcp", "https://tools.example./mcp", "https://bad-.example/mcp",
        "https://[v1.ab]/mcp", "https://[::abcd/mcp", "https://" HOST "/mcp//x",
        "https://" HOST "/mcp/.", "https://" HOST "/../mcp", "https://" HOST "/%2f",
        "https://" HOST "/mcp/", "https://" HOST "/mcp\\path", "https://" HOST "/mcp\r\n",
        "https://" HOST "/.well-known/oauth-protected-resource/mcp" };
    struct fixture *f = config->user;
    unsigned verifies = f->verifies, origins = f->origins, authz = f->authorizations;
    mulib_mcp_broadcast_config_t cfg = *config;
    mulib_cdd_mcp_catalog_t cat = *catalog;
    const char *const bad_issuer[] = { "http://accounts.example" };
    const char *const dup_issuers[] = { ISSUER, ISSUER };
    char wire[512];
    size_t n = make_http(wire, sizeof wire, "POST", PATH, HOST,
                         AUTH "Origin: https://ui.example\r\n", "");
    for (size_t i = 0; i < sizeof bad_urls / sizeof *bad_urls; ++i) {
        cfg.resource_url = bad_urls[i];
        expect_error(catalog, &cfg, HOST, PATH, wire, n, MULIB_CDD_MCP_INVALID);
    }
    cfg = *config;
    cfg.authorization_servers = bad_issuer;
    cfg.authorization_server_count = 1;
    expect_error(catalog, &cfg, HOST, PATH, wire, n, MULIB_CDD_MCP_INVALID);
    cfg.authorization_servers = dup_issuers;
    cfg.authorization_server_count = 2;
    expect_error(catalog, &cfg, HOST, PATH, wire, n, MULIB_CDD_MCP_INVALID);
    cfg.authorization_server_count = MULIB_MCP_BROADCAST_MAX_ISSUERS + 1;
    expect_error(catalog, &cfg, HOST, PATH, wire, n, MULIB_CDD_MCP_INVALID);
    cfg = *config;
    cfg.authorization_servers = NULL;
    expect_error(catalog, &cfg, HOST, PATH, wire, n, MULIB_CDD_MCP_INVALID);
    cfg = *config;
    cfg.verify = NULL;
    expect_error(catalog, &cfg, HOST, PATH, wire, n, MULIB_CDD_MCP_INVALID);
    cat.features.authorize = NULL;
    expect_error(&cat, config, HOST, PATH, wire, n, MULIB_CDD_MCP_INVALID);
    assert(f->verifies == verifies && f->authorizations == authz && f->origins == origins);
    n = (size_t)snprintf(wire, sizeof wire, "POST " PATH " HTTP/1.1\r\nHost: " HOST
                         "\r\nContent-Length: %u\r\n\r\n", MULIB_CDD_MCP_MAX_FRAME + 1);
    assert(n < sizeof wire);
    expect_error(catalog, config, HOST, PATH, wire, n, MULIB_CDD_MCP_LIMIT);
}

int main(void)
{
    static const char *const issuers[] = { ISSUER, ISSUER_TENANT };
    struct fixture fixture = { 0 };
    mulib_json_t *schema = parse("{\"type\":\"object\",\"properties\":{\"label\":"
                                 "{\"type\":\"string\",\"x-mcp-header\":\"Label\"}}}");
    const mulib_cdd_mcp_structured_tool_t tool = { "echo", NULL, schema, NULL, validate, invoke, &fixture };
    const mulib_cdd_mcp_server_config_t base = { "Broadcast fixture", "1", NULL, 0, 1 };
    const mulib_cdd_mcp_catalog_config_t features = {
        .tools = &tool, .tool_count = 1, .authorize = authorize, .user = &fixture
    };
    mulib_mcp_broadcast_config_t config = {
        .resource_url = RESOURCE, .authorization_servers = issuers, .authorization_server_count = 2,
        .verify = verify, .user = &fixture
    };
    mulib_cdd_mcp_catalog_t catalog;
    assert(mulib_cdd_mcp_catalog_init(&catalog, &base, &features) == MULIB_CDD_MCP_OK);
    test_dispatch(&catalog, &config, schema);
    test_gate(&catalog, &config);
    test_metadata(&catalog, &config);
    test_config(&catalog, &config);
    mulib_json_destroy(schema);
    return 0;
}
