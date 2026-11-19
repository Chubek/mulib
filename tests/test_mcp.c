#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4
#define MULIB_IMPL_CDD_MCP_CLIENT
#define MULIB_IMPL_CDD_MCP_SERVER
#include "mulib-mcp-client.h"
#include "mulib-mcp-server.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define META "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"," \
    "\"io.modelcontextprotocol/clientCapabilities\":{}}"
#define START "{\"jsonrpc\":\"2.0\","

static mulib_json_t *parse(const char *s)
{
    mulib_json_t *v = mulib_json_parse(s, strlen(s), NULL);
    assert(v);
    return v;
}

static int eq(const mulib_json_t *v, const char *s)
{
    size_t n;
    const char *p = mulib_json_text(v, &n);
    return p && n == strlen(s) && memcmp(p, s, n) == 0;
}

static mulib_json_t *send_frame(mulib_cdd_mcp_server_t *server, const char *frame)
{
    char *out = NULL;
    mulib_json_t *r;
    assert(mulib_cdd_mcp_server_process(server, frame, strlen(frame), &out)
           == MULIB_CDD_MCP_OK);
    if (!out)
        return NULL;
    assert(!strchr(out, '\n') && !strchr(out, '\r'));
    r = parse(out);
    free(out);
    return r;
}

static void wire_error(mulib_cdd_mcp_server_t *server, const char *frame, int code)
{
    mulib_json_t *r = send_frame(server, frame);
    assert(eq(mulib_json_get(r, "jsonrpc"), "2.0"));
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(r, "error"), "code"))
           == (double)code);
    assert(!mulib_json_get(r, "result"));
    mulib_json_destroy(r);
}

static mulib_cdd_mcp_tool_status_t echo(void *ctx, const mulib_json_t *args,
                                      char *text, size_t cap, size_t *len)
{
    const char *s;
    ++*(unsigned *)ctx;
    s = mulib_json_text(mulib_json_get(args, "value"), len);
    if (!s || *len > cap) {
        memcpy(text, "bad input", 9);
        *len = 9;
        return MULIB_CDD_MCP_TOOL_ERROR;
    }
    memcpy(text, s, *len);
    return MULIB_CDD_MCP_TOOL_OK;
}

static mulib_cdd_mcp_tool_status_t failed(void *ctx, const mulib_json_t *args,
                                        char *text, size_t cap, size_t *len)
{
    (void)ctx;
    (void)args;
    (void)text;
    *len = cap + 1;
    return MULIB_CDD_MCP_TOOL_OK;
}

static mulib_json_t *trip(mulib_cdd_mcp_client_t *cl, mulib_cdd_mcp_server_t *server,
                          mulib_cdd_mcp_operation_t op, const char *target,
                          const mulib_json_t *args, mulib_cdd_mcp_status_t expected_status)
{
    char *req = NULL, *res = NULL;
    mulib_json_t *response = NULL;
    assert(mulib_cdd_mcp_client_request(cl, op, target, args, &req) == MULIB_CDD_MCP_OK);
    assert(req && !strchr(req, '\n'));
    assert(mulib_cdd_mcp_server_process(server, req, strlen(req), &res)
           == MULIB_CDD_MCP_OK);
    if (op == MULIB_CDD_MCP_INITIALIZED_LEGACY) {
        assert(!res);
    } else {
        assert(res);
        assert(mulib_cdd_mcp_client_receive(cl, res, strlen(res), &response)
               == expected_status);
        assert(response);
    }
    free(req);
    free(res);
    return response;
}

