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
#include "mulib-mcp-http.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTH "Bearer fixture"
#define HOST "localhost"
#define PATH "/mcp"

static unsigned calls;

static mulib_json_t *parse(const char *s)
{
    mulib_json_t *v = mulib_json_parse(s, strlen(s), NULL);
    assert(v);
    return v;
}

static unsigned authenticate(void *user, const mulib_http_message_t *m, const void **principal)
{
    const char *token = mulib_http_header(m, "Authorization");
    if (!token || strcmp(token, AUTH))
        return 401;
    *principal = user;
    return 200;
}

static int origin(void *user, const char *value)
{
    (void)user;
    return !strcmp(value, "https://ui.example");
}

static int authorize(void *user, const mulib_cdd_mcp_request_view_t *rq)
{
    return user == rq->principal && (!rq->target || strcmp(rq->target, "fixture:/hidden"));
}

static mulib_cdd_mcp_handler_status_t handler(
    void *user, const mulib_cdd_mcp_request_view_t *rq, mulib_json_t **result)
{
    assert(user == rq->principal);
    ++calls;
    if (!strcmp(rq->method, "tools/call")) {
        *result = parse("{\"content\":[{\"type\":\"text\",\"text\":\"ok\"}],\"structuredContent\":42}");
    } else {
        if (strcmp(rq->target, "café"))
            return MULIB_CDD_MCP_HANDLER_INVALID;
        *result = parse("{\"messages\":[{\"role\":\"user\",\"content\":{\"type\":\"text\","
                        "\"text\":\"Welcome\"}}]}");
    }
    return MULIB_CDD_MCP_HANDLER_OK;
}

static int validate(void *user, const mulib_json_t *schema, const mulib_json_t *value)
{
    (void)user;
    (void)schema;
    return mulib_json_typeof(mulib_json_get(value, "profile")) == MULIB_JSON_OBJECT;
}

static const char annotated[] =
    "{\"type\":\"object\",\"properties\":{\"profile\":{\"type\":\"object\",\"properties\":{"
    "\"label\":{\"type\":\"string\",\"x-mcp-header\":\"Label\"},"
    "\"number\":{\"type\":\"integer\",\"x-mcp-header\":\"Number\"},"
    "\"flag\":{\"type\":\"boolean\",\"x-mcp-header\":\"Flag\"}}}}}";
static const char bad_annotation[] =
    "{\"type\":\"object\",\"anyOf\":[{\"properties\":{\"name\":{\"type\":\"string\","
    "\"x-mcp-header\":\"Forbidden\"}}}]}";

static void init(mulib_cdd_mcp_catalog_t *c, mulib_cdd_mcp_structured_tool_t tools[2],
                 mulib_json_t *schema, mulib_json_t *bad, mulib_json_t *prompts)
{
    const mulib_cdd_mcp_server_config_t base = { "HTTP Fixture", "1", NULL, 0, 1 };
    const mulib_cdd_mcp_catalog_config_t f = {
        NULL, NULL, prompts, tools, 2, NULL, handler, NULL, authorize, &calls, 0, 1, 200
    };
    const mulib_cdd_mcp_structured_tool_t tool = {
        "fixture", NULL, schema, NULL, validate, handler, &calls
    };
    tools[0] = tool;
    tools[1] = tool;
    tools[1].name = "filtered";
    tools[1].input_schema = bad;
    assert(mulib_cdd_mcp_catalog_init(c, &base, &f) == MULIB_CDD_MCP_OK);
}

static void expect_http(const mulib_cdd_mcp_catalog_t *c,
    const mulib_cdd_mcp_http_config_t *cfg, const void *request, size_t n,
    unsigned code, const char *text)
{
    unsigned char *res;
    size_t z, used;
    mulib_http_message_t m = {0};
    assert(mulib_cdd_mcp_server_http(c, cfg, request, n, &res, &z) == MULIB_CDD_MCP_OK);
    assert(mulib_http_parse(res, z, 1, NULL, &m, &used) == MULIB_HTTP_OK && used == z);
    assert(m.status == code);
    assert(!text || strstr((const char *)res, text));
    mulib_http_message_free(&m);
    free(res);
}

static void independent_sse(mulib_cdd_mcp_client_t *cl)
{
    static const char head[] = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=UTF-8\r\n\r\n";
    static const char notifications[] =
        ":keepalive\r\n\r\n"
        "data: {\"jsonrpc\":\"2.0\",\"method\":\"notifications/progress\",\"params\":{}}\r\n\r\n";
    static const char final[] =
        "data: {\"jsonrpc\":\"2.0\",\"id\":\"mcp-1\",\r\n"
        "data: \"result\":{\"resultType\":\"complete\",\"messages\":[]}}\r\n\r\n";
    const mulib_cdd_mcp_request_options_t direct = { NULL, NULL, 1 };
    char wire[1024], *req;
    mulib_json_t *res = NULL;
    int n;
    assert(mulib_cdd_mcp_client_init(cl, "SSE fixture", "1") == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_request_ex(cl, MULIB_CDD_MCP_PROMPTS_GET, "foo", NULL,
                                          &direct, &req) == MULIB_CDD_MCP_OK);
    free(req);
    n = snprintf(wire, sizeof wire, "%s%s%s%s", head, notifications, final, final);
    assert(n > 0 && (size_t)n < sizeof wire);
    assert(mulib_cdd_mcp_client_http_receive(cl, wire, (size_t)n, &res) == MULIB_CDD_MCP_INVALID && !res);
    n = snprintf(wire, sizeof wire, "%s%s%s", head, notifications, final);
    assert(n > 0 && (size_t)n < sizeof wire);
    assert(mulib_cdd_mcp_client_http_receive(cl, wire, (size_t)n, &res) == MULIB_CDD_MCP_OK && res);
    mulib_json_destroy(res);
}

