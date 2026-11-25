#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_HTML
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_XML
#define MULIB_IMPL_XPATH
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_FETCH_POLICY
#define MULIB_IMPL_ELVI_SCRIPT_LOGIC
#include "mulib-elvi-script-logic.h"
#include <assert.h>
#include <stdio.h>

static const char fixture[] =
    "<!doctype html><HTML><body><section>"
    "<article data-testid=property-card><h3 data-testid=title>"
    "  Cedar &amp; <b>Stone</b><br> Inn </h3>"
    "<a data-testid=title-link href='https://example.org/h?a=1&amp;b=2'>Details</a>"
    "<span data-testid=price-and-discounted-price> $125 </span>"
    "<span data-testid=address>North&nbsp;Shore</span></article>"
    "<article data-testid=property-card><h3 data-testid=title>Lake View</h3>"
    "<a data-testid=title-link href=https://example.org/lake>Details</a></article>"
    "<article data-testid=property-card><h3 data-testid=title>Missing link</h3></article>"
    "<a data-testid=pagination-next href='https://example.org/page/2'>next</a>"
    "</section></body></HTML>";

typedef struct capture {
    size_t requests, results;
    int reject_request, reject_result;
    mulib_elvi_script_request_t last_request;
    mulib_elvi_script_result_t entries[2];
} capture_t;

static int on_enqueue(void *user, const mulib_elvi_script_request_t *request)
{
    capture_t *capture = user;
    if (capture->reject_request) return 0;
    capture->last_request = *request;
    ++capture->requests;
    return 1;
}

static int on_emit(void *user, const mulib_elvi_script_result_t *result)
{
    capture_t *capture = user;
    if (capture->reject_result) return 0;
    if (capture->results < 2) capture->entries[capture->results] = *result;
    ++capture->results;
    return 1;
}

static mulib_elvi_script_logic_t *create(const char *source, capture_t *capture)
{
    return mulib_elvi_script_logic_create(source, strlen(source), on_enqueue, on_emit, capture);
}

static int response(mulib_elvi_script_logic_t *logic, const char *html, size_t length)
{
    mulib_elvi_script_response_t input = {0};
    input.message.is_response = 1;
    input.message.status = 200;
    input.message.body = (unsigned char *)html;
    input.message.body_length = length;
    strcpy(input.effective_url, "https://example.org/fixture");
    return mulib_elvi_script_logic_response(logic, &input);
}

static void expect_hook_failure(const char *source, const char *html)
{
    capture_t capture = {0};
    mulib_elvi_script_logic_t *logic = create(source, &capture);
    assert(logic);
    assert(!response(logic, html, strlen(html)));
    assert(!response(logic, fixture, sizeof fixture - 1));
    assert(!mulib_elvi_script_logic_finish(logic));
    mulib_elvi_script_logic_destroy(logic);
}

static void canonical(const char *path)
{
    FILE *file = fopen(path, "rb");
    char code[MULIB_ELVI_SCRIPT_LOGIC_MAX_SOURCE];
    size_t length;
    capture_t capture = {0};
    mulib_elvi_script_logic_t *logic;
    assert(file);
    length = fread(code, 1, sizeof code, file);
    assert(feof(file) && !ferror(file) && fclose(file) == 0);
    logic = mulib_elvi_script_logic_create(code, length, on_enqueue, on_emit, &capture);
    assert(logic);
    assert(response(logic, fixture, sizeof fixture - 1));
    assert(capture.requests == 1 && capture.results == 2);
    assert(!strcmp(capture.last_request.url, "https://example.org/page/2"));
    assert(!strcmp(capture.entries[0].name, "Cedar & Stone Inn"));
    assert(!strcmp(capture.entries[0].url, "https://example.org/h?a=1&b=2"));
    assert(!strcmp(capture.entries[0].price, "$125"));
    assert(!strcmp(capture.entries[0].location, "North\xc2\xa0Shore"));
    assert(!strcmp(capture.entries[1].name, "Lake View"));
    assert(!capture.entries[1].price[0] && !capture.entries[1].location[0]);
    assert(mulib_elvi_script_logic_finish(logic));
    assert(!mulib_elvi_script_logic_finish(logic));
    assert(!response(logic, fixture, sizeof fixture - 1));
    mulib_elvi_script_logic_destroy(logic);
}

