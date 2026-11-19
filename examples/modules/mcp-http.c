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
#include <stdlib.h>
#include <string.h>

/* Intentionally anonymous, in-process discovery only; not a network policy. */
static unsigned allow_demo(void *user, const mulib_http_message_t *request,
                           const void **principal)
{
    (void)user;
    (void)request;
    *principal = NULL;
    return 200;
}

int main(void)
{
    const mulib_cdd_mcp_server_config_t base = { "demo", "1", NULL, 0, 0 };
    const mulib_cdd_mcp_catalog_config_t features = {0};
    const mulib_cdd_mcp_http_config_t config = {
        "localhost", "/mcp", NULL, NULL, allow_demo, NULL, 1
    };
    mulib_cdd_mcp_catalog_t server;
    mulib_cdd_mcp_client_t client;
    char *json;
    unsigned char *request, *response;
    size_t sent, received;
    mulib_json_t *result;
    assert(mulib_cdd_mcp_catalog_init(&server, &base, &features) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_init(&client, "demo", "1") == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_request(&client, MULIB_CDD_MCP_DISCOVER, NULL, NULL,
                                       &json) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_http_request("localhost", "/mcp", NULL, json, strlen(json),
                                            NULL, &request, &sent) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_server_http(&server, &config, request, sent,
                                    &response, &received) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_http_receive(&client, response, received, &result)
           == MULIB_CDD_MCP_OK);
    mulib_json_destroy(result);
    free(response);
    free(request);
    free(json);
    return 0;
}
