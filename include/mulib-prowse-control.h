/** @file mulib-prowse-control.h
 * @brief Browser automation, scraping, lprowse Lua DSL and highlighted interactive REPL.
 * Define MULIB_IMPL_PROWSE_CONTROL plus PROWSE_PROTOCOL, MUNE, READLINE and all
 * transitive switches. The embedded Lua library also ships in libraries/lprowse. */
#ifndef MULIB_PROWSE_CONTROL_H
#define MULIB_PROWSE_CONTROL_H
#include "mulib-prowse-protocol.h"
#include "mulib-mune.h"
#include "mulib-readline.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_prowse_control mulib_prowse_control_t;
typedef struct mulib_prowse_control_options
{
  size_t script_bytes, query_items;
  unsigned wait_ms, poll_ms;
  mulib_mune_runtime_options_t lua;
} mulib_prowse_control_options_t;
/** @brief Create a controller borrowing an optional protocol client. Defaults:
 * 1 MiB source, 1000 extracted nodes, 30-second max wait, 50-ms polling. */
mulib_prowse_control_t *mulib_prowse_control_create (mulib_prowse_protocol_t *, const mulib_prowse_control_options_t *);
/** @brief Free Lua/editor state and owned connections. Does not implicitly quit
 * a remote session. No callback or script may destroy its executing controller. */
void mulib_prowse_control_destroy (mulib_prowse_control_t *);
/** @brief Connect/replace an inactive controller's transport, owning the new client. */
int mulib_prowse_control_connect (mulib_prowse_control_t *, mulib_prowse_protocol_kind_t,
    const char *endpoint, const mulib_prowse_protocol_options_t *, const mulib_prowse_io_t *);
/** @brief Forget the session and release an owned connection (borrowed clients
 * remain caller-owned). No remote command; permits reconnecting after failure. */
void mulib_prowse_control_disconnect (mulib_prowse_control_t *);
/** @brief Create WebDriver/Marionette session from alwaysMatch capabilities, or
 * enable CDP Page/Runtime on the connected page target. NULL capabilities is {}. */
int mulib_prowse_control_start (mulib_prowse_control_t *, const mulib_json_t *capabilities);
/** @brief Attach an existing session ID without sending a request; CDP uses the
 * flattened session ID and NULL selects the already-connected page. */
int mulib_prowse_control_attach (mulib_prowse_control_t *, const char *session_id);
/** @brief Delete a driver session; CDP disables Runtime/Page without closing browser. */
int mulib_prowse_control_quit (mulib_prowse_control_t *);
/** @brief Send navigation. Completion semantics follow the selected protocol;
 * CDP acknowledgement precedes document load. Use wait for dynamic content. */
int mulib_prowse_control_navigate (mulib_prowse_control_t *, const char *url);
/** @brief Evaluate one JavaScript expression by value. *value must be NULL;
 * success transfers JSON ownership. Exceptions return REMOTE and a diagnostic. */
int mulib_prowse_control_evaluate (mulib_prowse_control_t *, const char *expression, mulib_json_t **value);
/** @brief Click a CSS-selected element using driver interaction or CDP mouse events. */
int mulib_prowse_control_click (mulib_prowse_control_t *, const char *selector);
/** @brief Focus/send text to a CSS-selected element (does not clear existing text). */
int mulib_prowse_control_type (mulib_prowse_control_t *, const char *selector, const char *text);
/** @brief Extract text/html/href/attribute from one/all CSS matches, bounded by
 * query_items. A missing single match returns JSON null; all returns an array. */
int mulib_prowse_control_query (mulib_prowse_control_t *, const char *selector,
    const char *property, const char *attribute, int all, mulib_json_t **value);
/** @brief Capture a base64 PNG string, owned as a JSON string. */
int mulib_prowse_control_screenshot (mulib_prowse_control_t *, mulib_json_t **value);
/** @brief Wait for a CSS match, up to min(timeout, configured wait_ms). */
int mulib_prowse_control_wait (mulib_prowse_control_t *, const char *selector, unsigned timeout_ms);
/** @brief Latest operation's diagnostic, borrowed until another operation. */
const char *mulib_prowse_control_error (const mulib_prowse_control_t *);
/** @brief Lazily initialize Mune with require('lprowse'), globals lprowse/browser,
 * and native bindings. Borrow the state for additional application extensions. */
mulib_mune_state_t *mulib_prowse_control_lua (mulib_prowse_control_t *);
/** @brief Execute persistent Lua source; result follows Mune ownership rules. */
mulib_mune_status_t mulib_prowse_control_run (mulib_prowse_control_t *, const char *, size_t,
    mulib_mune_result_t **result, mulib_mune_error_t *error);
/** @brief Borrow/configure the editor, initialized with Lua keywords and lprowse
 * completions/directives. Use readline_load to add language/application profiles. */
mulib_readline_t *mulib_prowse_control_editor (mulib_prowse_control_t *);
/** @brief Run a persistent REPL on descriptors. =expr prints a value; :begin/:end
 * delimit multiline Lua, :load executes a file, :help/:quit control the REPL.
 * User-defined expansion directives execute Lua. Action directives are exposed
 * through readline_dispatch for custom hosts. Returns prowse/readline error. */
