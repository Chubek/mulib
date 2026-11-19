#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4
#define MULIB_IMPL_CDD_MCP_CLIENT
#include "mulib-mcp-client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Print a discovery request, or a direct resource read, for mcp-server. */
int main(int argc, char **argv)
{
    mulib_cdd_mcp_client_t client;
    const mulib_cdd_mcp_request_options_t direct = { NULL, NULL, 1 };
    const int read = argc == 2 && !strcmp(argv[1], "--read");
    char *frame;
    int ok;
    if ((!read && argc != 1)
        || mulib_cdd_mcp_client_init(&client, "probe-demo", "1") != MULIB_CDD_MCP_OK
        || mulib_cdd_mcp_client_request_ex(&client,
              read ? MULIB_CDD_MCP_RESOURCES_READ : MULIB_CDD_MCP_DISCOVER,
              read ? "demo:/greeting" : NULL, NULL, read ? &direct : NULL,
              &frame) != MULIB_CDD_MCP_OK)
        return 1;
    ok = puts(frame) != EOF && fflush(stdout) == 0;
    free(frame);
    return !ok;
}