static void lifecycle(void)
{
    capture_t capture = {0};
    mulib_elvi_script_logic_t *logic = create(
        "local started, seen\n"
        "function on_start() started = true; enqueue('https://example.org/start') end\n"
        "function on_response(doc, url)\n"
        "  if not started or url ~= 'https://example.org/fixture' then missing() end\n"
        "  if #xpath(doc, '/html/body/section/article') ~= 3 then missing() end\n"
        "  seen = true; on_finish = nil; on_response = nil\n"
        "end\n"
        "function on_finish() if not seen then missing() end end\n", &capture);
    assert(logic && capture.requests == 1);
    assert(response(logic, fixture, sizeof fixture - 1));
    assert(response(logic, fixture, sizeof fixture - 1));
    assert(mulib_elvi_script_logic_finish(logic));
    mulib_elvi_script_logic_destroy(logic);

    logic = create("local saved\n"
        "function on_response(doc)"
        "  if saved then text(saved) else saved = xpath(doc, '/html')[1] end\n"
        "end", &capture);
    assert(logic && response(logic, fixture, sizeof fixture - 1));
    assert(!response(logic, fixture, sizeof fixture - 1));
    mulib_elvi_script_logic_destroy(logic);

    logic = create("local saved\nfunction on_response(doc) saved=doc end\n"
        "function on_finish() xpath(saved, '.') end", &capture);
    assert(logic && response(logic, fixture, sizeof fixture - 1));
    assert(!mulib_elvi_script_logic_finish(logic));
    assert(!response(logic, fixture, sizeof fixture - 1));
    mulib_elvi_script_logic_destroy(logic);
}

static void capabilities(void)
{
    capture_t capture = {0};
    mulib_elvi_script_logic_t *logic;
    logic = create(
        "if _G or io or os or package or debug or coroutine or math or string or table"
        " or utf8 or require or load or loadfile or dofile or setmetatable or getmetatable"
        " or pcall or xpcall or collectgarbage or print or warn or assert or next or pairs"
        " or ipairs or rawget or rawset then unavailable() end\n"
        "function on_response(d) if not(xpath and text and attr and enqueue and emit) then"
        " unavailable() end end", &capture);
    assert(logic && response(logic, fixture, sizeof fixture - 1));
    mulib_elvi_script_logic_destroy(logic);
    expect_hook_failure("function on_response(d) return ('str'):dump() end", fixture);
    expect_hook_failure("function on_response(d) text({1,1}) end", fixture);
    expect_hook_failure("function on_response(d) d.fake=1 end", fixture);
    expect_hook_failure("function on_response(d) return _ENV['io'].open('/invalid') end", fixture);
    assert(!create("function on_response() end; enqueue('https://example.org/')", &capture));
    assert(!create("function on_response() end; function on_start() emit('x','https://example.org/') end",
                   &capture));
    assert(!create("function on_response() end; on_finish={}", &capture));
    assert(!create("function on_start() end", &capture));
    assert(!capture.requests && !capture.results);

    capture.reject_request = 1;
    assert(!create("function on_response() end; function on_start() enqueue('https://example.org/') end",
                   &capture));
    capture.reject_result = 1;
    logic = create("function on_response() emit('x','https://example.org/') end", &capture);
    assert(logic && !response(logic, fixture, sizeof fixture - 1));
    assert(!capture.requests && !capture.results);
    mulib_elvi_script_logic_destroy(logic);
}