int mulib_prowse_control_repl (mulib_prowse_control_t *, int input_fd, int output_fd);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PROWSE_CONTROL) && !defined(MULIB_PROWSE_CONTROL_IMPLEMENTED)
#define MULIB_PROWSE_CONTROL_IMPLEMENTED
#include "include-mulib-lprowse.h"
#include "mulib-strbuf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#if !defined(__STDC_NO_THREADS__)
#include <threads.h>
#endif
struct mulib_prowse_control
{
  mulib_prowse_protocol_t *protocol;
  mulib_prowse_control_options_t options;
  mulib_mune_state_t *lua;
  mulib_readline_t *editor;
  char session[256], diagnostic[512];
  int owns_protocol, active, executing;
};
static int mulib_prowse_control_set_ (mulib_json_t *o, const char *key, mulib_json_t *v)
{ if (v && mulib_json_set (o, key, v)) return 1; mulib_json_destroy (v); return 0; }
static mulib_json_t *mulib_prowse_control_clone_ (const mulib_json_t *v)
{
  char *text = mulib_json_stringify (v, 0);
  mulib_json_t *copy = text ? mulib_json_parse (text, strlen (text), NULL) : NULL;
  free (text); return copy;
}
static int mulib_prowse_control_error_ (mulib_prowse_control_t *c, int rc, const mulib_prowse_reply_t *r)
{
  const char *text = r ? mulib_json_text (mulib_json_get (r->value, "message"), NULL) : NULL;
  if (c) (void)snprintf (c->diagnostic, sizeof (c->diagnostic), "%s", rc ? text ? text : mulib_prowse_strerror (rc) : "");
  return rc;
}
mulib_prowse_control_t *mulib_prowse_control_create (mulib_prowse_protocol_t *p, const mulib_prowse_control_options_t *options)
{
  mulib_prowse_control_t *c = (mulib_prowse_control_t *)calloc (1, sizeof (*c));
  if (!c) return NULL;
  c->protocol = p;
  if (options) c->options = *options;
  if (!c->options.script_bytes) c->options.script_bytes = 1048576;
  if (!c->options.query_items) c->options.query_items = 1000;
  if (!c->options.wait_ms) c->options.wait_ms = 30000;
  if (!c->options.poll_ms) c->options.poll_ms = 50;
  if (c->options.script_bytes > 1048576 || c->options.query_items > 1000000 || c->options.wait_ms > 3600000)
    { free (c); return NULL; }
  return c;
}
void mulib_prowse_control_destroy (mulib_prowse_control_t *c)
{
  if (!c || c->executing) return;
  mulib_mune_state_destroy (c->lua); mulib_readline_destroy (c->editor);
  if (c->owns_protocol) mulib_prowse_protocol_destroy (c->protocol);
  free (c);
}
const char *mulib_prowse_control_error (const mulib_prowse_control_t *c)
{ return c ? c->diagnostic : "invalid controller"; }
void mulib_prowse_control_disconnect (mulib_prowse_control_t *c)
{
  if (!c) return;
  if (c->owns_protocol) mulib_prowse_protocol_destroy (c->protocol);
  c->protocol = NULL; c->owns_protocol = c->active = 0; c->session[0] = 0;
}
int mulib_prowse_control_connect (mulib_prowse_control_t *c, mulib_prowse_protocol_kind_t kind,
    const char *endpoint, const mulib_prowse_protocol_options_t *options, const mulib_prowse_io_t *io)
{
  mulib_prowse_protocol_t *p;
  int rc;
  if (!c) return MULIB_PROWSE_INVALID;
  if (c->active) return mulib_prowse_control_error_ (c, MULIB_PROWSE_STATE, NULL);
  p = mulib_prowse_protocol_connect (kind, endpoint, options, io, &rc);
  if (!p) return mulib_prowse_control_error_ (c, rc, NULL);
  if (c->owns_protocol) mulib_prowse_protocol_destroy (c->protocol);
  c->protocol = p; c->owns_protocol = 1; c->session[0] = 0;
  return mulib_prowse_control_error_ (c, 0, NULL);
}
static int mulib_prowse_control_path_ (const char *id, const char *suffix, char *out, size_t cap)
{
  static const char hex[] = "0123456789ABCDEF";
  size_t at = 9, i;
  if (cap < 10) return MULIB_PROWSE_LIMIT;
  memcpy (out, "/session/", 9);
  for (i = 0; id[i]; ++i)
    {
      unsigned char ch = (unsigned char)id[i];
      int plain = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || strchr ("-._~", ch);
      if (cap - at <= (plain ? 1u : 3u)) return MULIB_PROWSE_LIMIT;
      if (plain) out[at++] = (char)ch;
      else { out[at++] = '%'; out[at++] = hex[ch >> 4]; out[at++] = hex[ch & 15]; }
    }
  if (strlen (suffix) >= cap - at) return MULIB_PROWSE_LIMIT;
  strcpy (out + at, suffix); return 0;
}
static int mulib_prowse_control_request_ (mulib_prowse_control_t *c, const char *http,
    const char *path, const char *marionette, const char *cdp, const mulib_json_t *params, mulib_prowse_reply_t *reply)
{
  int rc;
  if (!c || !c->protocol || !c->active) return mulib_prowse_control_error_ (c, MULIB_PROWSE_STATE, NULL);
  if (mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_WEBDRIVER)
    {
      char target[2048];
      rc = mulib_prowse_control_path_ (c->session, path, target, sizeof (target));
      if (!rc) rc = mulib_prowse_protocol_http (c->protocol, http, target, params, reply);
    }
  else rc = mulib_prowse_protocol_call (c->protocol,
      mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_CDP ? cdp : marionette, params, reply);
  if (!rc && mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_MARIONETTE)
    {
      const mulib_json_t *value = mulib_json_get (reply->value, "value");
      if (value) reply->value = value;
    }
  return mulib_prowse_control_error_ (c, rc, reply);
}
int mulib_prowse_control_attach (mulib_prowse_control_t *c, const char *id)
{
  size_t n = id ? strlen (id) : 0;
  int rc = 0;
  if (!c || !c->protocol || c->active || n >= sizeof (c->session)) return MULIB_PROWSE_STATE;
  if (mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_CDP) rc = mulib_prowse_protocol_session (c->protocol, id);
  else if (!n) return MULIB_PROWSE_INVALID;
  if (!rc) { if (n) memcpy (c->session, id, n); c->session[n] = 0; c->active = 1; }
  return mulib_prowse_control_error_ (c, rc, NULL);
}
int mulib_prowse_control_start (mulib_prowse_control_t *c, const mulib_json_t *caps)
{
  mulib_prowse_reply_t reply = { 0 };
  mulib_json_t *params = NULL, *capabilities = NULL, *copy = NULL;
  int rc = 0;
  if (!c || !c->protocol || c->active || (caps && mulib_json_typeof (caps) != MULIB_JSON_OBJECT)) return MULIB_PROWSE_STATE;
  if (mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_CDP)
    {
      rc = mulib_prowse_protocol_call (c->protocol, "Page.enable", NULL, &reply);
      mulib_prowse_control_error_ (c, rc, &reply); mulib_prowse_reply_free (&reply);
      if (rc) return rc;
      rc = mulib_prowse_protocol_call (c->protocol, "Runtime.enable", NULL, &reply);
    }
  else
    {
      params = mulib_json_new (MULIB_JSON_OBJECT); capabilities = mulib_json_new (MULIB_JSON_OBJECT);
      copy = caps ? mulib_prowse_control_clone_ (caps) : mulib_json_new (MULIB_JSON_OBJECT);
      if (!params || !capabilities || !copy) { rc = MULIB_PROWSE_MEMORY; goto done; }
      if (!mulib_json_set (capabilities, "alwaysMatch", copy)) { rc = MULIB_PROWSE_MEMORY; goto done; } copy = NULL;
      if (!mulib_json_set (params, "capabilities", capabilities)) { rc = MULIB_PROWSE_MEMORY; goto done; } capabilities = NULL;
      rc = mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_WEBDRIVER
          ? mulib_prowse_protocol_http (c->protocol, "POST", "/session", params, &reply)
          : mulib_prowse_protocol_call (c->protocol, "WebDriver:NewSession", caps, &reply);
      if (!rc)
        {
          size_t n;
          const char *id = mulib_json_text (mulib_json_get (reply.value, "sessionId"), &n);
          if (!id || !n || n >= sizeof (c->session) || memchr (id, 0, n)) rc = MULIB_PROWSE_FORMAT;
          else { memcpy (c->session, id, n); c->session[n] = 0; }
        }
    }
  if (!rc) c->active = 1;
done:
  mulib_prowse_control_error_ (c, rc, &reply); mulib_prowse_reply_free (&reply);
  mulib_json_destroy (params); mulib_json_destroy (capabilities); mulib_json_destroy (copy); return rc;
}
int mulib_prowse_control_quit (mulib_prowse_control_t *c)
{
  mulib_prowse_reply_t r = { 0 };
  int rc;
  if (!c) return MULIB_PROWSE_INVALID;
  if (!c->active) return 0;
  rc = mulib_prowse_control_request_ (c, "DELETE", "", "WebDriver:DeleteSession", "Runtime.disable", NULL, &r);
  mulib_prowse_reply_free (&r);
  if (!rc && mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_CDP)
    { rc = mulib_prowse_control_request_ (c, "DELETE", "", "WebDriver:DeleteSession", "Page.disable", NULL, &r); mulib_prowse_reply_free (&r); }
  if (!rc) { c->active = 0; c->session[0] = 0; }
  return rc;
}
int mulib_prowse_control_navigate (mulib_prowse_control_t *c, const char *url)
{
  mulib_json_t *params;
  mulib_prowse_reply_t r = { 0 };
  int rc;
  if (!c || !url) return MULIB_PROWSE_INVALID;
  params = mulib_json_new (MULIB_JSON_OBJECT);
  if (!params || !mulib_prowse_control_set_ (params, "url", mulib_json_string (url))) { mulib_json_destroy (params); return MULIB_PROWSE_MEMORY; }
  rc = mulib_prowse_control_request_ (c, "POST", "/url", "WebDriver:Navigate", "Page.navigate", params, &r);
  if (!rc && mulib_json_typeof (mulib_json_get (r.value, "errorText")) == MULIB_JSON_STRING)
    { rc = MULIB_PROWSE_REMOTE; (void)snprintf (c->diagnostic, sizeof (c->diagnostic), "%s", mulib_json_text (mulib_json_get (r.value, "errorText"), NULL)); }
  mulib_prowse_reply_free (&r); mulib_json_destroy (params); return rc;
}
int mulib_prowse_control_evaluate (mulib_prowse_control_t *c, const char *expression, mulib_json_t **out)
{
  mulib_json_t *params, *value = NULL;
  mulib_prowse_reply_t r = { 0 };
  mulib_strbuf_t script;
  int rc = 0, cdp;
  if (!c || !expression || !out || *out) return MULIB_PROWSE_INVALID;
  if (strlen (expression) > c->options.script_bytes) return MULIB_PROWSE_LIMIT;
  cdp = mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_CDP;
  params = mulib_json_new (MULIB_JSON_OBJECT); mulib_strbuf_init (&script);
  if (!params) return MULIB_PROWSE_MEMORY;
  if (cdp)
    {
      if (!mulib_prowse_control_set_ (params, "expression", mulib_json_string (expression))
          || !mulib_prowse_control_set_ (params, "returnByValue", mulib_json_bool (1))
          || !mulib_prowse_control_set_ (params, "awaitPromise", mulib_json_bool (1))) rc = MULIB_PROWSE_MEMORY;
    }
  else if (!mulib_strbuf_append (&script, "return (") || !mulib_strbuf_append (&script, expression)
      || !mulib_strbuf_append (&script, "\n);")
      || !mulib_prowse_control_set_ (params, "script", mulib_json_string (script.data))
      || !mulib_prowse_control_set_ (params, "args", mulib_json_new (MULIB_JSON_ARRAY))) rc = MULIB_PROWSE_MEMORY;
  if (!rc) rc = mulib_prowse_control_request_ (c, "POST", "/execute/sync", "WebDriver:ExecuteScript", "Runtime.evaluate", params, &r);
  if (!rc)
    {
      const mulib_json_t *v = r.value;
      if (cdp)
        {
          const mulib_json_t *remote = mulib_json_get (v, "result");
          if (mulib_json_get (v, "exceptionDetails"))
            {
              const char *message = mulib_json_text (mulib_json_get (remote, "description"), NULL);
              rc = MULIB_PROWSE_REMOTE; (void)snprintf (c->diagnostic, sizeof (c->diagnostic), "%s", message ? message : "JavaScript exception");
            }
          else if (mulib_json_typeof (remote) != MULIB_JSON_OBJECT) rc = MULIB_PROWSE_FORMAT;
          else if (mulib_json_get (remote, "unserializableValue") || mulib_json_get (remote, "objectId")) rc = MULIB_PROWSE_UNSUPPORTED;
          v = mulib_json_get (remote, "value");
        }
      if (!rc) { value = v ? mulib_prowse_control_clone_ (v) : mulib_json_new (MULIB_JSON_NULL); if (!value) rc = MULIB_PROWSE_MEMORY; }
    }
  if (!rc) *out = value;
  else if (rc != MULIB_PROWSE_REMOTE) mulib_prowse_control_error_ (c, rc, &r);
  mulib_prowse_reply_free (&r); mulib_json_destroy (params); mulib_strbuf_free (&script); return rc;
}
static char *mulib_prowse_control_quote_ (const char *s)
{
  mulib_json_t *v = mulib_json_string (s);
  char *text = v ? mulib_json_stringify (v, 0) : NULL;
  mulib_json_destroy (v); return text;
}
int mulib_prowse_control_query (mulib_prowse_control_t *c, const char *selector,
    const char *property, const char *attribute, int all, mulib_json_t **out)
{
  char *q, *a = NULL, count[32];
  const char *extract;
  mulib_strbuf_t code;
  int rc;
  if (!c || !selector || !property || !out || *out) return MULIB_PROWSE_INVALID;
  if (!strcmp (property, "text")) extract = "(e.innerText===undefined?e.textContent:e.innerText)";
  else if (!strcmp (property, "html")) extract = "e.outerHTML";
  else if (!strcmp (property, "href")) extract = "e.href";
  else if (!strcmp (property, "attribute") && attribute) extract = NULL;
  else return MULIB_PROWSE_INVALID;
  q = mulib_prowse_control_quote_ (selector);
  if (!extract) a = mulib_prowse_control_quote_ (attribute);
  if (!q || (!extract && !a)) { free (q); free (a); return MULIB_PROWSE_MEMORY; }
  mulib_strbuf_init (&code); (void)snprintf (count, sizeof (count), "%zu", c->options.query_items);
  rc = !mulib_strbuf_append (&code, all ? "Array.from(document.querySelectorAll(" : "(function(){const e=document.querySelector(")
      || !mulib_strbuf_append (&code, q)
      || !mulib_strbuf_append (&code, all ? ")).slice(0," : ");return e?") ? MULIB_PROWSE_MEMORY : 0;
  if (!rc && all && (!mulib_strbuf_append (&code, count) || !mulib_strbuf_append (&code, ").map(e=>"))) rc = MULIB_PROWSE_MEMORY;
  if (!rc && extract && !mulib_strbuf_append (&code, extract)) rc = MULIB_PROWSE_MEMORY;
  if (!rc && !extract && (!mulib_strbuf_append (&code, "e.getAttribute(") || !mulib_strbuf_append (&code, a)
      || !mulib_strbuf_append (&code, ")"))) rc = MULIB_PROWSE_MEMORY;
  if (!rc && !mulib_strbuf_append (&code, all ? ")" : ":null})()")) rc = MULIB_PROWSE_MEMORY;
  if (!rc) rc = mulib_prowse_control_evaluate (c, code.data, out);
  free (q); free (a); mulib_strbuf_free (&code); return rc;
}
static int mulib_prowse_control_find_ (mulib_prowse_control_t *c, const char *selector, char id[256])
{
  mulib_json_t *p = mulib_json_new (MULIB_JSON_OBJECT);
  mulib_prowse_reply_t r = { 0 };
  int rc;
  if (!p || !mulib_prowse_control_set_ (p, "using", mulib_json_string ("css selector"))
      || !mulib_prowse_control_set_ (p, "value", mulib_json_string (selector))) { mulib_json_destroy (p); return MULIB_PROWSE_MEMORY; }
  rc = mulib_prowse_control_request_ (c, "POST", "/element", "WebDriver:FindElement", NULL, p, &r);
  if (!rc)
    {
      size_t n;
      const char *s = mulib_json_text (mulib_json_get (r.value, "element-6066-11e4-a52e-4f735466cecf"), &n);
      if (!s || !n || n > 255 || memchr (s, 0, n)) rc = MULIB_PROWSE_FORMAT;
      else { memcpy (id, s, n); id[n] = 0; }
    }
  mulib_json_destroy (p); mulib_prowse_reply_free (&r); return rc;
}
static int mulib_prowse_control_interact_ (mulib_prowse_control_t *c, const char *selector, const char *text)
{
  mulib_json_t *p = NULL, *value = NULL;
  mulib_prowse_reply_t reply = { 0 };
  int rc;
  if (!c || !selector) return MULIB_PROWSE_INVALID;
  if (mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_CDP)
    {
      mulib_strbuf_t js;
      char *q = mulib_prowse_control_quote_ (selector);
      if (!q) return MULIB_PROWSE_MEMORY;
      mulib_strbuf_init (&js);
      rc = !mulib_strbuf_append (&js, "(function(){const e=document.querySelector(") || !mulib_strbuf_append (&js, q)
          || !mulib_strbuf_append (&js, ");if(!e)throw new Error('element not found');e.scrollIntoView({block:'center'});e.focus();const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})()") ? MULIB_PROWSE_MEMORY : 0;
      free (q);
      if (!rc) rc = mulib_prowse_control_evaluate (c, js.data, &value);
      mulib_strbuf_free (&js); if (rc) return rc;
      p = mulib_json_new (MULIB_JSON_OBJECT);
      if (!p) rc = MULIB_PROWSE_MEMORY;
      else if (text)
        { if (!mulib_prowse_control_set_ (p, "text", mulib_json_string (text))) rc = MULIB_PROWSE_MEMORY; }
      else
        {
          const mulib_json_t *x = mulib_json_get (value, "x"), *y = mulib_json_get (value, "y");
          if (mulib_json_typeof (x) != MULIB_JSON_NUMBER || mulib_json_typeof (y) != MULIB_JSON_NUMBER) rc = MULIB_PROWSE_FORMAT;
          else if (!mulib_prowse_control_set_ (p, "x", mulib_json_number (mulib_json_numeric (x)))
              || !mulib_prowse_control_set_ (p, "y", mulib_json_number (mulib_json_numeric (y)))
              || !mulib_prowse_control_set_ (p, "type", mulib_json_string ("mousePressed"))
              || !mulib_prowse_control_set_ (p, "button", mulib_json_string ("left"))
              || !mulib_prowse_control_set_ (p, "clickCount", mulib_json_number (1))) rc = MULIB_PROWSE_MEMORY;
        }
      if (!rc) rc = mulib_prowse_control_request_ (c, NULL, NULL, NULL, text ? "Input.insertText" : "Input.dispatchMouseEvent", p, &reply);
      mulib_prowse_reply_free (&reply);
      if (!rc && !text)
        {
          if (!mulib_prowse_control_set_ (p, "type", mulib_json_string ("mouseReleased"))) rc = MULIB_PROWSE_MEMORY;
          else rc = mulib_prowse_control_request_ (c, NULL, NULL, NULL, "Input.dispatchMouseEvent", p, &reply);
        }
    }
  else
    {
      char id[256], encoded[1024], path[1100];
      rc = mulib_prowse_control_find_ (c, selector, id);
      if (rc) return rc;
      rc = mulib_prowse_control_path_ (id, text ? "/value" : "/click", encoded, sizeof (encoded));
      if (rc) return rc;
      (void)snprintf (path, sizeof (path), "/element/%s", encoded + 9);
      p = mulib_json_new (MULIB_JSON_OBJECT);
      if (!p || !mulib_prowse_control_set_ (p, "id", mulib_json_string (id))
          || (text && !mulib_prowse_control_set_ (p, "text", mulib_json_string (text)))) rc = MULIB_PROWSE_MEMORY;
      else rc = mulib_prowse_control_request_ (c, "POST", path, text ? "WebDriver:ElementSendKeys" : "WebDriver:ElementClick", NULL, p, &reply);
    }
  mulib_json_destroy (p); mulib_json_destroy (value); mulib_prowse_reply_free (&reply); return rc;
}
int mulib_prowse_control_click (mulib_prowse_control_t *c, const char *selector)
{ return mulib_prowse_control_interact_ (c, selector, NULL); }
int mulib_prowse_control_type (mulib_prowse_control_t *c, const char *selector, const char *text)
{ return text ? mulib_prowse_control_interact_ (c, selector, text) : MULIB_PROWSE_INVALID; }
int mulib_prowse_control_screenshot (mulib_prowse_control_t *c, mulib_json_t **out)
{
  mulib_prowse_reply_t r = { 0 };
  const mulib_json_t *value;
  int rc;
  if (!c || !out || *out) return MULIB_PROWSE_INVALID;
  rc = mulib_prowse_control_request_ (c, "GET", "/screenshot", "WebDriver:TakeScreenshot", "Page.captureScreenshot", NULL, &r);
  value = mulib_prowse_protocol_kind (c->protocol) == MULIB_PROWSE_CDP ? mulib_json_get (r.value, "data") : r.value;
  if (!rc && mulib_json_typeof (value) != MULIB_JSON_STRING) rc = MULIB_PROWSE_FORMAT;
  if (!rc) { *out = mulib_prowse_control_clone_ (value); if (!*out) rc = MULIB_PROWSE_MEMORY; }
  mulib_prowse_reply_free (&r); return rc;
}
static int mulib_prowse_control_sleep_ (unsigned ms)
{
#if !defined(__STDC_NO_THREADS__)
  struct timespec duration, remaining;
  int rc;
  duration.tv_sec = ms / 1000; duration.tv_nsec = (long)(ms % 1000) * 1000000L;
  while ((rc = thrd_sleep (&duration, &remaining)) == -1) duration = remaining;
  return rc ? MULIB_PROWSE_IO : 0;
#else
  (void)ms; return MULIB_PROWSE_UNSUPPORTED;
#endif
}
static uint64_t mulib_prowse_control_now_ (void)
{
  struct timespec t;
#if defined(CLOCK_MONOTONIC)
  if (!clock_gettime (CLOCK_MONOTONIC, &t))
    return (uint64_t)t.tv_sec * 1000 + (unsigned long)t.tv_nsec / 1000000;
#endif
  if (timespec_get (&t, TIME_UTC) != TIME_UTC) return 0;
  return (uint64_t)t.tv_sec * 1000 + (unsigned long)t.tv_nsec / 1000000;
}
int mulib_prowse_control_wait (mulib_prowse_control_t *c, const char *selector, unsigned timeout)
{
  mulib_strbuf_t code;
  char *q;
  uint64_t end;
  int rc;
  if (!c || !selector) return MULIB_PROWSE_INVALID;
  if (timeout > c->options.wait_ms) timeout = c->options.wait_ms;
  q = mulib_prowse_control_quote_ (selector); if (!q) return MULIB_PROWSE_MEMORY;
  mulib_strbuf_init (&code);
  rc = !mulib_strbuf_append (&code, "document.querySelector(") || !mulib_strbuf_append (&code, q)
      || !mulib_strbuf_append (&code, ") !== null") ? MULIB_PROWSE_MEMORY : 0;
  free (q); end = mulib_prowse_control_now_ () + timeout;
  while (!rc)
    {
      mulib_json_t *v = NULL;
      uint64_t now;
      unsigned old = mulib_prowse_protocol_timeout (c->protocol, 0), remaining;
      now = mulib_prowse_control_now_ ();
      remaining = end > now ? (unsigned)(end - now) : 1;
      if (old && remaining < old) (void)mulib_prowse_protocol_timeout (c->protocol, remaining);
      rc = mulib_prowse_control_evaluate (c, code.data, &v);
      if (old) (void)mulib_prowse_protocol_timeout (c->protocol, old);
      if (!rc && mulib_json_typeof (v) != MULIB_JSON_BOOL) rc = MULIB_PROWSE_FORMAT;
      if (!rc && mulib_json_boolean (v)) { mulib_json_destroy (v); break; }
      mulib_json_destroy (v); if (rc) break;
      now = mulib_prowse_control_now_ ();
      if (now >= end) { rc = MULIB_PROWSE_TIMEOUT; break; }
      rc = mulib_prowse_control_sleep_ (end - now < c->options.poll_ms ? (unsigned)(end - now) : c->options.poll_ms);
    }
  mulib_strbuf_free (&code); return mulib_prowse_control_error_ (c, rc, NULL);
}
static const char *mulib_prowse_control_string_ (const mulib_json_t *args, const char *key)
{
  size_t n;
  const char *s = mulib_json_text (mulib_json_get (args, key), &n);
  return s && !memchr (s, 0, n) ? s : NULL;
}
static int mulib_prowse_control_to_lua_ (mulib_mune_state_t *s, const mulib_json_t *json,
    mulib_mune_value_t *out, mulib_mune_result_t *roots, unsigned depth)
{
  mulib_mune_value_t v = { MULIB_MUNE_NIL, { 0 } };
  size_t i, n;
  const char *text;
  if (depth > 64) return 0;
  switch (mulib_json_typeof (json))
    {
    case MULIB_JSON_NULL:
      {
        mulib_mune_value_t module, key;
        if (mulib_mune_get_global (s, "lprowse", &module)
            || mulib_mune_make_string (s, "null", 4, &key) || mulib_mune_raw_get (s, module, key, &v)) return 0;
        break;
      }
    case MULIB_JSON_BOOL: v.type = MULIB_MUNE_BOOLEAN; v.as.boolean = mulib_json_boolean (json); break;
    case MULIB_JSON_NUMBER: v.type = MULIB_MUNE_NUMBER; v.as.number = mulib_json_numeric (json); break;
    case MULIB_JSON_STRING:
      text = mulib_json_text (json, &n);
      if (mulib_mune_make_string (s, text, n, &v)) return 0;
      break;
    case MULIB_JSON_ARRAY: case MULIB_JSON_OBJECT:
      if (mulib_mune_make_table (s, &v) || !mulib_mune_result_push (roots, v)) return 0;
      for (i = 0; i < mulib_json_size (json); ++i)
        {
          mulib_mune_value_t key = { MULIB_MUNE_INTEGER, { 0 } }, child;
          if (mulib_json_typeof (json) == MULIB_JSON_ARRAY) key.as.integer = (int64_t)i + 1;
          else
            {
              text = mulib_json_key (json, i, &n);
              if (mulib_mune_make_string (s, text, n, &key) || !mulib_mune_result_push (roots, key)) return 0;
            }
          if (!mulib_prowse_control_to_lua_ (s, mulib_json_at (json, i), &child, roots, depth + 1)
              || mulib_mune_raw_set (s, v, key, child)) return 0;
        }
      if (mulib_json_typeof (json) == MULIB_JSON_ARRAY)
        {
          mulib_mune_value_t module, key, fn;
          mulib_mune_result_t *result = NULL;
          if (mulib_mune_get_global (s, "lprowse", &module) || mulib_mune_make_string (s, "array", 5, &key)
              || mulib_mune_raw_get (s, module, key, &fn) || mulib_mune_call (s, fn, &v, 1, &result, NULL)) return 0;
          mulib_mune_result_destroy (result);
        }
      break;
    }
  if (!mulib_mune_result_push (roots, v)) return 0;
  *out = v; return 1;
}
static mulib_mune_status_t mulib_prowse_control_native_ (mulib_mune_state_t *state,
    const mulib_mune_value_t *argv, size_t argc, mulib_mune_result_t *out, void *user)
{
  mulib_prowse_control_t *c = (mulib_prowse_control_t *)user;
  const char *op, *text, *s, *t;
  size_t n, opn;
  mulib_json_t *args = NULL, *value = NULL;
  mulib_prowse_reply_t reply = { 0 };
  mulib_mune_result_t *roots = NULL;
  mulib_mune_value_t v = { MULIB_MUNE_NIL, { 0 } };
  int rc = MULIB_PROWSE_INVALID, ok = 1;
  if (argc != 2 || !(op = mulib_mune_value_string (argv[0], &opn)) || memchr (op, 0, opn)
      || !(text = mulib_mune_value_string (argv[1], &n)) || n > c->options.script_bytes) return MULIB_MUNE_INVALID;
  c->diagnostic[0] = 0;
  args = mulib_json_parse (text, n, NULL);
  if (!args || mulib_json_typeof (args) != MULIB_JSON_OBJECT) goto done;
  s = mulib_prowse_control_string_ (args, "selector");
  if (!strcmp (op, "connect"))
    {
      mulib_prowse_protocol_kind_t kind = (mulib_prowse_protocol_kind_t)0;
      t = mulib_prowse_control_string_ (args, "protocol");
      if (t && !strcmp (t, "cdp")) kind = MULIB_PROWSE_CDP;
      if (t && !strcmp (t, "webdriver")) kind = MULIB_PROWSE_WEBDRIVER;
      if (t && !strcmp (t, "marionette")) kind = MULIB_PROWSE_MARIONETTE;
      rc = mulib_prowse_control_connect (c, kind, mulib_prowse_control_string_ (args, "endpoint"), NULL, NULL);
    }
  else if (!strcmp (op, "start")) rc = mulib_prowse_control_start (c, mulib_json_get (args, "capabilities"));
  else if (!strcmp (op, "attach")) rc = mulib_prowse_control_attach (c, mulib_prowse_control_string_ (args, "session"));
  else if (!strcmp (op, "quit")) rc = mulib_prowse_control_quit (c);
  else if (!strcmp (op, "disconnect")) { mulib_prowse_control_disconnect (c); rc = 0; }
  else if (!strcmp (op, "navigate")) rc = mulib_prowse_control_navigate (c, mulib_prowse_control_string_ (args, "url"));
  else if (!strcmp (op, "evaluate")) rc = mulib_prowse_control_evaluate (c, mulib_prowse_control_string_ (args, "expression"), &value);
  else if (!strcmp (op, "click")) rc = mulib_prowse_control_click (c, s);
  else if (!strcmp (op, "type")) rc = mulib_prowse_control_type (c, s, mulib_prowse_control_string_ (args, "text"));
  else if (!strcmp (op, "query")) rc = mulib_prowse_control_query (c, s, mulib_prowse_control_string_ (args, "property"),
      mulib_prowse_control_string_ (args, "attribute"), mulib_json_boolean (mulib_json_get (args, "all")), &value);
  else if (!strcmp (op, "screenshot")) rc = mulib_prowse_control_screenshot (c, &value);
  else if (!strcmp (op, "sleep") || !strcmp (op, "wait"))
    {
      const mulib_json_t *ms = mulib_json_get (args, "milliseconds");
      double number = mulib_json_numeric (ms);
      if (mulib_json_typeof (ms) == MULIB_JSON_NUMBER && number >= 0 && number <= c->options.wait_ms && floor (number) == number)
        rc = !strcmp (op, "sleep") ? mulib_prowse_control_sleep_ ((unsigned)number) : mulib_prowse_control_wait (c, s, (unsigned)number);
    }
  else if (!strcmp (op, "command") || !strcmp (op, "http"))
    {
      if (!strcmp (op, "command")) rc = mulib_prowse_protocol_call (c->protocol, mulib_prowse_control_string_ (args, "method"), mulib_json_get (args, "params"), &reply);
      else rc = mulib_prowse_protocol_http (c->protocol, mulib_prowse_control_string_ (args, "method"), mulib_prowse_control_string_ (args, "path"), mulib_json_get (args, "body"), &reply);
      mulib_prowse_control_error_ (c, rc, &reply);
      if (!rc) { value = mulib_prowse_control_clone_ (reply.value); if (!value) rc = MULIB_PROWSE_MEMORY; }
    }
  else if (!strcmp (op, "event"))
    { rc = mulib_prowse_protocol_event (c->protocol, &value); if (rc == MULIB_PROWSE_STATE) { value = mulib_json_new (MULIB_JSON_NULL); rc = value ? 0 : MULIB_PROWSE_MEMORY; } }
  else if (!strcmp (op, "status"))
    {
      value = mulib_json_new (MULIB_JSON_OBJECT);
      rc = !value || !mulib_prowse_control_set_ (value, "active", mulib_json_bool (c->active))
          || !mulib_prowse_control_set_ (value, "session", mulib_json_string (c->session))
          || !mulib_prowse_control_set_ (value, "protocol", mulib_json_number (mulib_prowse_protocol_kind (c->protocol))) ? MULIB_PROWSE_MEMORY : 0;
    }
  else if (!strcmp (op, "resolve"))
    {
      mulib_uri_t base, relative;
      char *resolved;
      s = mulib_prowse_control_string_ (args, "url"); t = mulib_prowse_control_string_ (args, "base");
      if (s && t && mulib_uri_parse (t, strlen (t), &base) && mulib_uri_parse (s, strlen (s), &relative))
        {
          resolved = mulib_uri_resolve (&base, &relative);
          value = resolved ? mulib_json_string (resolved) : NULL; free (resolved);
          rc = value ? 0 : MULIB_PROWSE_MEMORY;
        }
    }
done:
  if (rc)
    {
      mulib_mune_value_t message, code = { MULIB_MUNE_INTEGER, { 0 } };
      code.as.integer = rc;
      if (!*c->diagnostic) mulib_prowse_control_error_ (c, rc, NULL);
      ok = mulib_mune_result_push (out, v) && !mulib_mune_make_string (state, c->diagnostic, strlen (c->diagnostic), &message)
          && mulib_mune_result_push (out, message) && mulib_mune_result_push (out, code);
    }
  else if (value)
    {
      roots = mulib_mune_result_create (state);
      ok = roots && mulib_prowse_control_to_lua_ (state, value, &v, roots, 0) && mulib_mune_result_push (out, v);
    }
  else { v.type = MULIB_MUNE_BOOLEAN; v.as.boolean = 1; ok = mulib_mune_result_push (out, v); }
  mulib_mune_result_destroy (roots); mulib_prowse_reply_free (&reply); mulib_json_destroy (args); mulib_json_destroy (value);
  return ok ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}
