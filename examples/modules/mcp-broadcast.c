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
#define MULIB_IMPL_MCP_CLIENT
#define MULIB_IMPL_MCP_SERVER
#define MULIB_IMPL_CDD_MCP_HTTP
#define MULIB_IMPL_MCP_BROADCAST
#include "mulib-mcp-broadcast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEMO_HOST "tools.example"
#define DEMO_PATH "/mcp"
#define DEMO_URL "https://" DEMO_HOST DEMO_PATH

static const char demo_principal;

/* In-process fixture only. A public endpoint must verify real credentials. */
static const void *verify(void *user, const char *resource,
                          const char *token, size_t length)
{
    (void)user;
    return !strcmp(resource, DEMO_URL) && length == 7 && !memcmp(token, "fixture", 7)
        ? &demo_principal : NULL;
}

static int authorize(void *user, const mulib_mcp_request_view_t *request)
{
    (void)user;
    if (request->principal != &demo_principal)
        return 0;
    if (!strcmp(request->method, "server/discover") || !strcmp(request->method, "tools/list"))
        return 1;
    return !strcmp(request->method, "tools/call")
        && request->target && !strcmp(request->target, "greet");
}

static mulib_mcp_tool_status_t greet(void *user, const mulib_json_t *arguments,
                                   char *text, size_t capacity, size_t *length)
{
    (void)user;
    if (mulib_json_size(arguments) || capacity < 5)
        return MULIB_MCP_TOOL_FAILED;
    memcpy(text, "Hello", 5);
    *length = 5;
    return MULIB_MCP_TOOL_OK;
}

int main(void)
{
    static const char input[] = "{\"type\":\"object\",\"additionalProperties\":false}";
    mulib_json_t *schema = mulib_json_parse(input, sizeof input - 1, NULL);
    const mulib_mcp_tool_t tool = { "greet", "Return a fixed greeting", schema, greet, NULL };
    const mulib_mcp_server_config_t base = { "broadcast-demo", "1", &tool, 1, 0 };
    const mulib_mcp_catalog_config_t features = { .authorize = authorize };
    const mulib_mcp_broadcast_config_t endpoint = { .resource_url = DEMO_URL, .verify = verify };
    const mulib_mcp_client_request_options_t direct = { .direct = 1 };
    mulib_mcp_catalog_t catalog;
    mulib_mcp_client_t client;
    mulib_json_t *result = NULL;
    char *frame = NULL;
    unsigned char *request = NULL, *response = NULL;
    size_t request_length, response_length;
    int ok = 0;

    if (schema
        && mulib_mcp_catalog_init(&catalog, &base, &features) == MULIB_MCP_SERVER_OK
        && mulib_mcp_client_init(&client, "demo-client", "1") == MULIB_MCP_CLIENT_OK
        && mulib_mcp_client_request_ex(&client, MULIB_MCP_CLIENT_TOOLS_CALL,
            "greet", NULL, &direct, &frame) == MULIB_MCP_CLIENT_OK
        && mulib_cdd_mcp_client_http_request(DEMO_HOST, DEMO_PATH, "Bearer fixture",
            frame, strlen(frame), schema, &request, &request_length) == MULIB_CDD_MCP_OK
        && mulib_mcp_broadcast_http(&catalog, &endpoint, DEMO_HOST, DEMO_PATH,
            request, request_length, &response, &response_length) == MULIB_CDD_MCP_OK
        && mulib_cdd_mcp_client_http_receive(&client, response,
            response_length, &result) == MULIB_CDD_MCP_OK) {
        const mulib_json_t *payload = mulib_json_get(result, "result");
        const mulib_json_t *item = mulib_json_at(mulib_json_get(payload, "content"), 0);
        size_t n = 0;
        const char *text = mulib_json_text(mulib_json_get(item, "text"), &n);
        ok = text && n == 5 && !memcmp(text, "Hello", 5) && puts("Hello") != EOF;
    }
    mulib_json_destroy(schema);
    mulib_json_destroy(result);
    free(frame);
    free(request);
    free(response);
    return !ok;
}