static void current(mulib_cdd_mcp_server_t *server, unsigned *calls)
{
    mulib_json_t *r, *result, *item, *data;
    char *output = NULL;
    wire_error(server, "[{}]", -32600);
    wire_error(server, "{", -32700);
    wire_error(server, START "\"id\":null,\"method\":\"tools/list\"}", -32600);
    wire_error(server, START "\"id\":1.5,\"method\":\"tools/list\"}", -32600);
    wire_error(server, START "\"id\":9007199254740992,\"method\":\"tools/list\"}", -32600);
    r = send_frame(server, START "\"id\":\"correlate\",\"method\":false}");
    assert(eq(mulib_json_get(r, "id"), "correlate"));
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(r, "error"), "code"))
           == -32600.0);
    mulib_json_destroy(r);
    r = send_frame(server, START "\"id\":null,\"method\":false}");
    assert(!mulib_json_get(r, "id"));
    mulib_json_destroy(r);
    wire_error(server, START "\"id\":0,\"method\":\"tools/list\"}", -32602);
    wire_error(server, START "\"id\":1,\"method\":\"server/discover\",\"params\":{"
               "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"}}}",
               -32602);
    wire_error(server, START "\"id\":1,\"method\":\"ping\",\"params\":{" META "}}", -32601);
    assert(!send_frame(server, START "\"method\":\"tools/call\",\"params\":{"
                       META ",\"name\":\"echo\"}}"));
    wire_error(server, START "\"id\":1,\"method\":\"tools/call\",\"params\":{"
               META ",\"name\":\"echo\\u0000suffix\"}}", -32602);
    wire_error(server, START "\"id\":1,\"method\":\"tools/call\",\"params\":{"
               META ",\"name\":\"echo\",\"arguments\":[]}}", -32602);
    wire_error(server, START "\"id\":1,\"method\":\"tools/call\",\"params\":{"
               META ",\"name\":\"broken\"}}", -32603);
    assert(*calls == 0);
    r = send_frame(server, START "\"id\":1,\"method\":\"server/discover\",\"params\":{"
                   "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"1900-01-01\","
                   "\"io.modelcontextprotocol/clientCapabilities\":{}}}}");
    item = mulib_json_get(r, "error");
    assert(mulib_json_numeric(mulib_json_get(item, "code")) == -32022.0);
    data = mulib_json_get(item, "data");
    assert(eq(mulib_json_get(data, "requested"), "1900-01-01"));
    assert(eq(mulib_json_at(mulib_json_get(data, "supported"), 0), "2026-07-28"));
    mulib_json_destroy(r);

    /* Current tools/list works independently of discovery and legacy state. */
    r = send_frame(server, START "\"id\":\"a\\u0000b\",\"method\":\"tools/list\","
                   "\"params\":{" META "}}");
    {
        size_t n;
        const char *s = mulib_json_text(mulib_json_get(r, "id"), &n);
        assert(s && n == 3 && !memcmp(s, "a\0b", 3));
    }
    result = mulib_json_get(r, "result");
    assert(eq(mulib_json_get(result, "resultType"), "complete"));
    assert(eq(mulib_json_get(result, "cacheScope"), "private"));
    assert(mulib_json_numeric(mulib_json_get(result, "ttlMs")) == 0);
    item = mulib_json_get(mulib_json_get(result, "_meta"), "io.modelcontextprotocol/serverInfo");
    assert(eq(mulib_json_get(item, "name"), "server"));
    assert(eq(mulib_json_get(mulib_json_at(mulib_json_get(result, "tools"), 0), "name"), "echo"));
    assert(!mulib_json_get(result, "nextCursor"));
    mulib_json_destroy(r);
    assert(mulib_cdd_mcp_server_process(server, "x", MULIB_CDD_MCP_MAX_FRAME + 1u,
                                        &output) == MULIB_CDD_MCP_LIMIT);
    assert(mulib_cdd_mcp_server_process(server, "\n", 1, &output) == MULIB_CDD_MCP_INVALID);
    assert(!output);
}

