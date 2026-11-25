/** @file mulib-elvi-script-logic.h
 * @brief Bounded HTML/XPath and capability-only Mune logic for ElviScript.
 */
#ifndef MULIB_ELVI_SCRIPT_LOGIC_H
#define MULIB_ELVI_SCRIPT_LOGIC_H
#include "detail/mulib-elvi-script-types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_ELVI_SCRIPT_LOGIC_MAX_SOURCE (16u * 1024u)

typedef struct mulib_elvi_script_logic mulib_elvi_script_logic_t;

/** Run authorized Lua source and optional on_start(). Requires on_response.
 * Enqueue and emit callbacks are mandatory, synchronous and non-reentrant. */
mulib_elvi_script_logic_t *mulib_elvi_script_logic_create(const char *source,
    size_t length, mulib_elvi_script_enqueue_fn enqueue,
    mulib_elvi_script_emit_fn emit, void *user);
/** Parse a borrowed successful HTML response and call on_response(document,url).
 * Returns 1 on success; a stage error permanently fails this instance. */
int mulib_elvi_script_logic_response(mulib_elvi_script_logic_t *logic,
    const mulib_elvi_script_response_t *response);
/** Call optional on_finish() once after the host has drained its queue.
 * No capability calls are valid during this hook. Returns 1 on success. */
int mulib_elvi_script_logic_finish(mulib_elvi_script_logic_t *logic);
/** Release the stage without running any hooks. NULL is harmless. */
void mulib_elvi_script_logic_destroy(mulib_elvi_script_logic_t *logic);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ELVI_SCRIPT_LOGIC_H */

#if defined(MULIB_IMPL_ELVI_SCRIPT_LOGIC) && !defined(MULIB_ELVI_SCRIPT_LOGIC_IMPLEMENTED)
#define MULIB_ELVI_SCRIPT_LOGIC_IMPLEMENTED
#include "mulib-fetch-policy.h"
#include "mulib-html.h"
#include "mulib-html-textstream.h"
#include "mulib-mune.h"
#include "mulib-xpath.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    MULIB_ELVI_LOGIC_HTML_EVENTS_ = 1024,
    MULIB_ELVI_LOGIC_HTML_NODES_ = 512,
    MULIB_ELVI_LOGIC_HTML_DEPTH_ = 32,
    MULIB_ELVI_LOGIC_MAX_TAG_ = 4096,
    MULIB_ELVI_LOGIC_MAX_TEXT_ = 4096,
    MULIB_ELVI_LOGIC_MAX_QUERY_ = 256,
    MULIB_ELVI_LOGIC_MAX_STEPS_ = 8,
    MULIB_ELVI_LOGIC_MAX_MATCHES_ = 64,
    MULIB_ELVI_LOGIC_MAX_HANDLES_ = 512,
    MULIB_ELVI_LOGIC_MAX_NATIVE_ = 1024
};

typedef enum mulib_elvi_logic_phase_ {
    MULIB_ELVI_LOGIC_INIT_, MULIB_ELVI_LOGIC_START_, MULIB_ELVI_LOGIC_IDLE_,
    MULIB_ELVI_LOGIC_RESPONSE_, MULIB_ELVI_LOGIC_FINISH_, MULIB_ELVI_LOGIC_DEAD_
} mulib_elvi_logic_phase_;

struct mulib_elvi_script_logic {
    mulib_mune_state_t *state;
    mulib_mune_result_t *hooks;
    mulib_elvi_script_enqueue_fn enqueue;
    mulib_elvi_script_emit_fn emit;
    void *user;
    mulib_elvi_logic_phase_ phase;
    size_t responses, requests, emitted, handles, calls_left, inspect_left;
    size_t body_length, depth;
    uint64_t xpath_left;
    mulib_xml_node_t *nodes[MULIB_ELVI_LOGIC_MAX_HANDLES_];
};

/* No document address is stored in a Lua value. The per-response slot table
 * is valid only during on_response; a retained token never revives. */
typedef struct mulib_elvi_logic_token_ {
    size_t generation, slot;
} mulib_elvi_logic_token_;

