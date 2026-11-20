/**
 * @file mulib-mcp-client.h
 * @brief Bounded, single-in-flight Model Context Protocol client.
 * Define MULIB_IMPL_MCP_CLIENT in one translation unit. The legacy
 * MULIB_IMPL_CDD_MCP_CLIENT switch also emits both APIs. See the manual
 * for dependencies.
 *
 * This kernel only builds and parses frames. The host owns transport,
 * authorization, and all decisions to send or retry requests. Treat
 * peer metadata and response content as untrusted.
 */
#ifndef MULIB_MCP_CLIENT_H
#define MULIB_MCP_CLIENT_H
#include "mulib-json.h"
#include "detail/mulib-mcp-common.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Caller storage; fields private. Use one instance per peer/channel. */
  typedef struct mulib_cdd_mcp_client
  {
    const char *name, *version;
    unsigned long long serial;
    unsigned phase, pending;
    int tools;
  } mulib_cdd_mcp_client_t;

  /** Borrow immutable name/version, each nonempty UTF-8 and <=256 bytes.
   * Re-init only when changing/resetting the transport; IDs restart. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_client_init (
      mulib_cdd_mcp_client_t *client, const char *name, const char *version);

  typedef enum mulib_cdd_mcp_operation
  {
    MULIB_CDD_MCP_DISCOVER,
    MULIB_CDD_MCP_INITIALIZE_LEGACY,
    MULIB_CDD_MCP_INITIALIZED_LEGACY,
    MULIB_CDD_MCP_PING_LEGACY,
    MULIB_CDD_MCP_TOOLS_LIST,
    MULIB_CDD_MCP_TOOLS_CALL,
    MULIB_CDD_MCP_RESOURCES_LIST,
    MULIB_CDD_MCP_RESOURCE_TEMPLATES_LIST,
    MULIB_CDD_MCP_RESOURCES_READ,
    MULIB_CDD_MCP_PROMPTS_LIST,
    MULIB_CDD_MCP_PROMPTS_GET,
    MULIB_CDD_MCP_COMPLETE
  } mulib_cdd_mcp_operation_t;

  /** Build one malloc-owned, compact frame with no delimiter. With
   * TOOLS_CALL, target is the tool name and object is optional arguments.
   * For any list, target is an optional cursor and object must be NULL.
   * RESOURCES_READ takes an absolute URI; PROMPTS_GET takes the prompt name
   * and optional string-map arguments. COMPLETE takes the ref/argument/context
   * object and no target. Other operations require both NULL. Borrowed inputs.
   * Free the frame after sending. A failed build changes neither state nor out. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_client_request (
      mulib_cdd_mcp_client_t *client, mulib_cdd_mcp_operation_t operation,
       const char *target, const mulib_json_t *object, char **out);

  typedef struct mulib_cdd_mcp_request_options
  {
    /* Optional object of extra _meta (e.g. progressToken or vendor keys).
     * The kernel owns MCP's protocolVersion, clientInfo and capabilities. */
    const mulib_json_t *meta;
    /* Exact unmodified requestState from a state-only INPUT_REQUIRED result,
     * for a host-approved retry with identical target and arguments. */
    const mulib_json_t *request_state;
    /* Use current protocol without discovery or capability gating. */
    int direct;
  } mulib_cdd_mcp_request_options_t;

  mulib_cdd_mcp_status_t mulib_cdd_mcp_client_request_ex (
      mulib_cdd_mcp_client_t *client, mulib_cdd_mcp_operation_t operation,
      const char *target, const mulib_json_t *object,
      const mulib_cdd_mcp_request_options_t *options, char **out);

  /** Consume one UTF-8 JSON-RPC frame with no delimiter. On OK, REMOTE_ERROR,
   * LEGACY_PEER, NO_VERSION, INPUT_REQUIRED or UNSUPPORTED, completes the pending request and
   * returns the owned response DOM. Destroy it with mulib_json_destroy.
   * Other statuses leave out unchanged; IGNORED means notification/stale ID. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_client_receive (
      mulib_cdd_mcp_client_t *client, const char *frame, size_t length,
      mulib_json_t **out);

  /** Stop waiting for a response without sending or reusing its ID. A host
   * timeout of the initial discovery can be followed by explicit legacy
   * initialization. Returns STATE when no request is pending. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_client_abandon (
       mulib_cdd_mcp_client_t *client);

  /** Format a stdio cancellation notification and abandon on success.
   * The host must send/free it. Does not cancel a legacy initialize request
   * or guarantee the peer will stop. Do not send it over modern HTTP. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_client_cancel (
      mulib_cdd_mcp_client_t *client, const char *reason, char **out);

  /* Standalone names use the same types, status values and contracts as
   * the corresponding mulib_cdd_mcp_client_* functions. The old names and
   * struct/enum tags remain available for source and link compatibility. */
  typedef mulib_cdd_mcp_client_t mulib_mcp_client_t;
  typedef mulib_cdd_mcp_status_t mulib_mcp_client_status_t;
  typedef mulib_cdd_mcp_operation_t mulib_mcp_client_operation_t;
  typedef mulib_cdd_mcp_request_options_t mulib_mcp_client_request_options_t;