static void peers(mulib_cdd_mcp_server_t *server, unsigned *calls)
{
    mulib_cdd_mcp_client_t cl;
    mulib_json_t *r, *arg = parse("{\"value\":\"line\\nnext \\ud83d\\ude00\"}");
    char *req = NULL;
    assert(mulib_cdd_mcp_client_init(&cl, "client", "1") == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_TOOLS_LIST, NULL, NULL, &req)
           == MULIB_CDD_MCP_STATE);
    r = trip(&cl, server, MULIB_CDD_MCP_DISCOVER, NULL, NULL, MULIB_CDD_MCP_OK);
    assert(mulib_json_size(mulib_json_get(mulib_json_get(r, "result"), "supportedVersions"))
           == 2);
    mulib_json_destroy(r);
    mulib_json_destroy(trip(&cl, server, MULIB_CDD_MCP_TOOLS_LIST, NULL, NULL,
                            MULIB_CDD_MCP_OK));
    r = trip(&cl, server, MULIB_CDD_MCP_TOOLS_CALL, "echo", arg, MULIB_CDD_MCP_OK);
    assert(eq(mulib_json_get(mulib_json_at(mulib_json_get(mulib_json_get(r, "result"),
                           "content"), 0), "text"), "line\nnext \xf0\x9f\x98\x80"));
    mulib_json_destroy(r);
    mulib_json_destroy(arg);
    r = trip(&cl, server, MULIB_CDD_MCP_TOOLS_CALL, "echo", NULL, MULIB_CDD_MCP_OK);
    assert(mulib_json_boolean(mulib_json_get(mulib_json_get(r, "result"), "isError")));
    mulib_json_destroy(r);
    mulib_json_destroy(trip(&cl, server, MULIB_CDD_MCP_TOOLS_CALL, "not-registered",
                            NULL, MULIB_CDD_MCP_REMOTE_ERROR));
    assert(*calls == 2);
    assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_PING_LEGACY, NULL, NULL,
                                         &req) == MULIB_CDD_MCP_STATE);
    assert(mulib_cdd_mcp_client_init(&cl, "client", "1") == MULIB_CDD_MCP_OK);
    assert(!send_frame(server, START "\"method\":\"notifications/initialized\"}"));
    r = trip(&cl, server, MULIB_CDD_MCP_INITIALIZE_LEGACY, NULL, NULL, MULIB_CDD_MCP_OK);
    assert(eq(mulib_json_get(mulib_json_get(r, "result"), "protocolVersion"), "2025-11-25"));
    assert(!mulib_json_get(mulib_json_get(r, "result"), "resultType"));
    mulib_json_destroy(r);
    wire_error(server, START "\"id\":5,\"method\":\"tools/list\"}", -32602);
    wire_error(server, START "\"id\":5,\"method\":\"notifications/initialized\"}", -32602);
    assert(!send_frame(server, START "\"method\":\"notifications/initialized\",\"params\":[]}"));
    wire_error(server, START "\"id\":5,\"method\":\"tools/list\"}", -32602);
    mulib_json_destroy(trip(&cl, server, MULIB_CDD_MCP_PING_LEGACY, NULL, NULL,
                            MULIB_CDD_MCP_OK));
    assert(!trip(&cl, server, MULIB_CDD_MCP_INITIALIZED_LEGACY, NULL, NULL,
                 MULIB_CDD_MCP_OK));
    r = trip(&cl, server, MULIB_CDD_MCP_TOOLS_LIST, NULL, NULL, MULIB_CDD_MCP_OK);
    assert(!mulib_json_get(mulib_json_get(r, "result"), "cacheScope"));
    mulib_json_destroy(r);
}

static mulib_cdd_mcp_status_t receive(mulib_cdd_mcp_client_t *cl, const char *s,
                                     mulib_json_t **out)
{
    return mulib_cdd_mcp_client_receive(cl, s, strlen(s), out);
}

static void start_probe(mulib_cdd_mcp_client_t *cl)
{
    char *frame = NULL;
    assert(mulib_cdd_mcp_client_init(cl, "host", "1") == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_request(cl, MULIB_CDD_MCP_DISCOVER, NULL, NULL, &frame)
           == MULIB_CDD_MCP_OK);
    assert(strstr(frame, "\"method\":\"server/discover\"")
           && strstr(frame, "\"io.modelcontextprotocol/clientCapabilities\":{}"));
    free(frame);
}

