#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
CXX=${CXX:-c++}
CXXFLAGS=${CXXFLAGS-${CFLAGS:-}}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
warnings='-Wall -Wextra -Wpedantic -Werror -UNDEBUG'
$CC -std=c11 $warnings ${CFLAGS:-} -Iinclude tests/test_mcp.c ${LDFLAGS:-} -lm -o "$work/test"
"$work/test"
$CC -std=c11 $warnings ${CFLAGS:-} -Iinclude tests/test_mcp_http.c ${LDFLAGS:-} -lm -o "$work/test-http"
"$work/test-http"
python3 tests/test_mcp_http.py --fixture "$work/test-http"
for role in client server http; do
    $CC -std=c11 $warnings ${CFLAGS:-} -Iinclude "examples/modules/mcp-$role.c" \
        ${LDFLAGS:-} -lm -o "$work/$role"
    macro=$(printf '%s' "$role" | tr '[:lower:]' '[:upper:]')
    printf '#include "mulib-mcp-%s.h"\n#define MULIB_IMPL_CDD_MCP_%s\n#include "mulib-mcp-%s.h"\n#include "mulib-mcp-%s.h"\n' \
        "$role" "$macro" "$role" "$role" > "$work/$role.cpp"
    $CXX -std=c++17 $warnings $CXXFLAGS -Iinclude -c "$work/$role.cpp" -o "$work/$role.o"
done
"$work/http"
python3 tests/test_mcp_stdio.py --server "$work/server" --client "$work/client"
cat > "$work/deps.c" <<'EOF'
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_SSE
#include "mulib-json.h"
#include "mulib-base64.h"
#include "mulib-uri.h"
#include "mulib-http.h"
#include "mulib-sse.h"
EOF
cat > "$work/consumer.c" <<'EOF'
#include "mulib-mcp-server.h"
#include "mulib-mcp-client.h"
#include "mulib-mcp-http.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    const mulib_cdd_mcp_server_config_t cfg = { "server", "1", NULL, 0, 0 };
    mulib_cdd_mcp_server_t server;
    mulib_cdd_mcp_client_t client;
    char *req, *res;
    mulib_json_t *dom;
    assert(mulib_cdd_mcp_server_init(&server, &cfg) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_init(&client, "client", "1") == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_request(&client, MULIB_CDD_MCP_DISCOVER,
                                       NULL, NULL, &req) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_server_process(&server, req, strlen(req), &res) == MULIB_CDD_MCP_OK);
    assert(mulib_cdd_mcp_client_receive(&client, res, strlen(res), &dom) == MULIB_CDD_MCP_OK);
    mulib_json_destroy(dom);
    free(req);
    free(res);
    return 0;
}
EOF
$CC -std=c11 $warnings ${CFLAGS:-} -Iinclude -c "$work/deps.c" -o "$work/deps.o"
$CC -std=c11 $warnings ${CFLAGS:-} -Iinclude -c "$work/consumer.c" -o "$work/consumer.o"
$CXX $CXXFLAGS "$work/consumer.o" "$work/client.o" "$work/server.o" "$work/http.o" "$work/deps.o" \
    ${LDFLAGS:-} -lm -o "$work/mixed"
"$work/mixed"
echo 'MCP protocol, stdio, localhost HTTP/SSE, and C/C++ linkage tests passed.'