static void boundaries(void)
{
    capture_t capture = {0};
    mulib_elvi_script_logic_t *logic;
    mulib_elvi_script_response_t invalid = {0};
    char html[8192];
    const char *empty_hook = "function on_response() end";
    char *p = html;
    assert(!create("while true do end", &capture));
    assert(!create("repeat until false", &capture));
    assert(!mulib_elvi_script_logic_create(empty_hook, MULIB_ELVI_SCRIPT_LOGIC_MAX_SOURCE + 1,
                                          on_enqueue, on_emit, &capture));
    assert(!mulib_elvi_script_logic_create(empty_hook, strlen(empty_hook) + 1,
                                          on_enqueue, on_emit, &capture));
    expect_hook_failure("function on_response(d) local function r() return 1+r() end r() end", fixture);
    expect_hook_failure("function on_response(d) local a='x' for i=1,30 do a=a..a end end", fixture);
    expect_hook_failure("function on_response(d) xpath(d,'//a//b') end", fixture);
    expect_hook_failure("function on_response(d) xpath(d,'/html/body/..') end", fixture);
    expect_hook_failure("function on_response(d) xpath(d,'././././././././.') end", fixture);
    expect_hook_failure("function on_response(d) xpath(d,'//*[') end", fixture);
    expect_hook_failure("function on_response(d) enqueue('http://example.org/') end", fixture);
    expect_hook_failure("function on_response(d) enqueue('https://localhost/') end", fixture);
    expect_hook_failure("function on_response(d) enqueue('https://example.org/\\0a') end", fixture);
    expect_hook_failure("function on_response(d) emit(123,'https://example.org/') end", fixture);
    expect_hook_failure("function on_response(d) emit('\\255','https://example.org/') end", fixture);
    expect_hook_failure("function on_response(d) emit('','https://example.org/') end", fixture);
    expect_hook_failure(empty_hook, "<html><body><script>noop</script></body></html>");
    expect_hook_failure(empty_hook, "<html><body><b></body></html>");
    expect_hook_failure(empty_hook, "<html><body>\xff</body></html>");
    logic = create(empty_hook, &capture);
    invalid.message.is_response = 1;
    invalid.message.status = 200;
    invalid.message.body = (unsigned char *)fixture;
    invalid.message.body_length = sizeof fixture - 1;
    memset(invalid.effective_url, 'a', sizeof invalid.effective_url);
    assert(logic && !mulib_elvi_script_logic_response(logic, &invalid));
    assert(!response(logic, fixture, sizeof fixture - 1));
    mulib_elvi_script_logic_destroy(logic);

    p += sprintf(p, "<html><body>");
    for (int i = 0; i < 65; ++i) p += sprintf(p, "<b></b>");
    sprintf(p, "</body></html>");
    expect_hook_failure("function on_response(d) xpath(d,'//b') end", html);
    p = html + sprintf(html, "<html><body>");
    for (int i = 0; i < 500; ++i) p += sprintf(p, "<br>");
    sprintf(p, "</body></html>");
    logic = create("function on_response(d) xpath(d,'//missing') end", &capture);
    assert(logic && response(logic, html, strlen(html)));
    mulib_elvi_script_logic_destroy(logic);
    expect_hook_failure("function on_response(d) for i=1,3 do xpath(d,'//missing') end end", html);
    p = html + sprintf(html, "<html><body>");
    for (int i = 0; i < 512; ++i) p += sprintf(p, "<br>");
    sprintf(p, "</body></html>");
    expect_hook_failure(empty_hook, html);
    expect_hook_failure("function on_response(d) for i=1,513 do xpath(d,'.') end end", fixture);
    expect_hook_failure("function on_response(d) for i=1,1025 do attr(d,'name') end end", fixture);
    logic = create("function on_response() for i=1,65 do emit('x','https://example.org/') end end",
                   &capture);
    assert(logic && !response(logic, fixture, sizeof fixture - 1));
    assert(capture.results == MULIB_ELVI_SCRIPT_MAX_RESULTS);
    mulib_elvi_script_logic_destroy(logic);
    logic = create("function on_response() for i=1,9 do enqueue('https://example.org/') end end",
                   &capture);
    assert(logic && !response(logic, fixture, sizeof fixture - 1));
    assert(capture.requests == MULIB_ELVI_SCRIPT_MAX_REQUESTS);
    mulib_elvi_script_logic_destroy(logic);

    p = html + sprintf(html, "<html><body>");
    memset(p, 'a', 4097);
    strcpy(p + 4097, "</body></html>");
    expect_hook_failure("function on_response(d) text(d) end", html);
    logic = create(empty_hook, &capture);
    assert(logic && !response(logic, html, MULIB_ELVI_SCRIPT_MAX_BODY + 1));
    mulib_elvi_script_logic_destroy(logic);

    p = html + sprintf(html, "<html><body><b value='");
    memset(p, 'a', 4097);
    strcpy(p + 4097, "'></b></body></html>");
    expect_hook_failure(empty_hook, html);
    p = html + sprintf(html, "<html><body>");
    for (int i = 0; i < 40; ++i) p += sprintf(p, "<b>");
    for (int i = 0; i < 40; ++i) p += sprintf(p, "</b>");
    sprintf(p, "</body></html>");
    expect_hook_failure(empty_hook, html);
}

int main(int argc, char **argv)
{
    canonical(argc > 1 ? argv[1] : "library/elvis/logic/BookingDotCom.lua");
    lifecycle();
    capabilities();
    boundaries();
    puts("ElviScript logic: offline fixture, hooks, capabilities and bounds passed");
    return 0;
}