mulib_mune_state_t *mulib_prowse_control_lua (mulib_prowse_control_t *c)
{
  mulib_mune_state_t *s;
  mulib_mune_result_t *result = NULL;
  mulib_mune_error_t error;
  mulib_strbuf_t source;
  size_t i;
  int rc = 0;
  if (!c) return NULL;
  if (c->lua) return c->lua;
  s = mulib_mune_state_create (&c->options.lua); if (!s) return NULL;
  mulib_strbuf_init (&source);
  for (i = 0; i < sizeof (mulib_lprowse_source_) / sizeof (*mulib_lprowse_source_); ++i)
    if (!mulib_strbuf_append (&source, mulib_lprowse_source_[i])) { rc = MULIB_MUNE_MEMORY; break; }
  if (!rc) rc = mulib_mune_bind (s, "__mulib_lprowse", mulib_prowse_control_native_, c);
  if (!rc) rc = mulib_mune_run_source (s, source.data, source.length, &result, &error);
  if (!rc) rc = mulib_mune_set_global (s, "lprowse", mulib_mune_result_get (result, 0));
  if (!rc) rc = mulib_mune_set_global (s, "browser", mulib_mune_result_get (result, 0));
  mulib_mune_result_destroy (result); result = NULL; mulib_strbuf_free (&source);
  if (!rc)
    {
      static const char setup[] = "package.loaded.lprowse=lprowse";
      rc = mulib_mune_run_source (s, setup, sizeof (setup) - 1, &result, &error);
    }
  mulib_mune_result_destroy (result);
  if (rc) { (void)snprintf (c->diagnostic, sizeof (c->diagnostic), "lprowse initialization failed (%d)", rc); mulib_mune_state_destroy (s); return NULL; }
  c->lua = s; return s;
}
mulib_mune_status_t mulib_prowse_control_run (mulib_prowse_control_t *c, const char *source, size_t length,
    mulib_mune_result_t **result, mulib_mune_error_t *error)
{
  mulib_mune_status_t rc;
  if (!c || (!source && length) || !result || *result) return MULIB_MUNE_INVALID;
  if (c->executing) return MULIB_MUNE_INVALID;
  if (length > c->options.script_bytes) return MULIB_MUNE_LIMIT;
  if (!mulib_prowse_control_lua (c)) return MULIB_MUNE_MEMORY;
  c->executing = 1; rc = mulib_mune_run_source (c->lua, source, length, result, error); c->executing = 0; return rc;
}
mulib_readline_t *mulib_prowse_control_editor (mulib_prowse_control_t *c)
{
  static const char *const words[] = { "local", "function", "end", "if", "then", "else", "elseif", "for", "while", "do", "return", "true", "false", "nil", "and", "or", "not", "in", "repeat", "until", "break" };
  static const char *const methods[] = { "go", "navigate", "start", "connect", "attach", "quit", "disconnect", "eval", "click", "type", "fill", "text", "html", "attr", "links", "title", "url", "screenshot", "wait", "sleep", "scrape", "crawl", "retry", "command", "http", "event", "resolve", "status" };
  static const char *const directives[] = { "help", "quit", "begin", "end", "load", "profile" };
  mulib_readline_t *r;
  size_t i;
  int rc = 0;
  if (!c) return NULL;
  if (c->editor) return c->editor;
  r = mulib_readline_create (NULL); if (!r) return NULL;
  rc = mulib_readline_rule (r, MULIB_READLINE_PREFIX, MULIB_READLINE_COMMENT, "--", NULL, 0);
  if (!rc) rc = mulib_readline_rule (r, MULIB_READLINE_SPAN, MULIB_READLINE_STRING, "\"", "\"", '\\');
  if (!rc) rc = mulib_readline_rule (r, MULIB_READLINE_SPAN, MULIB_READLINE_STRING, "'", "'", '\\');
  if (!rc) rc = mulib_readline_rule (r, MULIB_READLINE_NUMERIC, MULIB_READLINE_NUMBER, NULL, NULL, 0);
  for (i = 0; i < sizeof (words) / sizeof (*words) && !rc; ++i)
    { rc = mulib_readline_rule (r, MULIB_READLINE_WORD, MULIB_READLINE_KEYWORD, words[i], NULL, 0); if (!rc) rc = mulib_readline_completion (r, words[i], "Lua keyword"); }
  for (i = 0; i < sizeof (methods) / sizeof (*methods) && !rc; ++i)
    { char name[64]; (void)snprintf (name, sizeof (name), "browser.%s", methods[i]); rc = mulib_readline_completion (r, name, "lprowse operation"); }
  for (i = 0; i < sizeof (directives) / sizeof (*directives) && !rc; ++i)
    {
      char name[32];
      rc = mulib_readline_directive (r, directives[i], directives[i], NULL, "Prowse REPL directive");
      (void)snprintf (name, sizeof (name), ":%s", directives[i]);
      if (!rc) rc = mulib_readline_completion (r, name, "Prowse REPL directive");
    }
  if (rc) { mulib_readline_destroy (r); return NULL; }
  c->editor = r; return r;
}
static int mulib_prowse_control_print_ (int fd, const char *s)
{ size_t written; return mulib_termutils_write (fd, s, strlen (s), &written) ? MULIB_PROWSE_IO : 0; }
int mulib_prowse_control_repl (mulib_prowse_control_t *c, int input, int output)
{
  mulib_readline_t *editor = mulib_prowse_control_editor (c);
  mulib_strbuf_t source;
  int multiline = 0, rc = 0;
  if (!editor || !mulib_prowse_control_lua (c)) return MULIB_PROWSE_MEMORY;
  mulib_strbuf_init (&source);
  while (!rc)
    {
      char *line = NULL, *expanded = NULL;
      const char *action = NULL, *arguments = NULL;
      mulib_mune_result_t *result = NULL;
      mulib_mune_error_t error = { MULIB_MUNE_OK, 0, NULL };
      int ready = 1, status = mulib_readline_read (editor, input, output, multiline ? "....> " : "prowse> ", &line);
      if (status == MULIB_READLINE_EOF) break;
      if (status == MULIB_READLINE_INTERRUPT) { source.length = 0; if (source.data) source.data[0] = 0; multiline = 0; continue; }
      if (status != MULIB_READLINE_LINE) { rc = status; break; }
      status = mulib_readline_dispatch (editor, line, &action, &arguments, &expanded);
      if (status == MULIB_READLINE_DIRECTIVE && action)
        {
          ready = 0;
          if (!strcmp (action, "quit")) { free (line); break; }
          else if (!strcmp (action, "help")) rc = mulib_prowse_control_print_ (output,
              "require('lprowse') or browser: use dot calls, e.g. browser.go(url)\n=expression prints results; :begin / :end wraps multiline Lua\n:load path.lua executes a file; :profile file loads .mlsyn/.mlac/.mldir\n:quit exits; browser.quit() ends the session\n");
          else if (!strcmp (action, "begin")) { multiline = 1; source.length = 0; if (source.data) source.data[0] = 0; }
          else if (!strcmp (action, "end")) { ready = multiline; multiline = 0; }
          else if (!strcmp (action, "load"))
            {
              FILE *file = fopen (arguments, "rb");
              if (!file) rc = mulib_prowse_control_print_ (output, "cannot open script\n");
              else
                {
                  char buffer[4096]; size_t n;
                  source.length = 0; if (source.data) source.data[0] = 0;
                  while ((n = fread (buffer, 1, sizeof (buffer), file)) != 0)
                    if (n > c->options.script_bytes - source.length || !mulib_strbuf_append_n (&source, buffer, n)) { rc = MULIB_PROWSE_LIMIT; break; }
                  if (ferror (file)) rc = MULIB_PROWSE_IO;
                  fclose (file); ready = !rc;
                }
            }
          else if (!strcmp (action, "profile"))
            {
              size_t error_line;
              status = mulib_readline_load (editor, arguments, &error_line);
              if (status) rc = mulib_prowse_control_print_ (output, "profile could not be loaded\n");
            }
          else rc = mulib_prowse_control_print_ (output, "action directive has no REPL handler\n");
        }
      else if (status < 0 && status != MULIB_READLINE_NOT_FOUND) rc = status;
      else if (*line == ':' && status == MULIB_READLINE_NOT_FOUND)
        { rc = mulib_prowse_control_print_ (output, "unknown directive\n"); ready = 0; }
      else
        {
          const char *code = expanded ? expanded : line;
          if (!multiline) { source.length = 0; if (source.data) source.data[0] = 0; }
          if (*code == '=' && !multiline) { if (!mulib_strbuf_append (&source, "return ")) rc = MULIB_PROWSE_MEMORY; ++code; }
          if (strlen (code) >= c->options.script_bytes - source.length) rc = MULIB_PROWSE_LIMIT;
          else if (!mulib_strbuf_append (&source, code) || !mulib_strbuf_append (&source, "\n")) rc = MULIB_PROWSE_MEMORY;
          ready = !multiline;
        }
      free (expanded); free (line);
      if (rc || !ready) continue;
      status = mulib_prowse_control_run (c, source.data, source.length, &result, &error);
      if (status)
        { rc = mulib_prowse_control_print_ (output, error.message ? error.message : "Lua execution failed"); if (!rc) rc = mulib_prowse_control_print_ (output, "\n"); }
      else
        for (size_t i = 0; i < mulib_mune_result_count (result) && !rc; ++i)
          {
            mulib_mune_value_t value = mulib_mune_result_get (result, i);
            const char *text = NULL;
            char buffer[80];
            size_t n = 0, written;
            if (value.type == MULIB_MUNE_STRING) text = mulib_mune_value_string (value, &n);
            else if (value.type == MULIB_MUNE_INTEGER) { (void)snprintf (buffer, sizeof (buffer), "%lld", (long long)value.as.integer); text = buffer; }
            else if (value.type == MULIB_MUNE_NUMBER) { (void)snprintf (buffer, sizeof (buffer), "%.17g", value.as.number); text = buffer; }
            else text = value.type == MULIB_MUNE_NIL ? "nil" : value.type == MULIB_MUNE_BOOLEAN ? value.as.boolean ? "true" : "false" : "<Lua object>";
            if (value.type != MULIB_MUNE_STRING) n = strlen (text);
            if (mulib_termutils_write (output, text, n, &written)) rc = MULIB_PROWSE_IO;
            if (!rc) rc = mulib_prowse_control_print_ (output, "\n");
          }
      mulib_mune_result_destroy (result);
    }
  mulib_strbuf_free (&source); return rc;
}
#endif
