#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4

/* Exercise declaration-first use and allow the legacy switch to be tested. */
#include "mulib-mcp-server.h"
#ifndef MULIB_IMPL_CDD_MCP_SERVER
#define MULIB_IMPL_MCP_SERVER
#endif
#include "mulib-mcp-server.h"
#include "mulib-mcp-server.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define START "{\"jsonrpc\":\"2.0\","
#define META "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"" \
    MULIB_MCP_SERVER_VERSION "\",\"io.modelcontextprotocol/clientCapabilities\":{}}"

static mulib_json_t *parse(const char *text)
{
    mulib_json_t *v = mulib_json_parse(text, strlen(text), NULL);
    assert(v);
    return v;
}

static int text_is(const mulib_json_t *v, const char *expected)
{
    size_t n;
    const char *text = mulib_json_text(v, &n);
    return text && n == strlen(expected) && !memcmp(text, expected, n);
}

static mulib_json_t *reply(char *out)
{
    mulib_json_t *v;
    assert(out && !strchr(out, '\r') && !strchr(out, '\n'));
    v = parse(out);
    free(out);
    assert(text_is(mulib_json_get(v, "jsonrpc"), "2.0"));
    return v;
}

static void wire_error(char *out, int code)
{
    mulib_json_t *v = reply(out);
    assert(!mulib_json_get(v, "result"));
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(v, "error"), "code"))
           == code);
    mulib_json_destroy(v);
}

static mulib_mcp_tool_status_t text_tool(void *user, const mulib_json_t *args,
                                        char *text, size_t capacity, size_t *length)
{
    assert(!args && capacity == MULIB_MCP_SERVER_MAX_TEXT);
    ++*(unsigned *)user;
    memcpy(text, "ok", 2);
    *length = 2;
    return MULIB_MCP_TOOL_OK;
}

static void base_server(const mulib_json_t *schema)
{
    const char initialize[] = START "\"id\":1,\"method\":\"initialize\",\"params\":{"
        "\"protocolVersion\":\"" MULIB_MCP_SERVER_LEGACY_VERSION "\","
        "\"capabilities\":{},\"clientInfo\":{\"name\":\"fixture\",\"version\":\"1\"}}}";
    const char initialized[] = START "\"method\":\"notifications/initialized\"}";
    const char call[] = START "\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"text\"}}";
    static const char oversized[MULIB_MCP_SERVER_MAX_FRAME + 1] = { 0 };
    char sentinel = 0, *out = &sentinel;
    unsigned calls = 0;
    mulib_mcp_server_t server;
    const mulib_mcp_tool_t tool = { "text", NULL, schema, text_tool, &calls };
    const mulib_mcp_server_config_t config = { "fixture", "1", &tool, 1, 1 };
    mulib_json_t *v;
    const mulib_json_t *content;

    /* Mix old and new symbols on one stateful instance. */
    assert(mulib_cdd_mcp_server_init(&server, &config) == MULIB_CDD_MCP_OK);
    assert(mulib_mcp_server_process(&server, "\n", 1, &out) == MULIB_MCP_SERVER_INVALID);
    assert(out == &sentinel);
    assert(mulib_mcp_server_process(&server, oversized, sizeof oversized, &out)
           == MULIB_MCP_SERVER_LIMIT);
    assert(out == &sentinel && calls == 0);
    assert(mulib_mcp_server_process(&server, initialize, sizeof initialize - 1, &out)
           == MULIB_MCP_SERVER_OK);
    v = reply(out);
    assert(text_is(mulib_json_get(mulib_json_get(v, "result"), "protocolVersion"),
                   MULIB_MCP_SERVER_LEGACY_VERSION));
    mulib_json_destroy(v);
    assert(mulib_mcp_server_init(&server, NULL) == MULIB_MCP_SERVER_INVALID);
    out = &sentinel;
    assert(mulib_mcp_server_process(&server, initialized, sizeof initialized - 1, &out)
           == MULIB_MCP_SERVER_OK && !out);
    assert(mulib_cdd_mcp_server_process(&server, call, sizeof call - 1, &out)
           == MULIB_CDD_MCP_OK && calls == 1);
    v = reply(out);
    content = mulib_json_get(mulib_json_get(v, "result"), "content");
    assert(text_is(mulib_json_get(mulib_json_at(content, 0), "text"), "ok"));
    mulib_json_destroy(v);

    assert(mulib_mcp_server_init(&server, &config) == MULIB_MCP_SERVER_OK);
    out = NULL;
    assert(mulib_mcp_server_process(&server, call, sizeof call - 1, &out)
           == MULIB_MCP_SERVER_OK && calls == 1);
    wire_error(out, -32602);
}

