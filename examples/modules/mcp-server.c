#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4
#define MULIB_IMPL_MCP_SERVER
#include "mulib-mcp-server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mulib_mcp_tool_status_t
greet(void *user, const mulib_json_t *args, char *text, size_t cap, size_t *len)
{
    int has_args = mulib_json_size(args) != 0;
    int n;
    (void)user;
    n = snprintf(text, cap, "%s", has_args ? "Expected no arguments" : "Hello");
    if (n < 0 || (size_t)n >= cap)
        return MULIB_MCP_TOOL_FAILED;
    *len = (size_t)n;
    return has_args ? MULIB_MCP_TOOL_ERROR : MULIB_MCP_TOOL_OK;
}

static mulib_mcp_handler_status_t
read_greeting(void *user, const mulib_mcp_request_view_t *request, mulib_json_t **out)
{
    const char result[] = "{\"contents\":[{\"uri\":\"demo:/greeting\","
                          "\"mimeType\":\"text/plain\",\"text\":\"Hello\"}]}";
    (void)user;
    if (strcmp(request->target, "demo:/greeting") || request->request_state)
        return MULIB_MCP_HANDLER_INVALID;
    *out = mulib_json_parse(result, sizeof result - 1, NULL);
    return *out ? MULIB_MCP_HANDLER_OK : MULIB_MCP_HANDLER_FAILED;
}

int main(void)
{
    static char line[MULIB_MCP_SERVER_MAX_FRAME];
    const char schema_text[] = "{\"type\":\"object\",\"additionalProperties\":false}";
    mulib_json_t *schema = mulib_json_parse(schema_text, sizeof schema_text - 1, NULL);
    const char inventory[] = "[{\"name\":\"Greeting\",\"uri\":\"demo:/greeting\",\"mimeType\":\"text/plain\"}]";
    mulib_json_t *resources = mulib_json_parse(inventory, sizeof inventory - 1, NULL);
    const mulib_mcp_tool_t tool = { "greet", "Return a fixed greeting", schema, greet, NULL };
    const mulib_mcp_server_config_t cfg = { "greeting-demo", "1", &tool, 1, 1 };
    const mulib_mcp_catalog_config_t features = {
        .resources = resources, .read = read_greeting
    };
    mulib_mcp_catalog_t server;
    size_t n = 0;
    int ch, ok = schema && resources
                 && mulib_mcp_catalog_init(&server, &cfg, &features) == MULIB_MCP_SERVER_OK;
    while (ok && (ch = getchar()) != EOF) {
        if (ch == '\n') {
            char *out;
            if (n && line[n - 1] == '\r')
                --n;
            ok = mulib_mcp_catalog_process(&server, NULL, line, n, &out) == MULIB_MCP_SERVER_OK;
            if (ok && out) {
                ok = puts(out) != EOF && fflush(stdout) == 0;
                free(out);
            }
            n = 0;
        } else if (n < sizeof line) {
            line[n++] = (char)ch;
        } else {
            ok = 0;
        }
    }
    mulib_json_destroy(resources);
    mulib_json_destroy(schema);
    return !ok || n != 0 || ferror(stdin);
}