static int
mulib_elvi_logic_space_(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static int
mulib_elvi_logic_html_event_(void *user, const mulib_html_textstream_event_t *event)
{
    (void)user;
    return event->kind != MULIB_HTML_STREAM_START
        || event->len <= MULIB_ELVI_LOGIC_MAX_TAG_;
}

/* Advance in preorder without ever leaving root's subtree. */
static mulib_xml_node_t *
mulib_elvi_logic_next_(mulib_xml_node_t *root, mulib_xml_node_t *node, size_t *depth)
{
    mulib_xml_node_t *child = mulib_xml_first(node);
    if (child) {
        if (depth) ++*depth;
        return child;
    }
    while (node != root) {
        mulib_xml_node_t *sibling = mulib_xml_next(node);
        if (sibling) return sibling;
        node = mulib_xml_parent(node);
        if (depth) --*depth;
    }
    return NULL;
}

static int
mulib_elvi_logic_shape_(mulib_xml_node_t *root, size_t *count,
                       size_t *depth, size_t *width)
{
    size_t level = 0;
    *count = *depth = *width = 0;
    for (mulib_xml_node_t *node = root; node;
         node = mulib_elvi_logic_next_(root, node, &level)) {
        size_t siblings = 0;
        if (++*count > MULIB_ELVI_LOGIC_HTML_NODES_
            || level > MULIB_ELVI_LOGIC_HTML_DEPTH_) return 0;
        if (level > *depth) *depth = level;
        for (mulib_xml_node_t *child = mulib_xml_first(node); child;
             child = mulib_xml_next(child)) ++siblings;
        if (siblings > *width) *width = siblings;
    }
    return 1;
}

static int
mulib_elvi_logic_take_call_(mulib_elvi_script_logic_t *logic)
{
    if (!logic->calls_left) return 0;
    --logic->calls_left;
    return 1;
}

static mulib_xml_node_t *
mulib_elvi_logic_get_node_(mulib_elvi_script_logic_t *logic, mulib_mune_value_t value)
{
    size_t size = 0;
    const mulib_elvi_logic_token_ *token;
    if (logic->phase != MULIB_ELVI_LOGIC_RESPONSE_) return NULL;
    token = (const mulib_elvi_logic_token_ *)mulib_mune_value_userdata(value, &size);
    if (!token || size != sizeof (*token) || token->generation != logic->responses
        || token->slot >= logic->handles) return NULL;
    return logic->nodes[token->slot];
}

static int
mulib_elvi_logic_make_node_(mulib_elvi_script_logic_t *logic, mulib_xml_node_t *node,
                           mulib_mune_value_t *value)
{
    void *data = NULL;
    mulib_elvi_logic_token_ *token;
    if (logic->handles == MULIB_ELVI_LOGIC_MAX_HANDLES_
        || mulib_mune_make_userdata(logic->state, sizeof (*token), value, &data)
           != MULIB_MUNE_OK) return 0;
    token = (mulib_elvi_logic_token_ *)data;
    token->generation = logic->responses;
    token->slot = logic->handles;
    logic->nodes[logic->handles++] = node;
    return 1;
}

static const char *
mulib_elvi_logic_string_(mulib_mune_value_t value, size_t capacity, size_t *length)
{
    const char *s = mulib_mune_value_string(value, length);
    if (!s || *length >= capacity || !mulib_xml_valid_text(s, *length)) return NULL;
    return s;
}

static mulib_mune_status_t
mulib_elvi_logic_push_string_(mulib_mune_state_t *state, mulib_mune_result_t *out,
                              const char *s, size_t length)
{
    mulib_mune_value_t value = mulib_mune_nil();
    if (s && mulib_mune_make_string(state, s, length, &value) != MULIB_MUNE_OK)
        return MULIB_MUNE_MEMORY;
    return mulib_mune_result_push(out, value) ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}

/* Prohibit ancestor navigation and repeated descendant axes, which otherwise
 * allow many duplicate visits to overlapping subtrees in mulib_xpath_query.
 * This is an admission check, not an XPath evaluator; the actual grammar,
 * predicate handling and document ordering remain mulib-xpath's job. */
static size_t
mulib_elvi_logic_query_steps_(const char *query)
{
    size_t steps = 1, i = 0, begin, axis;
    char quote = 0;
    int predicate = 0, at_step = 1;
    while (mulib_elvi_logic_space_(query[i])) ++i;
    begin = i;
    if (query[i] == '/') {
        ++i;
        if (query[i] == '/') ++i;
    }
    for (; query[i]; ++i) {
        char c = query[i];
        if (quote) {
            if (c == quote) quote = 0;
        } else if (predicate) {
            if (c == '\'' || c == '"') quote = c;
            else if (c == ']') predicate = 0;
        } else if (c == '/') {
            if (++steps > MULIB_ELVI_LOGIC_MAX_STEPS_) return 0;
            if (query[i + 1] == '/') {
                axis = begin;
                if (query[axis] != '.') return 0;
                ++axis;
                while (mulib_elvi_logic_space_(query[axis])) ++axis;
                if (axis != i) return 0;
                ++i;
            }
            at_step = 1;
        } else if (!mulib_elvi_logic_space_(c)) {
            if (at_step && c == '.' && query[i + 1] == '.') return 0;
            at_step = 0;
            if (c == '[') predicate = 1;
        }
    }
    return steps;
}

static mulib_mune_status_t
mulib_elvi_logic_xpath_(mulib_mune_state_t *state, const mulib_mune_value_t *args,
    size_t argc, mulib_mune_result_t *out, void *user)
{
    mulib_elvi_script_logic_t *logic = (mulib_elvi_script_logic_t *)user;
    mulib_xml_node_t *node, *scope;
    mulib_xpath_result_t matches = {NULL, 0};
    mulib_mune_value_t list, handle;
    const char *query, *first;
    size_t length, steps, nodes, depth, width;
    uint64_t work;
    mulib_mune_status_t status = MULIB_MUNE_LIMIT;
    if (argc != 2 || !mulib_elvi_logic_take_call_(logic)
        || !(node = mulib_elvi_logic_get_node_(logic, args[0]))
        || !(query = mulib_elvi_logic_string_(args[1],
                        MULIB_ELVI_LOGIC_MAX_QUERY_ + 1, &length))
        || !(steps = mulib_elvi_logic_query_steps_(query))) return MULIB_MUNE_RUNTIME;
    first = query;
    while (mulib_elvi_logic_space_(*first)) ++first;
    scope = *first == '/' ? logic->nodes[0] : node;
    if (!mulib_elvi_logic_shape_(scope, &nodes, &depth, &width)) return MULIB_MUNE_LIMIT;
    /* Conservative cumulative charge for XPath insertion ordering, predicate
     * string scans, and visits. Each non-descendant step works on disjoint
     * child lists (or on self); only one step can scan descendants. */
    work = (uint64_t)steps * ((uint64_t)nodes * nodes * (4 * logic->depth + width + 8)
        + 8 * (uint64_t)logic->body_length + 32 * (uint64_t)nodes);
    if (work > logic->xpath_left) return MULIB_MUNE_LIMIT;
    logic->xpath_left -= work;
    if (!mulib_xpath_query(node, query, &matches, NULL)) return MULIB_MUNE_RUNTIME;
    if (matches.count > MULIB_ELVI_LOGIC_MAX_MATCHES_
        || matches.count > MULIB_ELVI_LOGIC_MAX_HANDLES_ - logic->handles) goto done;
    if (mulib_mune_make_table(state, &list) != MULIB_MUNE_OK
        || !mulib_mune_result_push(out, list)) goto done;
    for (size_t i = 0; i < matches.count; ++i)
        if (!mulib_elvi_logic_make_node_(logic, matches.nodes[i], &handle)
            || mulib_mune_raw_set(state, list, mulib_mune_integer((int64_t)i + 1),
                                handle) != MULIB_MUNE_OK) goto done;
    status = MULIB_MUNE_OK;
done:
    mulib_xpath_result_free(&matches);
    return status;
}

static mulib_mune_status_t
mulib_elvi_logic_text_(mulib_mune_state_t *state, const mulib_mune_value_t *args,
    size_t argc, mulib_mune_result_t *out, void *user)
{
    mulib_elvi_script_logic_t *logic = (mulib_elvi_script_logic_t *)user;
    mulib_xml_node_t *root;
    char text[MULIB_ELVI_LOGIC_MAX_TEXT_];
    size_t length = 0;
    int space = 0;
    if (argc != 1 || !mulib_elvi_logic_take_call_(logic)
        || !(root = mulib_elvi_logic_get_node_(logic, args[0]))) return MULIB_MUNE_RUNTIME;
    for (mulib_xml_node_t *node = root; node; node = mulib_elvi_logic_next_(root, node, NULL)) {
        const char *s = NULL, *name = mulib_xml_name(node);
        mulib_xml_type_t type = mulib_xml_type(node);
        if (!logic->inspect_left) return MULIB_MUNE_LIMIT;
        --logic->inspect_left;
        if (type == MULIB_XML_TEXT || type == MULIB_XML_CDATA) s = mulib_xml_value(node);
        else if (name && !strcmp(name, "br")) s = " ";
        if (!s) continue;
        for (; *s; ++s) {
            if (!logic->inspect_left) return MULIB_MUNE_LIMIT;
            --logic->inspect_left;
            if (mulib_elvi_logic_space_(*s)) {
                space = length != 0;
                continue;
            }
            if (length + (size_t)space >= sizeof text) return MULIB_MUNE_LIMIT;
            if (space) text[length++] = ' ';
            text[length++] = *s;
            space = 0;
        }
    }
    return mulib_elvi_logic_push_string_(state, out, text, length);
}

static mulib_mune_status_t
mulib_elvi_logic_attr_(mulib_mune_state_t *state, const mulib_mune_value_t *args,
    size_t argc, mulib_mune_result_t *out, void *user)
{
    mulib_elvi_script_logic_t *logic = (mulib_elvi_script_logic_t *)user;
    mulib_xml_node_t *node;
    const char *key, *value;
    size_t length;
    if (argc != 2 || !mulib_elvi_logic_take_call_(logic)
        || !(node = mulib_elvi_logic_get_node_(logic, args[0]))
        || !(key = mulib_elvi_logic_string_(args[1], 128, &length))
        || !mulib_xml_valid_name(key)) return MULIB_MUNE_RUNTIME;
    value = mulib_xml_attr(node, key);
    return mulib_elvi_logic_push_string_(state, out, value, value ? strlen(value) : 0);
}

static int
mulib_elvi_logic_url_(const char *url)
{
    const mulib_fetch_policy_t policy = {8191, 0, 0, 1, 0};
    if (strncmp(url, "https://", 8) != 0) return 0;
    for (const unsigned char *c = (const unsigned char *)url; *c; ++c)
        if (*c <= 0x20 || *c >= 0x7f || *c == '\\') return 0;
    return mulib_fetch_policy_check(&policy, url);
}

static int
mulib_elvi_logic_copy_(char *dest, size_t capacity, mulib_mune_value_t value,
                      int required)
{
    size_t length;
    const char *s;
    if (!required && value.type == MULIB_MUNE_NIL) return 1;
    s = mulib_elvi_logic_string_(value, capacity, &length);
    if (!s || (required && !length)) return 0;
    memcpy(dest, s, length + 1);
    return 1;
}

static mulib_mune_status_t
mulib_elvi_logic_enqueue_(mulib_mune_state_t *state, const mulib_mune_value_t *args,
    size_t argc, mulib_mune_result_t *out, void *user)
{
    mulib_elvi_script_logic_t *logic = (mulib_elvi_script_logic_t *)user;
    mulib_elvi_script_request_t request = {""};
    (void)state;
    (void)out;
    if ((logic->phase != MULIB_ELVI_LOGIC_START_ && logic->phase != MULIB_ELVI_LOGIC_RESPONSE_)
        || !mulib_elvi_logic_take_call_(logic) || argc != 1
        || logic->requests == MULIB_ELVI_SCRIPT_MAX_REQUESTS
        || !mulib_elvi_logic_copy_(request.url, sizeof request.url, args[0], 1)
        || !mulib_elvi_logic_url_(request.url)
        || !logic->enqueue(logic->user, &request)) return MULIB_MUNE_RUNTIME;
    ++logic->requests;
    return MULIB_MUNE_OK;
}

static mulib_mune_status_t
mulib_elvi_logic_emit_(mulib_mune_state_t *state, const mulib_mune_value_t *args,
    size_t argc, mulib_mune_result_t *out, void *user)
{
    mulib_elvi_script_logic_t *logic = (mulib_elvi_script_logic_t *)user;
    mulib_elvi_script_result_t result = {"", "", "", ""};
    (void)state;
    (void)out;
    if (logic->phase != MULIB_ELVI_LOGIC_RESPONSE_ || !mulib_elvi_logic_take_call_(logic)
        || argc < 2 || argc > 4 || logic->emitted == MULIB_ELVI_SCRIPT_MAX_RESULTS
        || !mulib_elvi_logic_copy_(result.name, sizeof result.name, args[0], 1)
        || !mulib_elvi_logic_copy_(result.url, sizeof result.url, args[1], 1)
        || !mulib_elvi_logic_url_(result.url)
        || (argc >= 3 && !mulib_elvi_logic_copy_(result.price, sizeof result.price, args[2], 0))
        || (argc == 4 && !mulib_elvi_logic_copy_(result.location, sizeof result.location, args[3], 0))
        || !logic->emit(logic->user, &result)) return MULIB_MUNE_RUNTIME;
    ++logic->emitted;
    return MULIB_MUNE_OK;
}

static int
mulib_elvi_logic_hook_(mulib_elvi_script_logic_t *logic, size_t which,
                       const mulib_mune_value_t *args, size_t count)
{
    mulib_mune_value_t fn = mulib_mune_result_get(logic->hooks, which);
    mulib_mune_result_t *result = NULL;
    mulib_mune_status_t status;
    if (fn.type == MULIB_MUNE_NIL) return 1;
    logic->calls_left = MULIB_ELVI_LOGIC_MAX_NATIVE_;
    status = mulib_mune_call(logic->state, fn, args, count, &result, NULL);
    mulib_mune_result_destroy(result);
    return status == MULIB_MUNE_OK;
}

mulib_elvi_script_logic_t *
mulib_elvi_script_logic_create(const char *source, size_t length,
    mulib_elvi_script_enqueue_fn enqueue, mulib_elvi_script_emit_fn emit, void *user)
{
    static const char *const names[] = {"on_start", "on_response", "on_finish"};
    static const mulib_mune_runtime_options_t runtime = {2u * 1024u * 1024u, 200000, 32, 256};
    static const mulib_mune_parse_options_t frontend = {
        MULIB_ELVI_SCRIPT_LOGIC_MAX_SOURCE, 4096, 4096, 4096, 1000000
    };
    mulib_elvi_script_logic_t *logic;
    mulib_mune_chunk_t *chunk;
    mulib_mune_result_t *result = NULL;
    if (!source || !length || length > MULIB_ELVI_SCRIPT_LOGIC_MAX_SOURCE
        || memchr(source, 0, length) || !enqueue || !emit) return NULL;
    chunk = mulib_mune_parse(source, length, &frontend, NULL);
    if (!chunk) return NULL;
    logic = (mulib_elvi_script_logic_t *)calloc(1, sizeof (*logic));
    if (!logic) {
        mulib_mune_chunk_destroy(chunk);
        return NULL;
    }
    logic->state = mulib_mune_state_create_bare(&runtime);
    logic->enqueue = enqueue;
    logic->emit = emit;
    logic->user = user;
    if (!logic->state
        || mulib_mune_bind(logic->state, "xpath", mulib_elvi_logic_xpath_, logic) != MULIB_MUNE_OK
        || mulib_mune_bind(logic->state, "text", mulib_elvi_logic_text_, logic) != MULIB_MUNE_OK
        || mulib_mune_bind(logic->state, "attr", mulib_elvi_logic_attr_, logic) != MULIB_MUNE_OK
        || mulib_mune_bind(logic->state, "enqueue", mulib_elvi_logic_enqueue_, logic) != MULIB_MUNE_OK
        || mulib_mune_bind(logic->state, "emit", mulib_elvi_logic_emit_, logic) != MULIB_MUNE_OK
        || mulib_mune_run(logic->state, chunk, &result, NULL) != MULIB_MUNE_OK) goto fail;
    mulib_mune_result_destroy(result);
    result = NULL;
    logic->hooks = mulib_mune_result_create(logic->state);
    if (!logic->hooks) goto fail;
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        mulib_mune_value_t fn;
        if (mulib_mune_get_global(logic->state, names[i], &fn) != MULIB_MUNE_OK
            || (fn.type != MULIB_MUNE_FUNCTION && (i == 1 || fn.type != MULIB_MUNE_NIL))
            || !mulib_mune_result_push(logic->hooks, fn)) goto fail;
    }
    logic->phase = MULIB_ELVI_LOGIC_START_;
    if (!mulib_elvi_logic_hook_(logic, 0, NULL, 0)) goto fail;
    logic->phase = MULIB_ELVI_LOGIC_IDLE_;
    mulib_mune_chunk_destroy(chunk);
    return logic;
fail:
    mulib_mune_result_destroy(result);
    mulib_mune_chunk_destroy(chunk);
    logic->phase = MULIB_ELVI_LOGIC_DEAD_;
    mulib_elvi_script_logic_destroy(logic);
    return NULL;
}