#define MULIB_MCP_CLIENT_VERSION MULIB_CDD_MCP_VERSION
#define MULIB_MCP_CLIENT_LEGACY_VERSION MULIB_CDD_MCP_LEGACY_VERSION
#define MULIB_MCP_CLIENT_MAX_FRAME MULIB_CDD_MCP_MAX_FRAME

#define MULIB_MCP_CLIENT_OK MULIB_CDD_MCP_OK
#define MULIB_MCP_CLIENT_INVALID MULIB_CDD_MCP_INVALID
#define MULIB_MCP_CLIENT_CONFLICT MULIB_CDD_MCP_CONFLICT
#define MULIB_MCP_CLIENT_LIMIT MULIB_CDD_MCP_LIMIT
#define MULIB_MCP_CLIENT_NOMEM MULIB_CDD_MCP_NOMEM
#define MULIB_MCP_CLIENT_STATE MULIB_CDD_MCP_STATE
#define MULIB_MCP_CLIENT_IGNORED MULIB_CDD_MCP_IGNORED
#define MULIB_MCP_CLIENT_REMOTE_ERROR MULIB_CDD_MCP_REMOTE_ERROR
#define MULIB_MCP_CLIENT_LEGACY_PEER MULIB_CDD_MCP_LEGACY_PEER
#define MULIB_MCP_CLIENT_NO_VERSION MULIB_CDD_MCP_NO_VERSION
#define MULIB_MCP_CLIENT_UNSUPPORTED MULIB_CDD_MCP_UNSUPPORTED
#define MULIB_MCP_CLIENT_INPUT_REQUIRED MULIB_CDD_MCP_INPUT_REQUIRED

