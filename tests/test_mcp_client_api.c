#include "mulib-mcp-client.h"

#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4
#define MULIB_IMPL_MCP_CLIENT
#include "mulib-json.h"
#include "mulib-mcp-client.h"
#include "mulib-mcp-client.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define RPC "{\"jsonrpc\":\"2.0\","
#define VERSION_KEY "io.modelcontextprotocol/protocolVersion"
#define CACHE "\"ttlMs\":0,\"cacheScope\":\"private\""

static mulib_json_t *parse(const char *text)
{
    mulib_json_t *dom = mulib_json_parse(text, strlen(text), NULL);
    assert(dom);
    return dom;
}

static void expect_text(const mulib_json_t *item, const char *value)
{
    size_t length = 0;
    const char *text = mulib_json_text(item, &length);
    assert(text && length == strlen(value) && !memcmp(text, value, length));
}

static mulib_json_t *request_dom(char *frame, const char *method, const char *id)
{
    mulib_json_t *dom;
    assert(frame && !strchr(frame, '\r') && !strchr(frame, '\n'));
    dom = parse(frame);
    free(frame);
    expect_text(mulib_json_get(dom, "jsonrpc"), "2.0");
    expect_text(mulib_json_get(dom, "method"), method);
    if (id)
        expect_text(mulib_json_get(dom, "id"), id);
    else
        assert(!mulib_json_get(dom, "id"));
    return dom;
}

static mulib_mcp_client_status_t receive(mulib_mcp_client_t *client,
                                         const char *text, mulib_json_t **out)
{
    return mulib_mcp_client_receive(client, text, strlen(text), out);
}

static void discovery(void)
{
    static const char discovery_reply[] = RPC "\"id\":\"mcp-1\",\"result\":{"
        "\"resultType\":\"complete\",\"supportedVersions\":[\"" MULIB_MCP_CLIENT_VERSION
        "\"],\"capabilities\":{\"tools\":{}}," CACHE "}}";
    static const char tools_reply[] = RPC "\"id\":\"mcp-2\",\"result\":{"
        "\"resultType\":\"complete\",\"tools\":[]," CACHE "}}";
    static char oversized[MULIB_MCP_CLIENT_MAX_FRAME + 1u];
    mulib_mcp_client_t client;
    mulib_json_t *request;
    mulib_json_t *sentinel = parse("{}"), *response = sentinel;
    const mulib_json_t *meta, *identity;
    char marker[] = "", *frame = marker;
    assert(mulib_mcp_client_init(&client, "", "1") == MULIB_MCP_CLIENT_INVALID);
    assert(mulib_mcp_client_init(&client, "fixture", "1") == MULIB_MCP_CLIENT_OK);
    assert(mulib_mcp_client_request(&client, MULIB_MCP_CLIENT_TOOLS_LIST,
                                    NULL, NULL, &frame) == MULIB_MCP_CLIENT_STATE);
    assert(frame == marker);
    assert(mulib_mcp_client_request(&client, MULIB_MCP_CLIENT_DISCOVER,
                                    NULL, NULL, &frame) == MULIB_MCP_CLIENT_OK);
    request = request_dom(frame, "server/discover", "mcp-1");
    meta = mulib_json_get(mulib_json_get(request, "params"), "_meta");
    expect_text(mulib_json_get(meta, VERSION_KEY), MULIB_MCP_CLIENT_VERSION);
    identity = mulib_json_get(meta, "io.modelcontextprotocol/clientInfo");
    expect_text(mulib_json_get(identity, "name"), "fixture");
    mulib_json_destroy(request);
    frame = marker;
    assert(mulib_mcp_client_request(&client, MULIB_MCP_CLIENT_DISCOVER,
                                    NULL, NULL, &frame) == MULIB_MCP_CLIENT_STATE);
    assert(frame == marker);
    assert(receive(&client, RPC "\"id\":\"mcp-1\",\"result\":{}}\n",
                   &response) == MULIB_MCP_CLIENT_INVALID);
    assert(receive(&client, RPC "\"id\":\"mcp-1\",\"result\":{}}",
                   &response) == MULIB_MCP_CLIENT_INVALID);
    assert(receive(&client, RPC "\"id\":\"mcp-0\",\"result\":{}}",
                   &response) == MULIB_MCP_CLIENT_IGNORED);
    assert(mulib_mcp_client_receive(&client, oversized, sizeof oversized,
                                    &response) == MULIB_MCP_CLIENT_LIMIT);
    assert(response == sentinel);
    response = NULL;
    assert(receive(&client, discovery_reply, &response) == MULIB_MCP_CLIENT_OK);
    expect_text(mulib_json_get(response, "id"), "mcp-1");
    mulib_json_destroy(response);
    assert(mulib_mcp_client_request(&client, MULIB_MCP_CLIENT_RESOURCES_LIST,
                                    NULL, NULL, &frame) == MULIB_MCP_CLIENT_UNSUPPORTED);
    assert(frame == marker);
    assert(mulib_mcp_client_request(&client, MULIB_MCP_CLIENT_TOOLS_LIST,
                                    NULL, NULL, &frame) == MULIB_MCP_CLIENT_OK);
    mulib_json_destroy(request_dom(frame, "tools/list", "mcp-2"));
    response = NULL;
    assert(receive(&client, tools_reply, &response) == MULIB_MCP_CLIENT_OK);
    assert(mulib_json_typeof(mulib_json_get(mulib_json_get(response, "result"), "tools"))
           == MULIB_JSON_ARRAY);
    mulib_json_destroy(response);
    mulib_json_destroy(sentinel);
}