static void fallback(void)
{
    mulib_cdd_mcp_client_t cl;
    mulib_json_t *r = NULL;
    char *q = NULL;
    start_probe(&cl);
    assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_DISCOVER, NULL, NULL, &q)
           == MULIB_CDD_MCP_STATE);
    assert(receive(&cl, START "\"method\":\"notifications/progress\"}", &r)
           == MULIB_CDD_MCP_IGNORED);
    assert(receive(&cl, START "\"id\":\"mcp-2\",\"result\":{}}", &r)
           == MULIB_CDD_MCP_IGNORED);
    assert(receive(&cl, START "\"id\":\"mcp-1\",\"result\":{},\"error\":{}}", &r)
           == MULIB_CDD_MCP_INVALID);
    assert(!r && !q);
    assert(receive(&cl, START "\"id\":\"mcp-1\",\"error\":{\"code\":-32602,"
                   "\"message\":\"legacy pre-initialize error\"}}", &r)
           == MULIB_CDD_MCP_LEGACY_PEER);
    mulib_json_destroy(r);
    assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_INITIALIZE_LEGACY,
                                        NULL, NULL, &q) == MULIB_CDD_MCP_OK);
    assert(strstr(q, "2025-11-25") && strstr(q, "mcp-2"));
    free(q);
    assert(receive(&cl, START "\"id\":\"mcp-2\",\"result\":{\"protocolVersion\":"
                   "\"2025-06-18\",\"capabilities\":{},\"serverInfo\":{\"name\":\"old\","
                   "\"version\":\"1\"}}}", &r) == MULIB_CDD_MCP_NO_VERSION);
    mulib_json_destroy(r);
    assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_INITIALIZE_LEGACY,
                                        NULL, NULL, &q) == MULIB_CDD_MCP_STATE);
    start_probe(&cl);
    assert(receive(&cl, START "\"id\":\"mcp-1\",\"error\":{\"code\":-32022,"
                   "\"message\":\"bad version\",\"data\":{\"supported\":[\"2099-01-01\","
                   "\"2025-11-25\"],\"requested\":\"2026-07-28\"}}}", &r)
           == MULIB_CDD_MCP_NO_VERSION);
    mulib_json_destroy(r);
    assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_INITIALIZE_LEGACY,
                                        NULL, NULL, &q) == MULIB_CDD_MCP_STATE);
    start_probe(&cl);
    assert(mulib_cdd_mcp_client_abandon(&cl) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_abandon(&cl) == MULIB_CDD_MCP_STATE);
    assert(mulib_cdd_mcp_client_request(&cl, MULIB_CDD_MCP_INITIALIZE_LEGACY,
                                        NULL, NULL, &q) == MULIB_CDD_MCP_OK);
    assert(strstr(q, "mcp-2"));
    free(q);
}

static mulib_cdd_mcp_handler_status_t catalog_handler(
    void *user, const mulib_cdd_mcp_request_view_t *rq, mulib_json_t **out)
{
    ++*(unsigned *)user;
    if (!strcmp(rq->method, "resources/read")) {
        if (!strcmp(rq->target, "demo:/defer") && !rq->request_state) {
            *out = parse("\"a\\u0000b\"");
            return MULIB_CDD_MCP_HANDLER_DEFER;
        }
        if (!strcmp(rq->target, "demo:/defer")) {
            size_t n;
            const char *s = mulib_json_text(rq->request_state, &n);
            if (!s || n != 3 || memcmp(s, "a\0b", 3))
                return MULIB_CDD_MCP_HANDLER_INVALID;
        } else if (!strcmp(rq->target, "demo:/bad")) {
            *out = parse("{\"contents\":[{\"uri\":\"demo:/bad\",\"blob\":\"AR==\"}]}");
            return MULIB_CDD_MCP_HANDLER_OK;
        } else if (strcmp(rq->target, "demo:/guide")) {
            return MULIB_CDD_MCP_HANDLER_INVALID;
        }
        *out = parse("{\"contents\":[{\"uri\":\"demo:/guide\",\"text\":\"Hello\"},"
                     "{\"uri\":\"demo:/bytes\",\"blob\":\"AAEC\"}]}");
    } else if (!strcmp(rq->method, "prompts/get")) {
        assert(eq(mulib_json_get(rq->arguments, "topic"), "birds"));
        *out = parse("{\"messages\":[{\"role\":\"assistant\",\"content\":{\"type\":\"text\","
                     "\"text\":\"Look up local birds.\"}}]}");
    } else if (!strcmp(rq->method, "completion/complete")) {
        assert(eq(mulib_json_get(mulib_json_get(rq->params, "argument"), "value"), "b"));
        *out = parse("{\"completion\":{\"values\":[\"birds\"],\"total\":2,\"hasMore\":true}}");
    } else {
        *out = parse("{\"content\":[{\"type\":\"image\",\"data\":\"AQI=\",\"mimeType\":\"image/png\"}],"
                     "\"structuredContent\":{\"value\":7}}");
    }
    return MULIB_CDD_MCP_HANDLER_OK;
}

static int catalog_authorize(void *user, const mulib_cdd_mcp_request_view_t *rq)
{
    return rq->principal == user;
}

