/* C-only Ulysses boundary model. Only private connected Unix sockets, no
 * listener, external host, credentials, or production dispatch. */
#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_HTTP1
#define MULIB_IMPL_RATE_LIMIT
#include "mulib-http1.h"
#include "mulib-rate_limit.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void
send_all(int fd, const void *data, size_t len)
{
    const unsigned char *p = data;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n < 0 && errno == EINTR)
            continue;
        assert(n > 0 && (size_t)n <= len);
        p += n;
        len -= (size_t)n;
    }
    assert(shutdown(fd, SHUT_WR) == 0);
}

static size_t
read_all(int fd, unsigned char *data, size_t cap)
{
    size_t used = 0;
    for (;;) {
        assert(used < cap); /* All traffic originates in these fixed fixtures. */
        ssize_t n = read(fd, data + used, cap - used);
        if (n < 0 && errno == EINTR)
            continue;
        assert(n >= 0 && (size_t)n <= cap - used);
        if (n == 0)
            return used;
        used += (size_t)n;
    }
}

static void
respond(int fd, const unsigned char *input, size_t length,
        mulib_rate_limit_t *bucket, double now, unsigned *dispatches)
{
    const mulib_http_options_t limits = {256, 8, 16, MULIB_HTTP_EOF};
    mulib_http_message_t request = {0};
    size_t used = 0;
    unsigned status;
    mulib_http_result rc = mulib_http1_parse(input, length, 0, &limits, &request, &used);
    if (rc == MULIB_HTTP_NOMEM)
        status = 500;
    else if (rc == MULIB_HTTP_LIMIT)
        status = 413;
    else if (rc != MULIB_HTTP_OK || used != length)
        status = 400;
    else if (strcmp(request.target, "/echo"))
        status = 404;
    else if (strcmp(request.method, "POST"))
        status = 405;
    else if (!mulib_rate_limit_take(bucket, 1.0, now))
        status = 429;
    else {
        ++*dispatches;
        status = 200;
    }
    char *reason = status == 413 ? "Content Too Large" : status == 400 ? "Bad Request" :
                   status == 404 ? "Not Found" : status == 405 ? "Method Not Allowed" :
                   status == 429 ? "Too Many Requests" :
                   status == 500 ? "Internal Server Error" : "OK";
    mulib_http_header_t headers[2] = {{"Content-Type", "application/octet-stream"},
                                    {"Allow", "POST"}};
    mulib_http_message_t response = {.is_response = 1, .status = status, .reason = reason,
        .headers = headers, .header_count = status == 405 ? 2 : 1,
        .body = status == 200 ? request.body : NULL,
        .body_length = status == 200 ? request.body_length : 0};
    unsigned char *output = NULL;
    size_t size = 0;
    /* Encoder borrows; request owns the body until serialization completes. */
    assert(mulib_http1_serialize(&response, NULL, &output, &size) == MULIB_HTTP_OK);
    mulib_http_message_free(&request);
    send_all(fd, output, size);
    free(output);
}

static void
exchange(const char *request, size_t size, mulib_rate_limit_t *bucket,
         double now, unsigned *dispatches, unsigned status, unsigned expected_dispatches)
{
    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    send_all(sockets[0], request, size);
    unsigned char input[512], output[512];
    size_t in = read_all(sockets[1], input, sizeof input);
    respond(sockets[1], input, in, bucket, now, dispatches);
    size_t out = read_all(sockets[0], output, sizeof output);
    assert(close(sockets[0]) == 0);
    assert(close(sockets[1]) == 0);
    assert(*dispatches == expected_dispatches);

    char line[16];
    int n = snprintf(line, sizeof line, "HTTP/1.1 %u ", status);
    assert(n > 0 && (size_t)n < sizeof line && out >= (size_t)n);
    assert(memcmp(output, line, (size_t)n) == 0); /* Independent wire assertion. */
    mulib_http_message_t parsed = {0};
    size_t consumed = 0;
    assert(mulib_http1_parse(output, out, 1, NULL, &parsed, &consumed) == MULIB_HTTP_OK);
    assert(parsed.status == status && consumed == out);
    if (status == 200) {
        assert(parsed.body_length == 3 && !memcmp(parsed.body, "a\0b", 3));
        assert(out >= 3 && !memcmp(output + out - 3, "a\0b", 3));
    } else {
        assert(parsed.body_length == 0);
        if (status == 405) {
            const char *allow = mulib_http_header(&parsed, "allow");
            assert(allow && !strcmp(allow, "POST"));
        }
    }
    mulib_http_message_free(&parsed);
}

int
main(void)
{
    static const char ok[] = "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\na\0b";
    static const char missing[] = "GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n";
    static const char method[] = "GET /echo HTTP/1.1\r\nHost: localhost\r\n\r\n";
    static const char large[] = "POST /echo HTTP/1.1\r\nHost: localhost\r\n"
                                "Content-Length: 17\r\n\r\nabcdefghijklmnopq";
    mulib_rate_limit_t bucket;
    assert(mulib_rate_limit_init(&bucket, 2.0, 1.0, 10.0));
    unsigned dispatches = 0;
    exchange(method, sizeof method - 1, &bucket, 10.0, &dispatches, 405, 0);
    exchange(missing, sizeof missing - 1, &bucket, 10.0, &dispatches, 404, 0);
    exchange(ok, sizeof ok - 2, &bucket, 10.0, &dispatches, 400, 0); /* EOF before body complete. */
    exchange(large, sizeof large - 1, &bucket, 10.0, &dispatches, 413, 0);
    exchange(ok, sizeof ok - 1, &bucket, 10.0, &dispatches, 200, 1);
    exchange(ok, sizeof ok - 1, &bucket, 9.0, &dispatches, 429, 1); /* Clock reversal. */
    exchange(ok, sizeof ok - 1, &bucket, 10.25, &dispatches, 429, 1);
    exchange(ok, sizeof ok - 1, &bucket, 10.5, &dispatches, 200, 2);
    puts("PASS Ulysses C-only: private HTTP roundtrip, binary body, bounded failure and rate refill");
    return 0;
}
