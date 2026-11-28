/* Minimal MeekaMCP C API example and smoke test.
 *
 * Build (from the meekamcp module directory):
 *   go build -buildmode=c-shared -o libmeekamcp.so ./capi
 *   cc -I capi/include capi/example/example.c -L. -lmeekamcp -o meekamcp_example
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "meekamcp.h"

static char* dup_string(const char* src) {
    size_t len = strlen(src) + 1;
    char* out = (char*)malloc(len);
    if (out != NULL) {
        memcpy(out, src, len);
    }
    return out;
}

/* Echoes the argument object back as text. */
static char* echo_tool(const char* args_json, void* user_data) {
    (void)user_data;
    return dup_string(args_json);
}

/* Serves a fake config value for config://settings/{key}. */
static char* config_resource(const char* uri, const char* params_json,
                             void* user_data) {
    (void)uri;
    (void)user_data;
    return dup_string(params_json);
}

static int check(meekamcp_server_t srv, int ret, const char* what) {
    if (ret != 0) {
        char* err = meekamcp_last_error(srv);
        fprintf(stderr, "%s failed: %s\n", what, err ? err : "unknown");
        meekamcp_string_free(err);
        return 1;
    }
    return 0;
}

int main(void) {
    meekamcp_server_t srv = meekamcp_server_new("c-example", "1.0.0");
    if (srv == 0) {
        fprintf(stderr, "failed to create server\n");
        return 1;
    }

    if (check(srv,
              meekamcp_add_tool(srv, "echo", "Echoes its arguments", NULL,
                              echo_tool, NULL),
              "meekamcp_add_tool")) {
        return 1;
    }
    if (check(srv,
              meekamcp_add_resource(srv, "config://settings/{key}", "settings",
                                  "Configuration values", "application/json",
                                  config_resource, NULL),
              "meekamcp_add_resource")) {
        return 1;
    }

    static const char* frames[] = {
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}",
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}",
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\","
        "\"params\":{\"name\":\"echo\",\"arguments\":{\"msg\":\"hi\"}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"resources/read\","
        "\"params\":{\"uri\":\"config://settings/theme\"}}",
    };

    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        char* resp = meekamcp_handle_message(srv, frames[i]);
        if (resp == NULL) {
            char* err = meekamcp_last_error(srv);
            fprintf(stderr, "frame %zu failed: %s\n", i,
                    err ? err : "no response");
            meekamcp_string_free(err);
            meekamcp_server_free(srv);
            return 1;
        }
        printf("%s\n", resp);
        meekamcp_string_free(resp);
    }

    meekamcp_server_free(srv);
    return 0;
}