static int catalog_validate(void *user, const mulib_json_t *schema, const mulib_json_t *value)
{
    (void)user;
    return eq(mulib_json_get(schema, "title"), "output")
        ? mulib_json_numeric(mulib_json_get(value, "value")) == 7
        : eq(mulib_json_get(value, "word"), "birds");
}

static mulib_json_t *catalog_trip(mulib_cdd_mcp_client_t *cl, mulib_cdd_mcp_catalog_t *c,
                                 mulib_cdd_mcp_operation_t op, const char *target,
                                 const mulib_json_t *args, const mulib_cdd_mcp_request_options_t *opts,
                                 mulib_cdd_mcp_status_t status)
{
    char *q, *r;
    mulib_json_t *dom = NULL;
    assert(mulib_cdd_mcp_client_request_ex(cl, op, target, args, opts, &q) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_catalog_process(c, c->features.user, q, strlen(q), &r) == MULIB_CDD_MCP_OK);
    if (op != MULIB_CDD_MCP_INITIALIZED_LEGACY) {
        assert(r && receive(cl, r, &dom) == status && dom);
        free(r);
    } else {
        assert(!r);
    }
    free(q);
    return dom;
}

static void catalog(void)
{
    unsigned calls = 0;
    char *q, *r;
    const char *cursor;
    mulib_json_t *res, *page, *state, *args, *opts_meta;
    mulib_json_t *resources = parse("[{\"uri\":\"demo:/guide\",\"name\":\"Guide\"},"
                                  "{\"uri\":\"demo:/defer\",\"name\":\"Later\"}]");
    mulib_json_t *templates = parse("[{\"uriTemplate\":\"demo:/guide/{topic}{?lang}\",\"name\":\"Topics\"}]");
    mulib_json_t *prompts = parse("[{\"name\":\"ask\",\"arguments\":[{\"name\":\"topic\",\"required\":true}]}]");
    mulib_json_t *schema = parse("{\"type\":\"object\"}");
    mulib_json_t *output = parse("{\"type\":\"object\",\"title\":\"output\"}");
    const mulib_cdd_mcp_structured_tool_t tool = {
        "structured", NULL, schema, output, catalog_validate, catalog_handler, &calls
    };
    const mulib_cdd_mcp_server_config_t base = { "catalog", "1", NULL, 0, 1 };
    mulib_cdd_mcp_catalog_config_t config = {
        resources, templates, prompts, &tool, 1, catalog_handler, catalog_handler,
        catalog_handler, catalog_authorize, &calls, 1, 42, 200
    };
    mulib_cdd_mcp_catalog_t c;
    mulib_cdd_mcp_client_t cl;
    mulib_cdd_mcp_request_options_t opts = { NULL, NULL, 1 };
    config.read = NULL;
    assert(mulib_cdd_mcp_catalog_init(&c, &base, &config) == MULIB_CDD_MCP_INVALID);
    config.read = catalog_handler;
    assert(mulib_cdd_mcp_catalog_init(&c, &base, &config) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_init(&cl, "fixture", "1") == MULIB_CDD_MCP_OK);
    opts_meta = parse("{\"progressToken\":7,\"example.org/trace\":\"x\"}");
    opts.meta = opts_meta;
    assert(mulib_cdd_mcp_client_request_ex(&cl, MULIB_CDD_MCP_RESOURCES_READ, "no-scheme",
                                            NULL, &opts, &q) == MULIB_CDD_MCP_INVALID);
    assert(mulib_cdd_mcp_client_request_ex(&cl, MULIB_CDD_MCP_RESOURCES_READ, "demo:/guide",
                                            NULL, &opts, &q) == MULIB_CDD_MCP_OK);
    assert(strstr(q, "\"example.org/trace\":\"x\"") && strstr(q, "\"progressToken\":7"));
    assert(mulib_cdd_mcp_catalog_process(&c, NULL, q, strlen(q), &r) == MULIB_CDD_MCP_OK);
    assert(receive(&cl, r, &res) == MULIB_CDD_MCP_REMOTE_ERROR && calls == 0);
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(res, "error"), "code")) == -32000);
    free(q);
    free(r);
    mulib_json_destroy(res);
    mulib_json_destroy(opts_meta);
    opts.meta = NULL;
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_DISCOVER, NULL, NULL, NULL, MULIB_CDD_MCP_OK));
    page = catalog_trip(&cl, &c, MULIB_CDD_MCP_RESOURCES_LIST, NULL, NULL, NULL, MULIB_CDD_MCP_OK);
    res = mulib_json_get(page, "result");
    cursor = mulib_json_text(mulib_json_get(res, "nextCursor"), NULL);
    assert(cursor && mulib_json_size(mulib_json_get(res, "resources")) == 1);
    assert(mulib_json_numeric(mulib_json_get(res, "ttlMs")) == 200);
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_PROMPTS_LIST, cursor, NULL, NULL,
                                  MULIB_CDD_MCP_REMOTE_ERROR));
    res = catalog_trip(&cl, &c, MULIB_CDD_MCP_RESOURCES_LIST, cursor, NULL, NULL, MULIB_CDD_MCP_OK);
    assert(!mulib_json_get(mulib_json_get(res, "result"), "nextCursor"));
    mulib_json_destroy(res);
    ++config.generation;
    assert(mulib_cdd_mcp_catalog_init(&c, &base, &config) == MULIB_CDD_MCP_OK);
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_RESOURCES_LIST, cursor, NULL, NULL,
                                  MULIB_CDD_MCP_REMOTE_ERROR));
    mulib_json_destroy(page);
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_RESOURCE_TEMPLATES_LIST, NULL, NULL, NULL,
                                  MULIB_CDD_MCP_OK));
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_PROMPTS_LIST, NULL, NULL, NULL,
                                  MULIB_CDD_MCP_OK));
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_PROMPTS_GET, "ask", NULL, NULL,
                                  MULIB_CDD_MCP_REMOTE_ERROR));
    args = parse("{\"topic\":\"birds\"}");
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_PROMPTS_GET, "ask", args, NULL,
                                  MULIB_CDD_MCP_OK));
    mulib_json_destroy(args);
    args = parse("{\"ref\":{\"type\":\"ref/prompt\",\"name\":\"ask\"},"
                 "\"argument\":{\"name\":\"topic\",\"value\":\"b\"}}");
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_COMPLETE, NULL, args, NULL,
                                  MULIB_CDD_MCP_OK));
    mulib_json_destroy(args);
    args = parse("{\"ref\":{\"type\":\"ref/resource\",\"uri\":\"demo:/guide/{topic}{?lang}\"},"
                 "\"argument\":{\"name\":\"topic\",\"value\":\"b\"}}");
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_COMPLETE, NULL, args, NULL,
                                  MULIB_CDD_MCP_OK));
    mulib_json_destroy(args);
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_RESOURCES_READ, "demo:/bad", NULL, NULL,
                                  MULIB_CDD_MCP_REMOTE_ERROR));
    state = catalog_trip(&cl, &c, MULIB_CDD_MCP_RESOURCES_READ, "demo:/defer", NULL, NULL,
                         MULIB_CDD_MCP_INPUT_REQUIRED);
    opts.request_state = mulib_json_get(mulib_json_get(state, "result"), "requestState");
    assert(!mulib_json_get(mulib_json_get(state, "result"), "ttlMs"));
    res = catalog_trip(&cl, &c, MULIB_CDD_MCP_RESOURCES_READ, "demo:/defer", NULL, &opts, MULIB_CDD_MCP_OK);
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(res, "result"), "ttlMs")) == 0);
    mulib_json_destroy(res);
    mulib_json_destroy(state);
    args = parse("{\"word\":\"birds\"}");
    res = catalog_trip(&cl, &c, MULIB_CDD_MCP_TOOLS_CALL, "structured", args, NULL, MULIB_CDD_MCP_OK);
    assert(catalog_validate(NULL, output, mulib_json_get(mulib_json_get(res, "result"), "structuredContent")));
    mulib_json_destroy(res);
    assert(mulib_cdd_mcp_client_init(&cl, "legacy", "1") == MULIB_CDD_MCP_OK);
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_INITIALIZE_LEGACY, NULL, NULL, NULL, MULIB_CDD_MCP_OK));
    assert(!catalog_trip(&cl, &c, MULIB_CDD_MCP_INITIALIZED_LEGACY, NULL, NULL, NULL, MULIB_CDD_MCP_OK));
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_RESOURCES_READ, "demo:/guide", NULL, NULL,
                                  MULIB_CDD_MCP_OK));
    mulib_json_destroy(catalog_trip(&cl, &c, MULIB_CDD_MCP_TOOLS_CALL, "structured", args, NULL,
                                  MULIB_CDD_MCP_OK));
    mulib_json_destroy(args);
    mulib_json_destroy(output);
    mulib_json_destroy(schema);
    mulib_json_destroy(prompts);
    mulib_json_destroy(templates);
    mulib_json_destroy(resources);
}