static int authorize(void *user, const mulib_mcp_request_view_t *request)
{
    assert(request->modern && !strcmp(request->method, "tools/call"));
    return request->principal == user;
}

static int validate(void *user, const mulib_json_t *schema, const mulib_json_t *args)
{
    (void)user;
    assert(text_is(mulib_json_get(schema, "type"), "object"));
    return text_is(mulib_json_get(args, "word"), "hello");
}

static mulib_mcp_handler_status_t structured_tool(
    void *user, const mulib_mcp_request_view_t *request, mulib_json_t **out)
{
    assert(request->principal == user && !strcmp(request->target, "structured"));
    ++*(unsigned *)user;
    *out = parse("{\"content\":[],\"structuredContent\":{\"word\":\"hello\"}}");
    return MULIB_MCP_HANDLER_OK;
}

static void catalog_server(const mulib_json_t *schema)
{
    const char good[] = START "\"id\":1,\"method\":\"tools/call\",\"params\":{" META
        ",\"name\":\"structured\",\"arguments\":{\"word\":\"hello\"}}}";
    const char bad[] = START "\"id\":2,\"method\":\"tools/call\",\"params\":{" META
        ",\"name\":\"structured\",\"arguments\":{\"word\":\"goodbye\"}}}";
    const mulib_mcp_server_config_t base = { "catalog", "1", NULL, 0, 0 };
    unsigned calls = 0;
    const mulib_mcp_structured_tool_t tool = {
        "structured", NULL, schema, NULL, validate, structured_tool, &calls
    };
    const mulib_mcp_catalog_config_t features = {
        NULL, NULL, NULL, &tool, 1, NULL, NULL, NULL, authorize, &calls, 0, 0, 0
    };
    mulib_mcp_catalog_t catalog;
    mulib_json_t *v;
    const mulib_json_t *result;
    char sentinel = 0, *out = &sentinel;

    assert(mulib_mcp_catalog_init(&catalog, &base, &features) == MULIB_MCP_SERVER_OK);
    assert(mulib_cdd_mcp_catalog_init(&catalog, &base, NULL) == MULIB_CDD_MCP_INVALID);
    assert(mulib_mcp_catalog_process(NULL, &calls, good, sizeof good - 1, &out)
           == MULIB_MCP_SERVER_INVALID && out == &sentinel);
    assert(mulib_mcp_catalog_process(&catalog, NULL, good, sizeof good - 1, &out)
           == MULIB_MCP_SERVER_OK && calls == 0);
    wire_error(out, -32000);
    out = NULL;
    assert(mulib_cdd_mcp_catalog_process(&catalog, &calls, bad, sizeof bad - 1, &out)
           == MULIB_CDD_MCP_OK && calls == 0);
    wire_error(out, -32602);
    out = NULL;
    assert(mulib_mcp_catalog_process(&catalog, &calls, good, sizeof good - 1, &out)
           == MULIB_MCP_SERVER_OK && calls == 1);
    v = reply(out);
    assert(mulib_json_numeric(mulib_json_get(v, "id")) == 1);
    result = mulib_json_get(v, "result");
    assert(text_is(mulib_json_get(result, "resultType"), "complete"));
    assert(text_is(mulib_json_get(mulib_json_get(result, "structuredContent"), "word"),
                   "hello"));
    mulib_json_destroy(v);
}

int main(void)
{
    mulib_json_t *schema = parse("{\"type\":\"object\"}");
    base_server(schema);
    catalog_server(schema);
    mulib_json_destroy(schema);
    return 0;
}