static void direct_and_cancel(void)
{
    mulib_mcp_client_t client;
    mulib_json_t *forbidden_meta = parse("{\"" VERSION_KEY "\":\"unknown\"}");
    mulib_json_t *extra_meta = parse("{\"progressToken\":\"page\"}");
    mulib_mcp_client_request_options_t options = { forbidden_meta, NULL, 1 };
    mulib_json_t *request, *cancel, *response = NULL;
    const mulib_json_t *meta;
    char marker[] = "", *frame = marker;
    assert(mulib_mcp_client_init(&client, "fixture", "1") == MULIB_MCP_CLIENT_OK);
    assert(mulib_mcp_client_cancel(&client, NULL, &frame) == MULIB_MCP_CLIENT_STATE);
    assert(mulib_mcp_client_request_ex(&client, MULIB_MCP_CLIENT_RESOURCES_LIST,
                                       NULL, NULL, &options, &frame)
           == MULIB_MCP_CLIENT_INVALID);
    assert(frame == marker);
    options.meta = extra_meta;
    mulib_json_destroy(forbidden_meta);
    assert(mulib_mcp_client_request_ex(&client, MULIB_MCP_CLIENT_RESOURCES_LIST,
                                       NULL, NULL, &options, &frame)
           == MULIB_MCP_CLIENT_OK);
    mulib_json_destroy(extra_meta);
    request = request_dom(frame, "resources/list", "mcp-1");
    meta = mulib_json_get(mulib_json_get(request, "params"), "_meta");
    expect_text(mulib_json_get(meta, "progressToken"), "page");
    expect_text(mulib_json_get(meta, VERSION_KEY), MULIB_MCP_CLIENT_VERSION);
    mulib_json_destroy(request);
    assert(mulib_mcp_client_cancel(&client, "timeout", &frame) == MULIB_MCP_CLIENT_OK);
    cancel = request_dom(frame, "notifications/cancelled", NULL);
    expect_text(mulib_json_get(mulib_json_get(cancel, "params"), "requestId"), "mcp-1");
    mulib_json_destroy(cancel);
    assert(receive(&client, RPC "\"id\":\"mcp-1\",\"error\":{"
                   "\"code\":-32603,\"message\":\"late reply\"}}",
                   &response) == MULIB_MCP_CLIENT_IGNORED);
    assert(!response);
    assert(mulib_mcp_client_abandon(&client) == MULIB_MCP_CLIENT_STATE);
}

static void legacy_compatibility(void)
{
    struct mulib_cdd_mcp_client client;
    mulib_mcp_client_t *same_client = &client;
    mulib_json_t *request, *response = NULL;
    char *frame = NULL;
    assert(mulib_cdd_mcp_client_init(&client, "fixture", "1") == MULIB_CDD_MCP_OK);
    assert(mulib_mcp_client_request(same_client, MULIB_MCP_CLIENT_DISCOVER,
                                    NULL, NULL, &frame) == MULIB_MCP_CLIENT_OK);
    mulib_json_destroy(request_dom(frame, "server/discover", "mcp-1"));
    assert(mulib_mcp_client_abandon(same_client) == MULIB_MCP_CLIENT_OK);
    assert(mulib_cdd_mcp_client_request(&client, MULIB_CDD_MCP_INITIALIZE_LEGACY,
                                        NULL, NULL, &frame) == MULIB_CDD_MCP_OK);
    request = request_dom(frame, "initialize", "mcp-2");
    expect_text(mulib_json_get(mulib_json_get(request, "params"), "protocolVersion"),
                MULIB_MCP_CLIENT_LEGACY_VERSION);
    mulib_json_destroy(request);
    assert(receive(same_client, RPC "\"id\":\"mcp-2\",\"result\":{"
                   "\"protocolVersion\":\"" MULIB_MCP_CLIENT_LEGACY_VERSION
                   "\",\"capabilities\":{},\"serverInfo\":{\"name\":\"fixture-peer\","
                   "\"version\":\"1\"}}}", &response) == MULIB_MCP_CLIENT_OK);
    mulib_json_destroy(response);
    assert(mulib_mcp_client_request(same_client, MULIB_MCP_CLIENT_INITIALIZED_LEGACY,
                                    NULL, NULL, &frame) == MULIB_MCP_CLIENT_OK);
    mulib_json_destroy(request_dom(frame, "notifications/initialized", NULL));
}

int main(void)
{
    discovery();
    direct_and_cancel();
    legacy_compatibility();
    return 0;
}
