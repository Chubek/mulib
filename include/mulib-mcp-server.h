/**
 * @file mulib-mcp-server.h
 * @brief Bounded Model Context Protocol server and host-owned catalogue.
 * Define MULIB_IMPL_CDD_MCP_SERVER once. See the manual for dependencies.
 */
#ifndef MULIB_MCP_SERVER_H
#define MULIB_MCP_SERVER_H
#include "mulib-json.h"
#include "detail/mulib-mcp-common.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_cdd_mcp_tool_status
  {
    MULIB_CDD_MCP_TOOL_OK,
    MULIB_CDD_MCP_TOOL_ERROR,
    MULIB_CDD_MCP_TOOL_FAILED
  } mulib_cdd_mcp_tool_status_t;

  /** Borrow arguments for the duration of the call (NULL if omitted). Validate
   * their semantics before any action. Write at most capacity UTF-8 bytes
   * into text and set length. No terminator required. text may contain NUL.
   * TOOL_ERROR yields a tool result with isError; TOOL_FAILED is internal. */
  typedef mulib_cdd_mcp_tool_status_t (*mulib_cdd_mcp_tool_fn) (
      void *user, const mulib_json_t *arguments, char *text, size_t capacity,
      size_t *length);

  typedef struct mulib_cdd_mcp_tool
  {
    const char *name, *description; /* description optional */
    const mulib_json_t *input_schema; /* borrowed JSON object schema */
    mulib_cdd_mcp_tool_fn invoke;
    void *user;
  } mulib_cdd_mcp_tool_t;

  typedef struct mulib_cdd_mcp_server_config
  {
    const char *name, *version;
    const mulib_cdd_mcp_tool_t *tools;
    size_t tool_count;
    int allow_legacy;
  } mulib_cdd_mcp_server_config_t;

  /** Caller storage, fields private. One instance per legacy channel. */
  typedef struct mulib_cdd_mcp_server
  {
    mulib_cdd_mcp_server_config_t config;
    unsigned legacy_state;
  } mulib_cdd_mcp_server_t;

  /** Validate and copy the config; nested values stay borrowed and immutable.
   * On failure leaves server unchanged. Re-init resets legacy state. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_server_init (
      mulib_cdd_mcp_server_t *server,
      const mulib_cdd_mcp_server_config_t *config);

  /** Process one UTF-8 JSON-RPC frame (no LF/CR). On OK produces malloc-owned
   * compact JSON or NULL for a notification. On failure out is unchanged.
   * Neither concurrent nor reentrant use of the same instance is supported. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_server_process (
      mulib_cdd_mcp_server_t *server, const char *frame, size_t length,
      char **out);

  /** Borrowed for one callback. principal is supplied by the transport host
   * after authentication; clientInfo and every JSON field are untrusted. */
  typedef struct mulib_cdd_mcp_request_view
  {
    const char *method, *target; /* target URI/name, or NULL */
    const mulib_json_t *id, *params, *meta, *arguments, *request_state;
    const void *principal;
    int modern;
  } mulib_cdd_mcp_request_view_t;

  typedef enum mulib_cdd_mcp_handler_status
  {
    MULIB_CDD_MCP_HANDLER_OK,
    MULIB_CDD_MCP_HANDLER_INVALID,
    MULIB_CDD_MCP_HANDLER_FAILED,
    MULIB_CDD_MCP_HANDLER_DEFER
  } mulib_cdd_mcp_handler_status_t;

  /** On OK transfer a new owned result object (without the JSON-RPC
   * envelope) to the kernel; on DEFER transfer a JSON string (state-only
   * modern round trip). Host verifies state integrity, expiry and binding.
   * Never transfer a borrowed value. Kernel destroys any non-NULL output. */
  typedef mulib_cdd_mcp_handler_status_t (*mulib_cdd_mcp_handler_fn) (
      void *user, const mulib_cdd_mcp_request_view_t *request, mulib_json_t **out);

  /** Return nonzero to allow. Called after base envelope/metadata checks
   * but before method-specific validation and dispatch. An allowed list
   * exposes the entire configured inventory. Treat target/params as untrusted.
   * Never authenticate from the request body or mirrored routing headers. */
  typedef int (*mulib_cdd_mcp_authorize_fn) (
      void *user, const mulib_cdd_mcp_request_view_t *request);
  /** Host schema validator. value may be NULL for omitted arguments (treat
   * as {}). No schema lookup or evaluation is performed by the kernel. */
  typedef int (*mulib_cdd_mcp_validate_fn) (
      void *user, const mulib_json_t *schema, const mulib_json_t *value);

  typedef struct mulib_cdd_mcp_structured_tool
  {
    const char *name, *description;
    const mulib_json_t *input_schema, *output_schema;
    mulib_cdd_mcp_validate_fn validate;
    mulib_cdd_mcp_handler_fn invoke;
    void *user;
  } mulib_cdd_mcp_structured_tool_t;

  typedef struct mulib_cdd_mcp_catalog_config
  {
    /* Arrays of MCP wire descriptors, borrowed immutable. NULL = empty. */
    const mulib_json_t *resources, *resource_templates, *prompts;
    const mulib_cdd_mcp_structured_tool_t *tools;
    size_t tool_count;
    mulib_cdd_mcp_handler_fn read, get, complete;
    mulib_cdd_mcp_authorize_fn authorize;
    void *user; /* read/get/complete/authorize */
    size_t page_size; /* 0 selects 32; bounded by frame size as well */
    unsigned long long generation; /* change on catalogue/policy replacement */
    unsigned cache_ttl_ms; /* 0 = immediately stale; always private */
  } mulib_cdd_mcp_catalog_config_t;

  typedef struct mulib_cdd_mcp_catalog
  {
    mulib_cdd_mcp_server_t server;
    mulib_cdd_mcp_catalog_config_t features;
  } mulib_cdd_mcp_catalog_t;

  /** Same failure/state guarantees as server_init and server_process. The
   * host decides which catalogue/principal to use on each transport request.
   * Resource read accepts absolute URIs; only the host resolves templates. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_catalog_init (
      mulib_cdd_mcp_catalog_t *catalog, const mulib_cdd_mcp_server_config_t *base,
      const mulib_cdd_mcp_catalog_config_t *features);
  mulib_cdd_mcp_status_t mulib_cdd_mcp_catalog_process (
      mulib_cdd_mcp_catalog_t *catalog, const void *principal, const char *frame,
      size_t length, char **out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MCP_SERVER_H */