static void independent_responses(void)
{
    mulib_cdd_mcp_client_t cl;
    const mulib_cdd_mcp_request_options_t direct = { NULL, NULL, 1 };
    mulib_json_t *res = NULL;
    char *q;
    assert(mulib_cdd_mcp_client_init(&cl, "example", "1") == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_request_ex(&cl, MULIB_CDD_MCP_TOOLS_CALL, "t", NULL, &direct, &q)
           == MULIB_CDD_MCP_OK);
    free(q);
    assert(receive(&cl, START "\"id\":\"mcp-1\",\"result\":{\"resultType\":\"complete\","
                   "\"content\":[{\"type\":\"audio\",\"mimeType\":\"audio/wav\",\"data\":\"xx\"}]}}",
                   &res) == MULIB_CDD_MCP_INVALID && !res);
    assert(receive(&cl, START "\"id\":\"mcp-1\",\"id\":\"mcp-1\",\"result\":{\"resultType\":\"complete\","
                   "\"content\":[]}}", &res) == MULIB_CDD_MCP_INVALID && !res);
    assert(receive(&cl, START "\"id\":\"mcp-1\",\"result\":{\"resultType\":\"complete\","
                   "\"content\":[],\"structuredContent\":[1,true,null]}}", &res) == MULIB_CDD_MCP_OK);
    mulib_json_destroy(res);
    assert(mulib_cdd_mcp_client_request_ex(&cl, MULIB_CDD_MCP_RESOURCES_READ, "demo:/data", NULL,
                                           &direct, &q) == MULIB_CDD_MCP_OK);
    free(q);
    assert(mulib_cdd_mcp_client_cancel(&cl, "not needed", &q) == MULIB_CDD_MCP_OK);
    res = parse(q);
    assert(eq(mulib_json_get(res, "method"), "notifications/cancelled") && !mulib_json_get(res, "id"));
    assert(eq(mulib_json_get(mulib_json_get(res, "params"), "requestId"), "mcp-2"));
    mulib_json_destroy(res);
    free(q);
}