int main(int argc, char **argv)
{
    mulib_cdd_mcp_catalog_t c;
    mulib_cdd_mcp_client_t cl;
    mulib_cdd_mcp_http_config_t cfg = { HOST, PATH, "Bearer realm=\"fixture\"", origin,
                                        authenticate, &calls, 0 };
    mulib_cdd_mcp_structured_tool_t tools[2];
    mulib_json_t *schema = parse(annotated), *bad = parse(bad_annotation);
    mulib_json_t *prompts = parse("[{\"name\":\"café\"}]");
    mulib_json_t *dom = NULL;
    unsigned char *wire, *response;
    size_t n, z;
    char *q;
    const mulib_cdd_mcp_request_options_t direct = { NULL, NULL, 1 };
    init(&c, tools, schema, bad, prompts);
    if (argc == 2 && !strcmp(argv[1], "--respond")) {
        char bytes[98305];
        size_t len = fread(bytes, 1, sizeof bytes, stdin);
        cfg.sse = 1;
        assert(len < sizeof bytes && !ferror(stdin));
        assert(mulib_cdd_mcp_server_http(&c, &cfg, bytes, len, &response, &z) == MULIB_CDD_MCP_OK);
        assert(fwrite(response, 1, z, stdout) == z);
        free(response);
    } else if (argc == 2 && !strcmp(argv[1], "--client-request")) {
        assert(mulib_cdd_mcp_client_init(&cl, "network fixture", "1") == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_request_ex(&cl, MULIB_CDD_MCP_PROMPTS_GET, "café", NULL,
                                               &direct, &q) == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_http_request(HOST, PATH, AUTH, q, strlen(q), NULL, &wire, &n)
               == MULIB_CDD_MCP_OK && fwrite(wire, 1, n, stdout) == n);
        free(wire);
        free(q);
    } else if (argc == 2 && !strcmp(argv[1], "--client-response")) {
        char bytes[262145];
        size_t len = fread(bytes, 1, sizeof bytes, stdin);
        assert(len < sizeof bytes && !ferror(stdin));
        assert(mulib_cdd_mcp_client_init(&cl, "network fixture", "1") == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_request_ex(&cl, MULIB_CDD_MCP_PROMPTS_GET, "café", NULL,
                                               &direct, &q) == MULIB_CDD_MCP_OK);
        free(q);
        assert(mulib_cdd_mcp_client_http_receive(&cl, bytes, len, &dom) == MULIB_CDD_MCP_OK);
        assert(mulib_json_size(mulib_json_get(mulib_json_get(dom, "result"), "messages")) == 1);
        mulib_json_destroy(dom);
    } else {
        mulib_json_t *arg = parse("{\"profile\":{\"label\":\" a\\u0000b \",\"number\":-7,\"flag\":true}}");
        assert(argc == 1 && mulib_cdd_mcp_client_init(&cl, "HTTP Fixture", "1") == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_DISCOVER, NULL, NULL, &q) == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_http_request(HOST, PATH, AUTH, q, strlen(q), NULL, &wire, &n)
                == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_server_http(&c, &cfg, wire, n, &response, &z) == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_http_receive(&cl, response, z, &dom) == MULIB_CDD_MCP_OK);
        assert(mulib_json_size(mulib_json_get(mulib_json_get(dom, "result"), "supportedVersions")) == 1);
        mulib_json_destroy(dom);
        free(response); free(wire); free(q);

        assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_TOOLS_LIST, NULL, NULL, &q) == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_http_request(HOST, PATH, AUTH, q, strlen(q), NULL, &wire, &n)
                == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_server_http(&c, &cfg, wire, n, &response, &z) == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_http_receive(&cl, response, z, &dom) == MULIB_CDD_MCP_OK);
        assert(mulib_json_size(mulib_json_get(mulib_json_get(dom, "result"), "tools")) == 1);
        mulib_json_destroy(dom);
        free(response); free(wire); free(q);

        assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_TOOLS_CALL, "fixture", arg, &q) == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_http_request(HOST, PATH, AUTH, q, strlen(q), bad, &wire, &n)
                == MULIB_CDD_MCP_INVALID);
        assert(mulib_cdd_mcp_client_http_request(HOST, PATH, AUTH, q, strlen(q), NULL, &wire, &n)
                == MULIB_CDD_MCP_OK);
        expect_http(&c, &cfg, wire, n, 400, "-32020");
        assert(calls == 0);
        free(wire);
        assert(mulib_cdd_mcp_client_http_request(HOST, PATH, AUTH, q, strlen(q), schema, &wire, &n)
                == MULIB_CDD_MCP_OK);
        assert(strstr((const char *)wire, "Mcp-Param-Label: =?base64?IGEAYiA=?="));
        assert(strstr((const char *)wire, "Mcp-Param-Number: -7"));
        assert(strstr((const char *)wire, "Mcp-Param-Flag: true"));
        cfg.sse = 1;
        assert(mulib_cdd_mcp_server_http(&c, &cfg, wire, n, &response, &z) == MULIB_CDD_MCP_OK);
        assert(mulib_cdd_mcp_client_http_receive(&cl, response, z, &dom) == MULIB_CDD_MCP_OK);
        assert(mulib_json_numeric(mulib_json_get(mulib_json_get(dom, "result"), "structuredContent")) == 42);
        assert(calls == 1);
        mulib_json_destroy(dom);
        free(response); free(wire); free(q);
        mulib_json_destroy(arg);
        independent_sse(&cl);
    }
    mulib_json_destroy(prompts);
    mulib_json_destroy(bad);
    mulib_json_destroy(schema);
    return 0;
}