int
mulib_elvi_script_logic_response(mulib_elvi_script_logic_t *logic,
    const mulib_elvi_script_response_t *response)
{
    const char *body, *end;
    mulib_xml_node_t *doc;
    mulib_mune_value_t args[2];
    size_t count, width;
    int ok;
    if (!logic || logic->phase != MULIB_ELVI_LOGIC_IDLE_) return 0;
    logic->phase = MULIB_ELVI_LOGIC_DEAD_;
    if (!response || !response->message.is_response || response->message.status < 200
        || response->message.status >= 300 || !response->message.body_length
        || response->message.body_length > MULIB_ELVI_SCRIPT_MAX_BODY
        || !response->message.body || logic->responses == MULIB_ELVI_SCRIPT_MAX_REQUESTS
        || !(end = (const char *)memchr(response->effective_url, 0, sizeof response->effective_url))
        || !mulib_elvi_logic_url_(response->effective_url)) return 0;
    body = (const char *)response->message.body;
    logic->body_length = response->message.body_length;
    if (!mulib_html_textstream_scan(body, logic->body_length, mulib_elvi_logic_html_event_,
                                  NULL, MULIB_ELVI_LOGIC_HTML_EVENTS_)) return 0;
    doc = mulib_html_parse(body, logic->body_length, 0, NULL);
    if (!doc) return 0;
    ++logic->responses;
    logic->phase = MULIB_ELVI_LOGIC_RESPONSE_;
    logic->xpath_left = UINT64_C(128) * 1024 * 1024;
    logic->inspect_left = 4u * 1024u * 1024u;
    ok = mulib_elvi_logic_shape_(doc, &count, &logic->depth, &width)
        && mulib_elvi_logic_make_node_(logic, doc, &args[0])
        && mulib_mune_make_string(logic->state, response->effective_url,
                                 (size_t)(end - response->effective_url), &args[1]) == MULIB_MUNE_OK
        && mulib_elvi_logic_hook_(logic, 1, args, 2);
    while (logic->handles) logic->nodes[--logic->handles] = NULL;
    logic->phase = ok ? MULIB_ELVI_LOGIC_IDLE_ : MULIB_ELVI_LOGIC_DEAD_;
    mulib_xml_destroy(doc);
    return ok;
}

int
mulib_elvi_script_logic_finish(mulib_elvi_script_logic_t *logic)
{
    int ok;
    if (!logic || logic->phase != MULIB_ELVI_LOGIC_IDLE_) return 0;
    logic->phase = MULIB_ELVI_LOGIC_FINISH_;
    ok = mulib_elvi_logic_hook_(logic, 2, NULL, 0);
    logic->phase = MULIB_ELVI_LOGIC_DEAD_;
    return ok;
}

void
mulib_elvi_script_logic_destroy(mulib_elvi_script_logic_t *logic)
{
    if (!logic || logic->phase == MULIB_ELVI_LOGIC_START_
        || logic->phase == MULIB_ELVI_LOGIC_RESPONSE_ || logic->phase == MULIB_ELVI_LOGIC_FINISH_)
        return;
    mulib_mune_result_destroy(logic->hooks);
    mulib_mune_state_destroy(logic->state);
    free(logic);
}
#endif /* implementation */