#if defined(MULIB_IMPL_CDD_MCP_SERVER) && !defined(MULIB_MCP_SERVER_IMPLEMENTED)
#define MULIB_MCP_SERVER_IMPLEMENTED
#include "mulib-utf8.h"
#include "detail/mulib-mcp-wire.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_mcp_server_name_ (const char *name)
  {
    if (!name || !*name)
      return 0;
    for (size_t i = 0; name[i]; ++i)
      {
        unsigned char c = (unsigned char)name[i];
        if (i == 128 || (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                           || (c >= '0' && c <= '9') || c == '_' || c == '-'
                           || c == '.')))
          return 0;
      }
    return 1;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_server_init (mulib_cdd_mcp_server_t *server,
                              const mulib_cdd_mcp_server_config_t *config)
  {
    /* Reserve space for the result envelope, identity, and echoed ID. */
    size_t budget = MULIB_CDD_MCP_MAX_FRAME - 11000;
    if (!server || !config || (!config->tools && config->tool_count)
        || !mulib_mcp_wire_cstr_ (config->name, 256) || !config->name[0]
        || !mulib_mcp_wire_cstr_ (config->version, 256) || !config->version[0])
      return MULIB_CDD_MCP_INVALID;
    if (config->tool_count > MULIB_CDD_MCP_MAX_TOOLS)
      return MULIB_CDD_MCP_LIMIT;
    for (size_t i = 0; i < config->tool_count; ++i)
      {
        const mulib_cdd_mcp_tool_t *t = &config->tools[i];
        if (!mulib_mcp_server_name_ (t->name) || !t->invoke
            || (t->description && !mulib_mcp_wire_cstr_ (t->description, 2048))
            || !mulib_mcp_wire_is_ (
                mulib_json_get (t->input_schema, "type"), "object"))
          return MULIB_CDD_MCP_INVALID;
        for (size_t j = 0; j < i; ++j)
          if (!strcmp (config->tools[j].name, t->name))
            return MULIB_CDD_MCP_CONFLICT;
        if (!mulib_mcp_wire_charge_ (&budget, 70)
            || !mulib_mcp_wire_charge_text_ (&budget, strlen (t->name))
            || (t->description && !mulib_mcp_wire_charge_text_ (
                                      &budget, strlen (t->description)))
            || !mulib_mcp_wire_measure_ (t->input_schema, &budget, 0))
          return MULIB_CDD_MCP_LIMIT;
      }
    server->config = *config;
    server->legacy_state = 0;
    return MULIB_CDD_MCP_OK;
  }

  static mulib_json_t *
  mulib_mcp_server_versions_ (const mulib_cdd_mcp_server_config_t *cfg)
  {
    mulib_json_t *a = mulib_json_new (MULIB_JSON_ARRAY);
    if (a && mulib_mcp_wire_append_ (
                 a, mulib_json_string (MULIB_CDD_MCP_VERSION))
        && (!cfg->allow_legacy || mulib_mcp_wire_append_ (
                                     a, mulib_json_string (
                                            MULIB_CDD_MCP_LEGACY_VERSION))))
      return a;
    mulib_json_destroy (a);
    return NULL;
  }

  static mulib_json_t *
  mulib_mcp_server_caps_ (const mulib_cdd_mcp_server_config_t *cfg,
                         const mulib_cdd_mcp_catalog_config_t *features)
  {
    mulib_json_t *caps = mulib_mcp_wire_object_ ();
    if (caps && (!(cfg->tool_count || (features && features->tool_count))
                 || mulib_mcp_wire_put_ (caps, "tools", mulib_mcp_wire_object_ ()))
        && (!features
            || ((!features->read || mulib_mcp_wire_put_ (
                                        caps, "resources", mulib_mcp_wire_object_ ()))
                && (!features->get || mulib_mcp_wire_put_ (
                                         caps, "prompts", mulib_mcp_wire_object_ ()))
                && (!features->complete || mulib_mcp_wire_put_ (
                                              caps, "completions", mulib_mcp_wire_object_ ())))))
      return caps;
    mulib_json_destroy (caps);
    return NULL;
  }

  static mulib_json_t *
  mulib_mcp_server_tool_ (const char *name, const char *description,
                         const mulib_json_t *input, const mulib_json_t *output)
  {
    mulib_json_t *tool = mulib_mcp_wire_object_ ();
    if (tool && mulib_mcp_wire_str_ (tool, "name", name)
        && (!description || mulib_mcp_wire_str_ (tool, "description", description))
        && mulib_mcp_wire_put_ (tool, "inputSchema", mulib_mcp_wire_clone_ (input))
        && (!output || mulib_mcp_wire_put_ (
                           tool, "outputSchema", mulib_mcp_wire_clone_ (output))))
      return tool;
    mulib_json_destroy (tool);
    return NULL;
  }

  static mulib_json_t *
  mulib_mcp_server_tool_at_ (const mulib_cdd_mcp_server_config_t *cfg,
                            const mulib_cdd_mcp_catalog_config_t *features,
                            size_t i)
  {
    if (i < cfg->tool_count)
      {
        const mulib_cdd_mcp_tool_t *t = &cfg->tools[i];
        return mulib_mcp_server_tool_ (t->name, t->description, t->input_schema, NULL);
      }
    const mulib_cdd_mcp_structured_tool_t *t = &features->tools[i - cfg->tool_count];
    return mulib_mcp_server_tool_ (t->name, t->description, t->input_schema,
                                  t->output_schema);
  }

  static int
  mulib_mcp_server_tool_budget_ (const char *name, const char *desc,
                                const mulib_json_t *input, const mulib_json_t *output,
                                size_t *budget)
  {
    return mulib_mcp_wire_charge_ (budget, 112)
           && mulib_mcp_wire_charge_text_ (budget, strlen (name))
           && (!desc || mulib_mcp_wire_charge_text_ (budget, strlen (desc)))
           && mulib_mcp_wire_measure_ (input, budget, 0)
           && (!output || mulib_mcp_wire_measure_ (output, budget, 0));
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_catalog_init (mulib_cdd_mcp_catalog_t *catalog,
                             const mulib_cdd_mcp_server_config_t *base,
                             const mulib_cdd_mcp_catalog_config_t *f)
  {
    mulib_cdd_mcp_server_t tmp;
    mulib_cdd_mcp_status_t status;
    if (!catalog || !f || (!f->tools && f->tool_count)
        || ((f->resources || f->resource_templates) && !f->read)
        || (!!f->prompts != !!f->get)
        || (f->complete && !f->prompts && !f->resource_templates && !f->resources))
      return MULIB_CDD_MCP_INVALID;
    status = mulib_cdd_mcp_server_init (&tmp, base);
    if (status != MULIB_CDD_MCP_OK)
      return status;
    if (f->tool_count > MULIB_CDD_MCP_MAX_TOOLS - base->tool_count
        || f->page_size > MULIB_CDD_MCP_MAX_ENTRIES)
      return MULIB_CDD_MCP_LIMIT;
    for (size_t i = 0; i < f->tool_count; ++i)
      {
        const mulib_cdd_mcp_structured_tool_t *t = &f->tools[i];
        size_t budget = MULIB_MCP_WIRE_PAYLOAD;
        if (!mulib_mcp_server_name_ (t->name) || !t->invoke || !t->validate
            || (t->description && !mulib_mcp_wire_cstr_ (t->description, 2048))
            || !mulib_mcp_wire_is_ (mulib_json_get (t->input_schema, "type"), "object")
            || (t->output_schema
                && (mulib_json_typeof (t->output_schema) != MULIB_JSON_OBJECT
                    || (base->allow_legacy && !mulib_mcp_wire_is_ (
                             mulib_json_get (t->output_schema, "type"), "object")))))
          return MULIB_CDD_MCP_INVALID;
        for (size_t j = 0; j < base->tool_count; ++j)
          if (!strcmp (base->tools[j].name, t->name))
            return MULIB_CDD_MCP_CONFLICT;
        for (size_t j = 0; j < i; ++j)
          if (!strcmp (f->tools[j].name, t->name))
            return MULIB_CDD_MCP_CONFLICT;
        if (!mulib_mcp_server_tool_budget_ (
                t->name, t->description, t->input_schema, t->output_schema, &budget))
          return MULIB_CDD_MCP_LIMIT;
      }
    {
      const mulib_json_t *lists[] = { f->resources, f->resource_templates, f->prompts };
      const char *keys[] = { "uri", "uriTemplate", "name" };
      for (unsigned k = 0; k < 3; ++k)
        {
          const mulib_json_t *list = lists[k];
          if (list && mulib_json_typeof (list) != MULIB_JSON_ARRAY)
            return MULIB_CDD_MCP_INVALID;
          if (mulib_json_size (list) > MULIB_CDD_MCP_MAX_ENTRIES)
            return MULIB_CDD_MCP_LIMIT;
          for (size_t i = 0; i < mulib_json_size (list); ++i)
            {
              const mulib_json_t *entry = mulib_json_at (list, i);
              const char *name;
              size_t budget = MULIB_MCP_WIRE_PAYLOAD;
              if (!mulib_mcp_wire_measure_ (entry, &budget, 0))
                return MULIB_CDD_MCP_LIMIT;
              if (!(k == 2 ? mulib_mcp_wire_prompt_ (entry)
                            : mulib_mcp_wire_resource_ (entry, k == 1)))
                return MULIB_CDD_MCP_INVALID;
              name = mulib_json_text (mulib_json_get (entry, keys[k]), NULL);
              for (size_t j = 0; j < i; ++j)
                if (mulib_mcp_wire_is_ (
                        mulib_json_get (mulib_json_at (list, j), keys[k]), name))
                  return MULIB_CDD_MCP_CONFLICT;
            }
        }
    }
    catalog->features = *f;
    catalog->server = tmp;
    return MULIB_CDD_MCP_OK;
  }

  static const mulib_json_t *
  mulib_mcp_server_find_ (const mulib_json_t *list, const char *key, const char *name)
  {
    for (size_t i = 0; i < mulib_json_size (list); ++i)
      {
        const mulib_json_t *entry = mulib_json_at (list, i);
        if (mulib_mcp_wire_is_ (mulib_json_get (entry, key), name))
          return entry;
      }
    return NULL;
  }

  static int
  mulib_mcp_server_prompt_args_ (const mulib_json_t *prompt, const mulib_json_t *args)
  {
    const mulib_json_t *defs = mulib_json_get (prompt, "arguments");
    if (!mulib_mcp_wire_string_map_ (args))
      return 0;
    for (size_t i = 0; i < mulib_json_size (defs); ++i)
      {
        const mulib_json_t *def = mulib_json_at (defs, i);
        if (mulib_json_boolean (mulib_json_get (def, "required"))
            && !mulib_json_get (args, mulib_json_text (mulib_json_get (def, "name"), NULL)))
          return 0;
      }
    for (size_t i = 0; i < mulib_json_size (args); ++i)
      if (!mulib_mcp_server_find_ (defs, "name", mulib_json_key (args, i, NULL)))
        return 0;
    return 1;
  }

  static void
  mulib_mcp_server_cursor_ (char out[96], const char *key, unsigned long long gen,
                            int modern, size_t index)
  {
    (void)snprintf (out, 96, "%s:%llx:%d:%zu", key, gen, modern, index);
  }

  static mulib_json_t *
  mulib_mcp_server_page_ (const mulib_cdd_mcp_server_config_t *cfg,
                         const mulib_cdd_mcp_catalog_config_t *f,
                         const mulib_cdd_mcp_request_view_t *rq, const char *key,
                         const mulib_json_t *entries, int *invalid)
  {
    const mulib_json_t *cursor = mulib_json_get (rq->params, "cursor");
    int tools = !strcmp (key, "tools");
    size_t n = tools ? cfg->tool_count + (f ? f->tool_count : 0)
                     : mulib_json_size (entries);
    size_t i = 0, limit = f ? (f->page_size ? f->page_size : 32) : n;
    size_t budget = MULIB_MCP_WIRE_PAYLOAD;
    mulib_json_t *out, *list;
    char token[96];
    if (cursor)
      {
        if (f && mulib_mcp_wire_label_ (cursor, sizeof token - 1))
          for (i = 1; i < n; ++i)
            {
              mulib_mcp_server_cursor_ (token, key, f->generation, rq->modern, i);
              if (mulib_mcp_wire_is_ (cursor, token))
                break;
            }
        if (!i || i >= n)
          {
            *invalid = 1;
            return NULL;
          }
      }
    out = mulib_mcp_wire_object_ ();
    list = mulib_json_new (MULIB_JSON_ARRAY);
    if (!out || !list)
      goto fail;
    for (size_t end = i + limit; i < n && i < end; ++i)
      {
        const mulib_json_t *entry = mulib_json_at (entries, i);
        if (f)
          {
            int fits;
            if (!tools)
              fits = mulib_mcp_wire_measure_ (entry, &budget, 0);
            else if (i < cfg->tool_count)
              {
                const mulib_cdd_mcp_tool_t *t = &cfg->tools[i];
                /* Same conservative charge as base init for individual entries. */
                fits = mulib_mcp_wire_charge_ (&budget, 70)
                       && mulib_mcp_wire_charge_text_ (&budget, strlen (t->name))
                       && (!t->description || mulib_mcp_wire_charge_text_ (
                                                &budget, strlen (t->description)))
                       && mulib_mcp_wire_measure_ (t->input_schema, &budget, 0);
              }
            else
              {
                const mulib_cdd_mcp_structured_tool_t *t = &f->tools[i - cfg->tool_count];
                fits = mulib_mcp_server_tool_budget_ (
                    t->name, t->description, t->input_schema, t->output_schema, &budget);
              }
            if (!fits)
              break;
          }
        if (!mulib_mcp_wire_append_ (list, tools ? mulib_mcp_server_tool_at_ (cfg, f, i)
                                                  : mulib_mcp_wire_clone_ (entry)))
          goto fail;
      }
    if (i < n)
      {
        mulib_mcp_server_cursor_ (token, key, f->generation, rq->modern, i);
        if (!mulib_mcp_wire_str_ (out, "nextCursor", token))
          goto fail;
      }
    if (mulib_json_set (out, key, list))
      return out;
  fail:
    mulib_json_destroy (list);
    mulib_json_destroy (out);
    return NULL;
  }

  static mulib_json_t *
  mulib_mcp_server_text_ (const char *text, size_t len)
  {
    mulib_json_t *a = mulib_json_new (MULIB_JSON_ARRAY);
    mulib_json_t *block = mulib_mcp_wire_object_ ();
    if (a && block && mulib_mcp_wire_str_ (block, "type", "text")
        && mulib_mcp_wire_put_ (block, "text", mulib_json_string_n (text, len))
        && mulib_json_append (a, block))
      return a;
    mulib_json_destroy (block);
    mulib_json_destroy (a);
    return NULL;
  }

  static mulib_json_t *
  mulib_mcp_server_error_ (const mulib_json_t *id, int code,
                             const char *message, mulib_json_t *data)
  {
    mulib_json_t *out = mulib_mcp_wire_object_ ();
    mulib_json_t *error = mulib_mcp_wire_object_ ();
    if (out && error && mulib_mcp_wire_str_ (out, "jsonrpc", "2.0")
        && (!id || mulib_mcp_wire_put_ (out, "id", mulib_mcp_wire_clone_ (id)))
        && mulib_mcp_wire_put_ (error, "code", mulib_json_number ((double)code))
        && mulib_mcp_wire_str_ (error, "message", message)
        && (!data || mulib_json_set (error, "data", data)))
      {
        data = NULL;
        if (mulib_json_set (out, "error", error))
          return out;
      }
    mulib_json_destroy (data);
    mulib_json_destroy (error);
    mulib_json_destroy (out);
    return NULL;
  }

  static mulib_json_t *
  mulib_mcp_server_bad_version_ (const mulib_cdd_mcp_server_config_t *cfg,
                                   const mulib_json_t *id,
                                   const mulib_json_t *version)
  {
    mulib_json_t *data = mulib_mcp_wire_object_ ();
    if (data && mulib_mcp_wire_put_ (data, "supported",
                                     mulib_mcp_server_versions_ (cfg))
        && mulib_mcp_wire_put_ (
                data, "requested", mulib_mcp_wire_clone_ (version)))
      return mulib_mcp_server_error_ (id, -32022,
                                      "Unsupported protocol version", data);
    mulib_json_destroy (data);
    return NULL;
  }

  static int
  mulib_mcp_server_modern_ (const mulib_cdd_mcp_server_config_t *cfg,
                           mulib_json_t *result, int cacheable, unsigned ttl)
  {
    mulib_json_t *meta = mulib_json_get (result, "_meta");
    if (!meta)
      {
        if (!mulib_mcp_wire_put_ (result, "_meta", mulib_mcp_wire_object_ ()))
          return 0;
        meta = mulib_json_get (result, "_meta");
      }
    return mulib_mcp_wire_put_ (meta, MULIB_MCP_WIRE_SERVER,
                               mulib_mcp_wire_info_ (cfg->name, cfg->version))
           && (mulib_json_get (result, "resultType")
               || mulib_mcp_wire_str_ (result, "resultType", "complete"))
           && (!cacheable || (mulib_mcp_wire_put_ (
                                   result, "ttlMs", mulib_json_number ((double)ttl))
                              && mulib_mcp_wire_str_ (
                                  result, "cacheScope", "private")));
  }

  static mulib_json_t *
  mulib_mcp_server_callback_ (const mulib_cdd_mcp_request_view_t *rq,
                              mulib_cdd_mcp_handler_fn fn, void *user,
                              const char *key,
                              const mulib_cdd_mcp_structured_tool_t *tool,
                              int *error)
  {
    mulib_json_t *v = NULL;
    size_t budget = MULIB_MCP_WIRE_PAYLOAD;
    mulib_cdd_mcp_handler_status_t rc = fn (user, rq, &v);
    if (rc == MULIB_CDD_MCP_HANDLER_OK && v
        && mulib_mcp_wire_measure_ (v, &budget, 0)
        && mulib_mcp_wire_payload_ (v, key, !rq->modern)
        && mulib_mcp_wire_meta_ (mulib_json_get (v, "_meta"))
        && !mulib_json_get (v, "resultType") && !mulib_json_get (v, "inputRequests")
        && !mulib_json_get (v, "requestState") && !mulib_json_get (v, "ttlMs")
        && !mulib_json_get (v, "cacheScope")
        && (!tool || !tool->output_schema
            || (mulib_json_boolean (mulib_json_get (v, "isError"))
                && !mulib_json_get (v, "structuredContent"))
            || (mulib_json_get (v, "structuredContent")
                && tool->validate (tool->user, tool->output_schema,
                                   mulib_json_get (v, "structuredContent")))))
      return v;
    if (rc == MULIB_CDD_MCP_HANDLER_DEFER && rq->modern && strcmp (key, "completion")
        && mulib_mcp_wire_string_ (v) && mulib_mcp_wire_measure_ (v, &budget, 0))
      {
        mulib_json_t *result = mulib_mcp_wire_object_ ();
        if (result && mulib_mcp_wire_str_ (result, "resultType", "input_required")
            && mulib_json_set (result, "requestState", v))
          return result;
        mulib_json_destroy (result);
        mulib_json_destroy (v);
        return NULL;
      }
    mulib_json_destroy (v);
    *error = rc == MULIB_CDD_MCP_HANDLER_INVALID ? -32602 : -32603;
    return NULL;
  }

  /* Unless noted otherwise, NULL means local allocation failure. */
  static mulib_json_t *
  mulib_mcp_server_dispatch_ (const mulib_cdd_mcp_server_t *server,
                             const mulib_cdd_mcp_catalog_config_t *f,
                             const void *principal, const mulib_json_t *req,
                             unsigned *state, int *silent)
  {
    const mulib_cdd_mcp_server_config_t *cfg = &server->config;
    const mulib_json_t *id = NULL;
    const mulib_json_t *method, *params, *meta, *version, *caps, *info;
    mulib_json_t *result = NULL, *out;
    int modern = 0, cacheable = 0, callback_error = 0, invalid_page = 0;
    mulib_cdd_mcp_request_view_t rq = {
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, principal, 0
    };
    const char *list_key = NULL;
    const mulib_json_t *entries = NULL;
    if (mulib_json_typeof (req) != MULIB_JSON_OBJECT)
      goto invalid_request;
    id = mulib_json_get (req, "id");
    method = mulib_json_get (req, "method");
    params = mulib_json_get (req, "params");
    if (!mulib_mcp_wire_is_ (mulib_json_get (req, "jsonrpc"), "2.0")
        || !(rq.method = mulib_mcp_wire_label_ (method, 256))
        || (id && !mulib_mcp_wire_id_ (id))
        || mulib_json_get (req, "result") || mulib_json_get (req, "error"))
      goto invalid_request;
    if (!id)
      {
        const mulib_json_t *notification_meta = mulib_json_get (params, "_meta");
        /* Only this legacy notification changes state. Requests without IDs
           cannot invoke a tool or change either era's negotiation. */
        if (*state == 1 && mulib_mcp_wire_is_ (method, "notifications/initialized")
            && (!params || mulib_json_typeof (params) == MULIB_JSON_OBJECT)
            && (!notification_meta
                || (mulib_json_typeof (notification_meta) == MULIB_JSON_OBJECT
                    && !mulib_json_get (notification_meta, MULIB_MCP_WIRE_VERSION)
                    && !mulib_json_get (notification_meta, MULIB_MCP_WIRE_CAPS)
                    && !mulib_json_get (notification_meta, MULIB_MCP_WIRE_CLIENT))))
          *state = 2;
        *silent = 1;
        return NULL;
      }
    if (params && mulib_json_typeof (params) != MULIB_JSON_OBJECT)
      goto invalid_params;
    meta = mulib_json_get (params, "_meta");
    if (!mulib_mcp_wire_request_meta_ (meta))
      goto invalid_params;
    version = mulib_json_get (meta, MULIB_MCP_WIRE_VERSION);
    caps = mulib_json_get (meta, MULIB_MCP_WIRE_CAPS);
    info = mulib_json_get (meta, MULIB_MCP_WIRE_CLIENT);
    if (version || caps || info || mulib_mcp_wire_is_ (method, "server/discover"))
      {
        modern = 1;
        if (!mulib_mcp_wire_label_ (version, 128))
          goto invalid_params;
        if (!mulib_mcp_wire_is_ (version, MULIB_CDD_MCP_VERSION))
          return mulib_mcp_server_bad_version_ (cfg, id, version);
        if (mulib_json_typeof (caps) != MULIB_JSON_OBJECT
            || (info && !mulib_mcp_wire_info_valid_ (info)))
          goto invalid_params;
      }
    rq.modern = modern;
    rq.id = id;
    rq.params = params;
    rq.meta = meta;
    rq.arguments = mulib_json_get (params, "arguments");
    rq.request_state = mulib_json_get (params, "requestState");
    if (mulib_json_get (params, "inputResponses")
        || (rq.request_state
            && (!modern || !f || !mulib_mcp_wire_string_ (rq.request_state)
                || (strcmp (rq.method, "resources/read") && strcmp (rq.method, "prompts/get")
                    && strcmp (rq.method, "tools/call")))))
      goto invalid_params;
    if (!strcmp (rq.method, "resources/read"))
      rq.target = mulib_mcp_wire_label_ (mulib_json_get (params, "uri"), 4096);
    else if (!strcmp (rq.method, "prompts/get") || !strcmp (rq.method, "tools/call"))
      rq.target = mulib_mcp_wire_label_ (mulib_json_get (params, "name"), 4096);
    else if (!strcmp (rq.method, "completion/complete"))
      {
        const mulib_json_t *ref = mulib_json_get (params, "ref");
        rq.target = mulib_mcp_wire_label_ (mulib_json_get (
            ref, mulib_mcp_wire_is_ (mulib_json_get (ref, "type"), "ref/prompt") ? "name" : "uri"),
            4096);
      }
    if (f && f->authorize && !f->authorize (f->user, &rq))
      return mulib_mcp_server_error_ (id, -32000, "Request denied", NULL);
    if (!modern && mulib_mcp_wire_is_ (method, "initialize"))
      {
        if (!cfg->allow_legacy)
          return mulib_mcp_server_error_ (
              id, -32601, "Use server/discover with MCP " MULIB_CDD_MCP_VERSION,
              NULL);
        if (*state)
          return mulib_mcp_server_error_ (id, -32600, "Already initialized", NULL);
        if (mulib_json_typeof (mulib_json_get (params, "protocolVersion"))
                != MULIB_JSON_STRING
            || mulib_json_typeof (mulib_json_get (params, "capabilities"))
                   != MULIB_JSON_OBJECT
            || !mulib_mcp_wire_info_valid_ (mulib_json_get (params, "clientInfo")))
          goto invalid_params;
        result = mulib_mcp_wire_object_ ();
        if (!result || !mulib_mcp_wire_str_ (result, "protocolVersion",
                                             MULIB_CDD_MCP_LEGACY_VERSION)
            || !mulib_mcp_wire_put_ (result, "capabilities",
                                      mulib_mcp_server_caps_ (cfg, f))
            || !mulib_mcp_wire_put_ (result, "serverInfo",
                                     mulib_mcp_wire_info_ (cfg->name, cfg->version)))
          goto fail;
        *state = 1;
      }
    else if (modern && mulib_mcp_wire_is_ (method, "server/discover"))
      {
        result = mulib_mcp_wire_object_ ();
        if (!result || !mulib_mcp_wire_put_ (result, "supportedVersions",
                                             mulib_mcp_server_versions_ (cfg))
            || !mulib_mcp_wire_put_ (result, "capabilities",
                                      mulib_mcp_server_caps_ (cfg, f)))
          goto fail;
        cacheable = 1;
      }
    else if (!modern && cfg->allow_legacy && mulib_mcp_wire_is_ (method, "ping"))
      result = mulib_mcp_wire_object_ ();
    else if (!modern && *state != 2)
      goto invalid_params;
    else if ((cfg->tool_count || (f && f->tool_count)) && !strcmp (rq.method, "tools/list"))
      list_key = "tools";
    else if (f && f->read && !strcmp (rq.method, "resources/list"))
      list_key = "resources", entries = f->resources;
    else if (f && f->read && !strcmp (rq.method, "resources/templates/list"))
      list_key = "resourceTemplates", entries = f->resource_templates;
    else if (f && f->get && !strcmp (rq.method, "prompts/list"))
      list_key = "prompts", entries = f->prompts;
    else if (f && f->read && !strcmp (rq.method, "resources/read"))
      {
        if (!mulib_mcp_wire_uri_ (mulib_json_get (params, "uri")))
          goto invalid_params;
        result = mulib_mcp_server_callback_ (&rq, f->read, f->user, "contents",
                                             NULL, &callback_error);
        cacheable = !mulib_json_get (result, "resultType");
      }
    else if (f && f->get && !strcmp (rq.method, "prompts/get"))
      {
        const mulib_json_t *p = rq.target ? mulib_mcp_server_find_ (
            f->prompts, "name", rq.target) : NULL;
        if (!p || !mulib_mcp_server_prompt_args_ (p, rq.arguments))
          goto invalid_params;
        result = mulib_mcp_server_callback_ (&rq, f->get, f->user, "messages",
                                             NULL, &callback_error);
      }
    else if (f && f->complete && !strcmp (rq.method, "completion/complete"))
      {
        const mulib_json_t *ref = mulib_json_get (params, "ref");
        if (!mulib_mcp_wire_complete_args_ (params) || !rq.target)
          goto invalid_params;
        if (mulib_mcp_wire_is_ (mulib_json_get (ref, "type"), "ref/prompt"))
          {
            const mulib_json_t *p = mulib_mcp_server_find_ (f->prompts, "name", rq.target);
            const mulib_json_t *defs = mulib_json_get (p, "arguments");
            const mulib_json_t *ctx = mulib_json_get (mulib_json_get (params, "context"), "arguments");
            const char *name = mulib_json_text (mulib_json_get (
                mulib_json_get (params, "argument"), "name"), NULL);
            if (!mulib_mcp_server_find_ (defs, "name", name))
              goto invalid_params;
            for (size_t i = 0; i < mulib_json_size (ctx); ++i)
              if (!mulib_mcp_server_find_ (defs, "name", mulib_json_key (ctx, i, NULL)))
                goto invalid_params;
          }
        else if (!mulib_mcp_server_find_ (f->resource_templates, "uriTemplate", rq.target)
                 && !mulib_mcp_server_find_ (f->resources, "uri", rq.target))
          goto invalid_params;
        result = mulib_mcp_server_callback_ (&rq, f->complete, f->user, "completion",
                                             NULL, &callback_error);
      }
    else if ((cfg->tool_count || (f && f->tool_count)) && !strcmp (rq.method, "tools/call"))
      {
        const mulib_json_t *args = rq.arguments;
        const mulib_cdd_mcp_tool_t *tool = NULL;
        const mulib_cdd_mcp_structured_tool_t *structured = NULL;
        if (!rq.target || !mulib_mcp_server_name_ (rq.target)
            || (args && mulib_json_typeof (args) != MULIB_JSON_OBJECT)
            || mulib_json_get (params, "task"))
          goto invalid_params;
        for (size_t i = 0; i < cfg->tool_count; ++i)
          if (!strcmp (rq.target, cfg->tools[i].name))
            {
              tool = &cfg->tools[i];
              break;
            }
        for (size_t i = 0; !tool && f && i < f->tool_count; ++i)
          if (!strcmp (rq.target, f->tools[i].name))
            {
              structured = &f->tools[i];
              break;
            }
        if (structured)
          {
            if (!structured->validate (structured->user, structured->input_schema, args))
              goto invalid_params;
            result = mulib_mcp_server_callback_ (&rq, structured->invoke, structured->user,
                                                 "content", structured, &callback_error);
          }
        else if (tool && !rq.request_state)
          {
            char text[MULIB_CDD_MCP_MAX_TEXT] = { 0 };
            size_t n = 0;
            mulib_cdd_mcp_tool_status_t code;
            code = tool->invoke (tool->user, args, text, sizeof text, &n);
            if ((code != MULIB_CDD_MCP_TOOL_OK && code != MULIB_CDD_MCP_TOOL_ERROR)
                || n > sizeof text
                || !mulib_utf8_validate ((const unsigned char *)text, n))
              return mulib_mcp_server_error_ (id, -32603, "Tool callback failed", NULL);
            result = mulib_mcp_wire_object_ ();
            if (!result || !mulib_mcp_wire_put_ (result, "content",
                                                 mulib_mcp_server_text_ (text, n))
                || (code == MULIB_CDD_MCP_TOOL_ERROR
                    && !mulib_mcp_wire_put_ (result, "isError", mulib_json_bool (1))))
              goto fail;
          }
        else
          goto invalid_params;
      }
    else
      return mulib_mcp_server_error_ (id, -32601, "Method not found", NULL);
    if (list_key)
      {
        result = mulib_mcp_server_page_ (cfg, f, &rq, list_key, entries, &invalid_page);
        cacheable = 1;
        if (invalid_page)
          goto invalid_params;
      }
    if (callback_error)
      return mulib_mcp_server_error_ (
          id, callback_error, callback_error == -32602 ? "Invalid target or arguments"
                                                       : "Handler failed", NULL);
    if (!result || (modern && !mulib_mcp_server_modern_ (cfg, result, cacheable,
                                                        f && !rq.request_state
                                                            ? f->cache_ttl_ms : 0)))
      goto fail;
    out = mulib_mcp_wire_object_ ();
    if (out && mulib_mcp_wire_str_ (out, "jsonrpc", "2.0")
        && mulib_mcp_wire_put_ (out, "id", mulib_mcp_wire_clone_ (id))
        && mulib_json_set (out, "result", result))
      return out;
    mulib_json_destroy (out);
  fail:
    mulib_json_destroy (result);
    return NULL;
  invalid_params:
    return mulib_mcp_server_error_ (id, -32602, "Invalid params or protocol state",
                                    NULL);
  invalid_request:
    return mulib_mcp_server_error_ (mulib_mcp_wire_id_ (id) ? id : NULL,
                                    -32600, "Invalid Request", NULL);
  }

  static mulib_cdd_mcp_status_t
  mulib_mcp_server_process_ (mulib_cdd_mcp_server_t *server,
                             const mulib_cdd_mcp_catalog_config_t *f,
                             const void *principal, const char *frame,
                             size_t length, char **out)
  {
    mulib_json_t *req, *res;
    mulib_json_error_t error = { 0, 0, 0, NULL };
    mulib_cdd_mcp_status_t status;
    unsigned state;
    int silent = 0;
    if (!server || !server->config.name || !out)
      return MULIB_CDD_MCP_INVALID;
    status = mulib_mcp_wire_frame_ (frame, length);
    if (status != MULIB_CDD_MCP_OK)
      return status;
    state = server->legacy_state;
    req = mulib_mcp_wire_parse_ (frame, length, &error);
    if (!req)
      {
        if (mulib_mcp_wire_parse_nomem_ (&error))
          return MULIB_CDD_MCP_NOMEM;
        res = mulib_mcp_server_error_ (NULL, -32700, "Parse error", NULL);
      }
    else
      {
        res = mulib_mcp_server_dispatch_ (server, f, principal, req, &state, &silent);
        mulib_json_destroy (req);
      }
    if (silent)
      *out = NULL;
    else
      {
        if (!res)
          return MULIB_CDD_MCP_NOMEM;
        status = mulib_mcp_wire_format_ (res, out);
        mulib_json_destroy (res);
        if (status != MULIB_CDD_MCP_OK)
          return status;
      }
    server->legacy_state = state;
    return MULIB_CDD_MCP_OK;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_server_process (mulib_cdd_mcp_server_t *server, const char *frame,
                                size_t length, char **out)
  {
    return mulib_mcp_server_process_ (server, NULL, NULL, frame, length, out);
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_catalog_process (mulib_cdd_mcp_catalog_t *catalog,
                                 const void *principal, const char *frame,
                                 size_t length, char **out)
  {
    if (!catalog)
      return MULIB_CDD_MCP_INVALID;
    return mulib_mcp_server_process_ (
        &catalog->server, &catalog->features, principal, frame, length, out);
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