#define MULIB_MCP_CLIENT_DISCOVER MULIB_CDD_MCP_DISCOVER
#define MULIB_MCP_CLIENT_INITIALIZE_LEGACY MULIB_CDD_MCP_INITIALIZE_LEGACY
#define MULIB_MCP_CLIENT_INITIALIZED_LEGACY MULIB_CDD_MCP_INITIALIZED_LEGACY
#define MULIB_MCP_CLIENT_PING_LEGACY MULIB_CDD_MCP_PING_LEGACY
#define MULIB_MCP_CLIENT_TOOLS_LIST MULIB_CDD_MCP_TOOLS_LIST
#define MULIB_MCP_CLIENT_TOOLS_CALL MULIB_CDD_MCP_TOOLS_CALL
#define MULIB_MCP_CLIENT_RESOURCES_LIST MULIB_CDD_MCP_RESOURCES_LIST
#define MULIB_MCP_CLIENT_RESOURCE_TEMPLATES_LIST MULIB_CDD_MCP_RESOURCE_TEMPLATES_LIST
#define MULIB_MCP_CLIENT_RESOURCES_READ MULIB_CDD_MCP_RESOURCES_READ
#define MULIB_MCP_CLIENT_PROMPTS_LIST MULIB_CDD_MCP_PROMPTS_LIST
#define MULIB_MCP_CLIENT_PROMPTS_GET MULIB_CDD_MCP_PROMPTS_GET
#define MULIB_MCP_CLIENT_COMPLETE MULIB_CDD_MCP_COMPLETE

  mulib_mcp_client_status_t mulib_mcp_client_init (
      mulib_mcp_client_t *client, const char *name, const char *version);
  mulib_mcp_client_status_t mulib_mcp_client_request (
      mulib_mcp_client_t *client, mulib_mcp_client_operation_t operation,
      const char *target, const mulib_json_t *object, char **out);
  mulib_mcp_client_status_t mulib_mcp_client_request_ex (
      mulib_mcp_client_t *client, mulib_mcp_client_operation_t operation,
      const char *target, const mulib_json_t *object,
      const mulib_mcp_client_request_options_t *options, char **out);
  mulib_mcp_client_status_t mulib_mcp_client_receive (
      mulib_mcp_client_t *client, const char *frame, size_t length,
      mulib_json_t **out);
  mulib_mcp_client_status_t mulib_mcp_client_abandon (mulib_mcp_client_t *client);
  mulib_mcp_client_status_t mulib_mcp_client_cancel (
      mulib_mcp_client_t *client, const char *reason, char **out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MCP_CLIENT_H */

#if defined(MULIB_IMPL_CDD_MCP_CLIENT) || defined(MULIB_IMPL_MCP_CLIENT)
#ifndef MULIB_MCP_CLIENT_IMPLEMENTED
#define MULIB_MCP_CLIENT_IMPLEMENTED
#include "mulib-utf8.h"
#include "detail/mulib-mcp-wire.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_MCP_CLIENT_NEW_,
    MULIB_MCP_CLIENT_LEGACY_HINT_,
    MULIB_MCP_CLIENT_MODERN_ONLY_,
    MULIB_MCP_CLIENT_MODERN_,
    MULIB_MCP_CLIENT_LEGACY_NOTIFY_,
    MULIB_MCP_CLIENT_LEGACY_READY_,
    MULIB_MCP_CLIENT_LEGACY_INCOMPATIBLE_
  };

  enum { MULIB_MCP_CLIENT_DIRECT_ = 256 };

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_client_init (mulib_cdd_mcp_client_t *client, const char *name,
                              const char *version)
  {
    if (!client || !mulib_mcp_wire_cstr_ (name, 256) || !name[0]
        || !mulib_mcp_wire_cstr_ (version, 256) || !version[0])
      return MULIB_CDD_MCP_INVALID;
    client->name = name;
    client->version = version;
    client->serial = 0;
    client->phase = MULIB_MCP_CLIENT_NEW_;
    client->pending = 0;
    client->tools = 0;
    return MULIB_CDD_MCP_OK;
  }

  static void
  mulib_mcp_client_id_ (char id[32], unsigned long long serial)
  {
    /* The request builder bounds serial to 64 bits, independent of ABI. */
    (void)snprintf (id, 32, "mcp-%llu", serial);
  }

  static int
  mulib_mcp_client_metadata_ (const mulib_cdd_mcp_client_t *cl,
                              mulib_json_t *params, int modern,
                              const mulib_json_t *extra)
  {
    mulib_json_t *meta = modern ? (extra ? mulib_mcp_wire_clone_ (extra)
                                       : mulib_mcp_wire_object_ ()) : params;
    int ok
        = meta
          && mulib_mcp_wire_str_ (
              meta, modern ? MULIB_MCP_WIRE_VERSION : "protocolVersion",
              modern ? MULIB_CDD_MCP_VERSION : MULIB_CDD_MCP_LEGACY_VERSION)
          && mulib_mcp_wire_put_ (
              meta, modern ? MULIB_MCP_WIRE_CAPS : "capabilities",
              mulib_mcp_wire_object_ ())
          && mulib_mcp_wire_put_ (
              meta, modern ? MULIB_MCP_WIRE_CLIENT : "clientInfo",
              mulib_mcp_wire_info_ (cl->name, cl->version));
    if (!modern)
      return ok;
    if (ok)
      return mulib_mcp_wire_put_ (params, "_meta", meta);
    mulib_json_destroy (meta);
    return 0;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_client_request_ex (mulib_cdd_mcp_client_t *cl,
                                   mulib_cdd_mcp_operation_t op,
                                   const char *target, const mulib_json_t *object,
                                   const mulib_cdd_mcp_request_options_t *opts, char **out)
  {
    const char *method, *target_key = NULL;
    char id[32];
    mulib_json_t *r, *params = NULL;
    mulib_cdd_mcp_status_t status = MULIB_CDD_MCP_NOMEM;
    int modern = 0, notify = 0, cap = 0, direct = 0;
    size_t budget = MULIB_MCP_WIRE_PAYLOAD;
    const mulib_json_t *extra = opts ? opts->meta : NULL;
    const mulib_json_t *state = opts ? opts->request_state : NULL;
    if (!cl || !cl->name || !out)
      return MULIB_CDD_MCP_INVALID;
    if (cl->pending)
      return MULIB_CDD_MCP_STATE;
    if (op != MULIB_CDD_MCP_TOOLS_CALL && op != MULIB_CDD_MCP_PROMPTS_GET
        && op != MULIB_CDD_MCP_COMPLETE && object)
      return MULIB_CDD_MCP_INVALID;
    if ((op < MULIB_CDD_MCP_TOOLS_LIST || op == MULIB_CDD_MCP_COMPLETE) && target)
      return MULIB_CDD_MCP_INVALID;
    switch (op)
      {
      case MULIB_CDD_MCP_DISCOVER:
        if (cl->phase != MULIB_MCP_CLIENT_NEW_
            && cl->phase != MULIB_MCP_CLIENT_MODERN_ONLY_)
          return MULIB_CDD_MCP_STATE;
        method = "server/discover";
        modern = 1;
        break;
      case MULIB_CDD_MCP_INITIALIZE_LEGACY:
        if (cl->phase != MULIB_MCP_CLIENT_NEW_
            && cl->phase != MULIB_MCP_CLIENT_LEGACY_HINT_)
          return MULIB_CDD_MCP_STATE;
        method = "initialize";
        break;
      case MULIB_CDD_MCP_INITIALIZED_LEGACY:
        if (cl->phase != MULIB_MCP_CLIENT_LEGACY_NOTIFY_)
          return MULIB_CDD_MCP_STATE;
        method = "notifications/initialized";
        notify = 1;
        break;
      case MULIB_CDD_MCP_PING_LEGACY:
        if (cl->phase != MULIB_MCP_CLIENT_LEGACY_NOTIFY_
            && cl->phase != MULIB_MCP_CLIENT_LEGACY_READY_)
          return MULIB_CDD_MCP_STATE;
        method = "ping";
        break;
      case MULIB_CDD_MCP_TOOLS_LIST:
      case MULIB_CDD_MCP_TOOLS_CALL:
      case MULIB_CDD_MCP_RESOURCES_LIST:
      case MULIB_CDD_MCP_RESOURCE_TEMPLATES_LIST:
      case MULIB_CDD_MCP_RESOURCES_READ:
      case MULIB_CDD_MCP_PROMPTS_LIST:
      case MULIB_CDD_MCP_PROMPTS_GET:
      case MULIB_CDD_MCP_COMPLETE:
        {
          static const char *const methods[] = {
            "tools/list", "tools/call", "resources/list", "resources/templates/list",
            "resources/read", "prompts/list", "prompts/get", "completion/complete"
          };
          static const char *const keys[] = {
            "cursor", "name", "cursor", "cursor", "uri", "cursor", "name", NULL
          };
          static const int capabilities[] = {
            MULIB_MCP_WIRE_TOOLS, MULIB_MCP_WIRE_TOOLS, MULIB_MCP_WIRE_RESOURCES,
            MULIB_MCP_WIRE_RESOURCES, MULIB_MCP_WIRE_RESOURCES, MULIB_MCP_WIRE_PROMPTS,
            MULIB_MCP_WIRE_PROMPTS, MULIB_MCP_WIRE_COMPLETIONS
          };
          const unsigned i = (unsigned)(op - MULIB_CDD_MCP_TOOLS_LIST);
          method = methods[i];
          target_key = keys[i];
          cap = capabilities[i];
        }
        direct = opts && opts->direct;
        if (direct && cl->phase != MULIB_MCP_CLIENT_NEW_
            && cl->phase != MULIB_MCP_CLIENT_MODERN_ONLY_
            && cl->phase != MULIB_MCP_CLIENT_MODERN_)
          return MULIB_CDD_MCP_STATE;
        if (!direct && cl->phase != MULIB_MCP_CLIENT_MODERN_
            && cl->phase != MULIB_MCP_CLIENT_LEGACY_READY_)
          return MULIB_CDD_MCP_STATE;
        if (!direct && !(cl->tools & cap))
          return MULIB_CDD_MCP_UNSUPPORTED;
        modern = direct || cl->phase == MULIB_MCP_CLIENT_MODERN_;
        if ((op == MULIB_CDD_MCP_TOOLS_CALL
              && (!mulib_mcp_wire_cstr_ (target, 128) || !target[0]
                  || (object && mulib_json_typeof (object) != MULIB_JSON_OBJECT)))
            || ((op == MULIB_CDD_MCP_PROMPTS_GET || op == MULIB_CDD_MCP_RESOURCES_READ)
                && (!target || !*target))
            || (op == MULIB_CDD_MCP_PROMPTS_GET && !mulib_mcp_wire_string_map_ (object))
            || (op == MULIB_CDD_MCP_COMPLETE
                && (!mulib_mcp_wire_complete_args_ (object)
                    || mulib_json_size (object) != (mulib_json_get (object, "context") ? 3u : 2u)))
            || (target && !mulib_mcp_wire_cstr_ (target, 4096)))
          return MULIB_CDD_MCP_INVALID;
        if (object && !mulib_mcp_wire_measure_ (object, &budget, 0))
          return MULIB_CDD_MCP_LIMIT;
        break;
      default:
        return MULIB_CDD_MCP_INVALID;
      }
    if ((opts && opts->direct && !direct)
        || (extra && (!mulib_mcp_wire_request_meta_ (extra) || notify
                     || mulib_json_get (extra, MULIB_MCP_WIRE_VERSION)
                     || mulib_json_get (extra, MULIB_MCP_WIRE_CLIENT)
                     || mulib_json_get (extra, MULIB_MCP_WIRE_CAPS)))
        || (state && (!modern || !mulib_mcp_wire_string_ (state)
                     || (op != MULIB_CDD_MCP_TOOLS_CALL && op != MULIB_CDD_MCP_PROMPTS_GET
                         && op != MULIB_CDD_MCP_RESOURCES_READ))))
      return MULIB_CDD_MCP_INVALID;
    if ((target && !mulib_mcp_wire_charge_text_ (&budget, strlen (target)))
        || (extra && !mulib_mcp_wire_measure_ (extra, &budget, 0))
        || (state && !mulib_mcp_wire_measure_ (state, &budget, 0)))
      return MULIB_CDD_MCP_LIMIT;
    if (op == MULIB_CDD_MCP_RESOURCES_READ)
      {
        mulib_uri_t u;
        if (!mulib_uri_parse (target, strlen (target), &u) || !u.scheme.length)
          return MULIB_CDD_MCP_INVALID;
      }
    if (!notify && cl->serial >= UINT64_MAX)
      return MULIB_CDD_MCP_LIMIT;
    r = mulib_mcp_wire_object_ ();
    if (!r || !mulib_mcp_wire_str_ (r, "jsonrpc", "2.0")
        || !mulib_mcp_wire_str_ (r, "method", method))
      goto done;
    if (!notify)
      {
        mulib_mcp_client_id_ (id, cl->serial + 1);
        if (!mulib_mcp_wire_str_ (r, "id", id))
          goto done;
      }
    if (modern || op == MULIB_CDD_MCP_INITIALIZE_LEGACY || target || object || extra)
      {
        params = op == MULIB_CDD_MCP_COMPLETE ? mulib_mcp_wire_clone_ (object)
                                             : mulib_mcp_wire_object_ ();
        if (!params
            || ((modern || op == MULIB_CDD_MCP_INITIALIZE_LEGACY)
                && !mulib_mcp_client_metadata_ (cl, params, modern, extra))
            || (!modern && extra && !mulib_mcp_wire_put_ (
                                        params, "_meta", mulib_mcp_wire_clone_ (extra)))
            || (target && !mulib_mcp_wire_str_ (params, target_key, target))
            || (object && op != MULIB_CDD_MCP_COMPLETE
                && !mulib_mcp_wire_put_ (params, "arguments", mulib_mcp_wire_clone_ (object)))
            || (state && !mulib_mcp_wire_put_ (
                             params, "requestState", mulib_mcp_wire_clone_ (state)))
            || !mulib_json_set (r, "params", params))
          goto done;
        params = NULL;
      }
    status = mulib_mcp_wire_format_ (r, out);
    if (status == MULIB_CDD_MCP_OK)
      {
        if (notify)
          cl->phase = MULIB_MCP_CLIENT_LEGACY_READY_;
        else
          {
            ++cl->serial;
            cl->pending = ((unsigned)op + 1) | (direct ? MULIB_MCP_CLIENT_DIRECT_ : 0);
          }
      }
  done:
    mulib_json_destroy (params);
    mulib_json_destroy (r);
    return status;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_client_request (mulib_cdd_mcp_client_t *cl,
                                mulib_cdd_mcp_operation_t op,
                                const char *target, const mulib_json_t *object, char **out)
  {
    return mulib_cdd_mcp_client_request_ex (cl, op, target, object, NULL, out);
  }

  /* -1 malformed, 0 no matching modern version, 1 matching. A legacy
     version advertised by a modern server is not a failed-era probe. */
  static int
  mulib_mcp_client_versions_ (const mulib_json_t *a)
  {
    int seen = 0;
    if (mulib_json_typeof (a) != MULIB_JSON_ARRAY || !mulib_json_size (a))
      return -1;
    for (size_t i = 0; i < mulib_json_size (a); ++i)
      {
        const mulib_json_t *v = mulib_json_at (a, i);
        if (mulib_json_typeof (v) != MULIB_JSON_STRING)
          return -1;
        if (mulib_mcp_wire_is_ (v, MULIB_CDD_MCP_VERSION))
          seen = 1;
      }
    return seen;
  }

  static int
  mulib_mcp_client_list_valid_ (const mulib_json_t *result, const char *key, int legacy)
  {
    const mulib_json_t *list = mulib_json_get (result, key);
    const mulib_json_t *cursor = mulib_json_get (result, "nextCursor");
    size_t n;
    if (mulib_json_typeof (list) != MULIB_JSON_ARRAY
        || (cursor && (!mulib_json_text (cursor, &n) || n > 4096
                       || memchr (mulib_json_text (cursor, NULL), 0, n))))
      return 0;
    for (size_t i = 0; i < mulib_json_size (list); ++i)
      {
        const mulib_json_t *entry = mulib_json_at (list, i);
        if (!strcmp (key, "tools"))
          {
            const mulib_json_t *schema = mulib_json_get (entry, "outputSchema");
            if (!mulib_mcp_wire_label_ (mulib_json_get (entry, "name"), 4096)
                || !mulib_mcp_wire_opt_ (entry, "description", MULIB_JSON_STRING)
                || !mulib_mcp_wire_is_ (
                    mulib_json_get (mulib_json_get (entry, "inputSchema"), "type"), "object")
                || (schema && (mulib_json_typeof (schema) != MULIB_JSON_OBJECT
                               || (legacy && !mulib_mcp_wire_is_ (
                                     mulib_json_get (schema, "type"), "object")))))
              return 0;
          }
        else if (!strcmp (key, "prompts") ? !mulib_mcp_wire_prompt_ (entry)
                  : !mulib_mcp_wire_resource_ (entry, !strcmp (key, "resourceTemplates")))
          return 0;
      }
    return 1;
  }

  static int
  mulib_mcp_client_caps_ (const mulib_json_t *caps, int *bits)
  {
    const char *const names[] = { "tools", "resources", "prompts", "completions" };
    int mask = 0;
    if (mulib_json_typeof (caps) != MULIB_JSON_OBJECT)
      return 0;
    for (unsigned i = 0; i < 4; ++i)
      {
        const mulib_json_t *v = mulib_json_get (caps, names[i]);
        if (v && (mulib_json_typeof (v) != MULIB_JSON_OBJECT
                  || (i != 3 && !mulib_mcp_wire_opt_ (v, "listChanged", MULIB_JSON_BOOL))
                  || (i == 1 && !mulib_mcp_wire_opt_ (v, "subscribe", MULIB_JSON_BOOL))))
          return 0;
        if (v)
          mask |= 1 << i;
      }
    *bits = mask;
    return 1;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_client_receive (mulib_cdd_mcp_client_t *cl, const char *frame,
                                 size_t length, mulib_json_t **out)
  {
    const mulib_json_t *id, *result, *error, *type, *meta, *info;
    mulib_json_t *r;
    mulib_json_error_t parse_error = { 0, 0, 0, NULL };
    mulib_cdd_mcp_status_t status;
    unsigned phase, pending;
    int has_tools, modern;
    char expected[32];
    if (!cl || !cl->name || !out)
      return MULIB_CDD_MCP_INVALID;
    status = mulib_mcp_wire_frame_ (frame, length);
    if (status != MULIB_CDD_MCP_OK)
      return status;
    r = mulib_mcp_wire_parse_ (frame, length, &parse_error);
    if (!r)
      return mulib_mcp_wire_parse_nomem_ (&parse_error) ? MULIB_CDD_MCP_NOMEM
                                                     : MULIB_CDD_MCP_INVALID;
    status = MULIB_CDD_MCP_INVALID;
    phase = cl->phase;
    has_tools = cl->tools;
    pending = cl->pending & ~(unsigned)MULIB_MCP_CLIENT_DIRECT_;
    modern = pending == MULIB_CDD_MCP_DISCOVER + 1 || phase == MULIB_MCP_CLIENT_MODERN_
             || (cl->pending & MULIB_MCP_CLIENT_DIRECT_);
    if (!mulib_mcp_wire_is_ (mulib_json_get (r, "jsonrpc"), "2.0"))
      goto done;
    id = mulib_json_get (r, "id");
    result = mulib_json_get (r, "result");
    error = mulib_json_get (r, "error");
    if (mulib_json_get (r, "method"))
      {
        const mulib_json_t *params = mulib_json_get (r, "params");
        if (!id && !result && !error
            && mulib_json_typeof (mulib_json_get (r, "method")) == MULIB_JSON_STRING
            && (!params || mulib_json_typeof (params) == MULIB_JSON_OBJECT))
          status = MULIB_CDD_MCP_IGNORED;
        goto done;
      }
    if (!!result == !!error)
      goto done;
    mulib_mcp_client_id_ (expected, cl->serial);
    if (!cl->pending || !mulib_mcp_wire_is_ (id, expected))
      {
        status = MULIB_CDD_MCP_IGNORED;
        goto done;
      }
    if (error)
      {
        const mulib_json_t *code = mulib_json_get (error, "code");
        double n = mulib_json_numeric (code);
        if (!mulib_mcp_wire_integer_ (code)
            || mulib_json_typeof (mulib_json_get (error, "message")) != MULIB_JSON_STRING)
          goto done;
        status = MULIB_CDD_MCP_REMOTE_ERROR;
        if (pending == MULIB_CDD_MCP_DISCOVER + 1)
          {
            if (n == -32022 || n == -32020 || n == -32021)
              phase = MULIB_MCP_CLIENT_MODERN_ONLY_;
            else if (phase == MULIB_MCP_CLIENT_NEW_)
              {
                phase = MULIB_MCP_CLIENT_LEGACY_HINT_;
                status = MULIB_CDD_MCP_LEGACY_PEER;
              }
          }
        if (n == -32022 && modern)
          {
            const mulib_json_t *data = mulib_json_get (error, "data");
            int versions = mulib_mcp_client_versions_ (
                mulib_json_get (data, "supported"));
            if (versions < 0 || mulib_json_typeof (mulib_json_get (data, "requested"))
                                    != MULIB_JSON_STRING)
              goto invalid;
            phase = MULIB_MCP_CLIENT_MODERN_ONLY_;
            if (!versions)
              status = MULIB_CDD_MCP_NO_VERSION;
          }
        goto consume;
      }
    if (mulib_json_typeof (result) != MULIB_JSON_OBJECT
        || (modern ? !mulib_mcp_wire_meta_ (mulib_json_get (result, "_meta"))
                   : !mulib_mcp_wire_opt_ (result, "_meta", MULIB_JSON_OBJECT)))
      goto done;
    meta = mulib_json_get (result, "_meta");
    info = mulib_json_get (meta, MULIB_MCP_WIRE_SERVER);
    if (modern && info && !mulib_mcp_wire_info_valid_ (info))
      goto done;
    type = mulib_json_get (result, "resultType");
    if (!type && modern)
      goto done;
    if (type && !mulib_mcp_wire_is_ (type, "complete"))
      {
        const mulib_json_t *state = mulib_json_get (result, "requestState");
        const mulib_json_t *inputs = mulib_json_get (result, "inputRequests");
        if (!modern || !mulib_mcp_wire_is_ (type, "input_required")
            || (pending != MULIB_CDD_MCP_TOOLS_CALL + 1
                && pending != MULIB_CDD_MCP_RESOURCES_READ + 1
                && pending != MULIB_CDD_MCP_PROMPTS_GET + 1)
            || (!state && !inputs)
            || (state && !mulib_mcp_wire_string_ (state))
            || (inputs && mulib_json_typeof (inputs) != MULIB_JSON_OBJECT))
          goto done;
        if (phase == MULIB_MCP_CLIENT_NEW_)
          phase = MULIB_MCP_CLIENT_MODERN_ONLY_;
        status = inputs ? MULIB_CDD_MCP_UNSUPPORTED : MULIB_CDD_MCP_INPUT_REQUIRED;
        goto consume;
      }
    if (modern && (mulib_json_get (result, "requestState")
                   || mulib_json_get (result, "inputRequests")))
      goto done;
    switch (pending - 1)
      {
      case MULIB_CDD_MCP_DISCOVER:
      case MULIB_CDD_MCP_INITIALIZE_LEGACY:
        if (!mulib_mcp_client_caps_ (mulib_json_get (result, "capabilities"), &has_tools)
            || !mulib_mcp_wire_opt_ (result, "instructions", MULIB_JSON_STRING))
          goto done;
        if (pending == MULIB_CDD_MCP_DISCOVER + 1)
          {
            int versions = mulib_mcp_client_versions_ (
                mulib_json_get (result, "supportedVersions"));
            if (versions < 0 || !mulib_mcp_wire_cache_ (result))
              goto done;
            phase = versions ? MULIB_MCP_CLIENT_MODERN_ : MULIB_MCP_CLIENT_MODERN_ONLY_;
            status = versions ? MULIB_CDD_MCP_OK : MULIB_CDD_MCP_NO_VERSION;
            goto consume;
          }
        info = mulib_json_get (result, "protocolVersion");
        if (mulib_json_typeof (info) != MULIB_JSON_STRING
            || !mulib_mcp_wire_info_valid_ (mulib_json_get (result, "serverInfo")))
          goto done;
        if (!mulib_mcp_wire_is_ (info, MULIB_CDD_MCP_LEGACY_VERSION))
          {
            /* The peer may retain the incompatible handshake; do not retry it. */
            phase = MULIB_MCP_CLIENT_LEGACY_INCOMPATIBLE_;
            status = MULIB_CDD_MCP_NO_VERSION;
            goto consume;
          }
        phase = MULIB_MCP_CLIENT_LEGACY_NOTIFY_;
        break;
      case MULIB_CDD_MCP_TOOLS_LIST:
      case MULIB_CDD_MCP_RESOURCES_LIST:
      case MULIB_CDD_MCP_RESOURCE_TEMPLATES_LIST:
      case MULIB_CDD_MCP_PROMPTS_LIST:
        {
          const char *key = pending == MULIB_CDD_MCP_TOOLS_LIST + 1 ? "tools"
              : pending == MULIB_CDD_MCP_RESOURCES_LIST + 1 ? "resources"
              : pending == MULIB_CDD_MCP_RESOURCE_TEMPLATES_LIST + 1 ? "resourceTemplates"
              : "prompts";
          if (!mulib_mcp_client_list_valid_ (result, key, !modern)
              || (modern && !mulib_mcp_wire_cache_ (result)))
            goto done;
          break;
        }
      case MULIB_CDD_MCP_TOOLS_CALL:
      case MULIB_CDD_MCP_RESOURCES_READ:
      case MULIB_CDD_MCP_PROMPTS_GET:
      case MULIB_CDD_MCP_COMPLETE:
        if (!mulib_mcp_wire_payload_ (
                result, pending == MULIB_CDD_MCP_TOOLS_CALL + 1 ? "content"
                : pending == MULIB_CDD_MCP_RESOURCES_READ + 1 ? "contents"
                : pending == MULIB_CDD_MCP_PROMPTS_GET + 1 ? "messages" : "completion",
                !modern)
            || (modern && pending == MULIB_CDD_MCP_RESOURCES_READ + 1
                && !mulib_mcp_wire_cache_ (result)))
          goto done;
        break;
      default:
        break; /* legacy ping */
      }
    status = MULIB_CDD_MCP_OK;
  consume:
    if (modern && phase == MULIB_MCP_CLIENT_NEW_)
      phase = MULIB_MCP_CLIENT_MODERN_ONLY_;
    cl->pending = 0;
    cl->phase = phase;
    cl->tools = has_tools;
    *out = r;
    return status;
  invalid:
    status = MULIB_CDD_MCP_INVALID;
  done:
    mulib_json_destroy (r);
    return status;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_client_abandon (mulib_cdd_mcp_client_t *cl)
  {
    if (!cl || !cl->name)
      return MULIB_CDD_MCP_INVALID;
    if (!cl->pending)
      return MULIB_CDD_MCP_STATE;
    if (cl->pending == MULIB_CDD_MCP_DISCOVER + 1
        && cl->phase == MULIB_MCP_CLIENT_NEW_)
      cl->phase = MULIB_MCP_CLIENT_LEGACY_HINT_;
    cl->pending = 0;
    return MULIB_CDD_MCP_OK;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_client_cancel (mulib_cdd_mcp_client_t *cl, const char *reason,
                               char **out)
  {
    mulib_json_t *r, *params;
    char id[32];
    mulib_cdd_mcp_status_t status = MULIB_CDD_MCP_NOMEM;
    if (!cl || !cl->name || !out || (reason && !mulib_mcp_wire_cstr_ (reason, 4096)))
      return MULIB_CDD_MCP_INVALID;
    if (!cl->pending || cl->pending == MULIB_CDD_MCP_INITIALIZE_LEGACY + 1)
      return MULIB_CDD_MCP_STATE;
    mulib_mcp_client_id_ (id, cl->serial);
    r = mulib_mcp_wire_object_ ();
    params = mulib_mcp_wire_object_ ();
    if (r && params && mulib_mcp_wire_str_ (r, "jsonrpc", "2.0")
        && mulib_mcp_wire_str_ (r, "method", "notifications/cancelled")
        && mulib_mcp_wire_str_ (params, "requestId", id)
        && (!reason || mulib_mcp_wire_str_ (params, "reason", reason))
        && mulib_json_set (r, "params", params))
      {
        params = NULL;
        status = mulib_mcp_wire_format_ (r, out);
        if (status == MULIB_CDD_MCP_OK)
          (void)mulib_cdd_mcp_client_abandon (cl);
      }
    mulib_json_destroy (params);
    mulib_json_destroy (r);
    return status;
  }

  mulib_mcp_client_status_t
  mulib_mcp_client_init (mulib_mcp_client_t *client, const char *name,
                         const char *version)
  {
    return mulib_cdd_mcp_client_init (client, name, version);
  }

  mulib_mcp_client_status_t
  mulib_mcp_client_request (mulib_mcp_client_t *client,
                            mulib_mcp_client_operation_t operation,
                            const char *target, const mulib_json_t *object,
                            char **out)
  {
    return mulib_cdd_mcp_client_request (client, operation, target, object, out);
  }

  mulib_mcp_client_status_t
  mulib_mcp_client_request_ex (mulib_mcp_client_t *client,
                               mulib_mcp_client_operation_t operation,
                               const char *target, const mulib_json_t *object,
                               const mulib_mcp_client_request_options_t *options,
                               char **out)
  {
    return mulib_cdd_mcp_client_request_ex (client, operation, target, object,
                                            options, out);
  }

  mulib_mcp_client_status_t
  mulib_mcp_client_receive (mulib_mcp_client_t *client, const char *frame,
                            size_t length, mulib_json_t **out)
  {
    return mulib_cdd_mcp_client_receive (client, frame, length, out);
  }

  mulib_mcp_client_status_t
  mulib_mcp_client_abandon (mulib_mcp_client_t *client)
  {
    return mulib_cdd_mcp_client_abandon (client);
  }

  mulib_mcp_client_status_t
  mulib_mcp_client_cancel (mulib_mcp_client_t *client, const char *reason,
                           char **out)
  {
    return mulib_cdd_mcp_client_cancel (client, reason, out);
  }
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MCP_CLIENT_IMPLEMENTED */
#endif /* implementation */