int main(void)
{
    unsigned calls = 0;
    mulib_json_t *schema = parse("{\"type\":\"object\"}");
    mulib_cdd_mcp_tool_t tools[] = { { "echo", "Echo one string", schema, echo, &calls },
                                    { "broken", NULL, schema, failed, NULL } };
    mulib_cdd_mcp_server_config_t config = { "server", "1", tools, 2, 1 };
    mulib_cdd_mcp_server_t server;
    mulib_cdd_mcp_server_config_t invalid = config;
    invalid.tool_count = MULIB_CDD_MCP_MAX_TOOLS + 1u;
    assert(mulib_cdd_mcp_server_init(&server, &invalid) == MULIB_CDD_MCP_LIMIT);
    tools[1].name = "bad name";
    assert(mulib_cdd_mcp_server_init(&server, &config) == MULIB_CDD_MCP_INVALID);
    tools[1].name = tools[0].name;
    assert(mulib_cdd_mcp_server_init(&server, &config) == MULIB_CDD_MCP_CONFLICT);
    tools[1].name = "broken";
    assert(mulib_cdd_mcp_server_init(&server, &config) == MULIB_CDD_MCP_OK);
    current(&server, &calls);
    peers(&server, &calls);
    fallback();
    catalog();
    independent_responses();
    config.allow_legacy = 0;
    config.tool_count = 0;
    assert(mulib_cdd_mcp_server_init(&server, &config) == MULIB_CDD_MCP_OK);
    wire_error(&server, START "\"id\":1,\"method\":\"initialize\"}", -32601);
    wire_error(&server, START "\"id\":1,\"method\":\"tools/list\",\"params\":{"
               META "}}", -32601);
    mulib_json_destroy(schema);
    return 0;
}
