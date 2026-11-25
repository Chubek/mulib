/** @file mulib-elvi-script-transport.h
 * @brief Scoped cookie import and a verified HTTPS ElviScript transport.
 * Define MULIB_IMPL_ELVI_SCRIPT_TRANSPORT in one C translation unit.
 */
#ifndef MULIB_ELVI_SCRIPT_TRANSPORT_H
#define MULIB_ELVI_SCRIPT_TRANSPORT_H
#include "mulib-elvi-scrape.h"
#include "detail/mulib-elvi-script-types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_ELVI_SCRIPT_MAX_COOKIE_JSON (64u * 1024u)
#define MULIB_ELVI_SCRIPT_MAX_COOKIES 64
#define MULIB_ELVI_SCRIPT_MAX_COOKIE_HEADER 8192

typedef struct mulib_elvi_script_cookies mulib_elvi_script_cookies_t;

/* Import caller-authorized bytes. The store is bound to the profile's HTTPS
 * home host. NULL means failure; an empty JSON array returns an empty store. */
mulib_elvi_script_cookies_t *
mulib_elvi_script_cookies_parse(const mulib_elvi_scrape_program_t *program,
                               const char *json, size_t length);

/* The caller must authorize approved_path independently of the untrusted
 * profile. Both arguments must designate the exact same absolute path.
 * Only the POSIX private, regular-file loader is supported. */
mulib_elvi_script_cookies_t *
mulib_elvi_script_cookies_load(const mulib_elvi_scrape_program_t *program,
                              const char *requested_path, const char *approved_path);

void mulib_elvi_script_cookies_destroy(mulib_elvi_script_cookies_t *cookies);

typedef struct mulib_elvi_script_transport {
  const mulib_elvi_scrape_program_t *program;
  const mulib_elvi_scrape_https_t *https;
  const mulib_elvi_script_cookies_t *cookies; /* Optional; all fields borrowed. */
} mulib_elvi_script_transport_t;

/* A synchronous mulib_elvi_script_fetch_fn. GET, exact first-party HTTPS
 * host, verified TLS, no redirects. On failure response is unchanged.
 * On success it owns its message; free using mulib_http_message_free. */
int mulib_elvi_script_fetch_https(void *context,
    const mulib_elvi_script_request_t *request, mulib_elvi_script_response_t *response);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ELVI_SCRIPT_TRANSPORT) && !defined(MULIB_ELVI_SCRIPT_TRANSPORT_IMPLEMENTED)
#define MULIB_ELVI_SCRIPT_TRANSPORT_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(_WIN32) && ((defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || defined(_GNU_SOURCE))
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(O_NOFOLLOW) && defined(O_NONBLOCK) && defined(O_DIRECTORY) && defined(O_CLOEXEC)
#define MULIB_ELVI_SCRIPT_POSIX_COOKIE_FILES_
#endif
#endif

typedef struct melvi_cookie_ {
  char name[129], value[2049], domain[254], path[1025];
  int64_t expires;
  int persistent, host_only, secure, same_site;
} melvi_cookie_;

struct mulib_elvi_script_cookies {
  char host[254];
  size_t count;
  melvi_cookie_ entries[MULIB_ELVI_SCRIPT_MAX_COOKIES];
};

static void melvi_transport_wipe_(void *data, size_t length) {
  volatile unsigned char *p = (volatile unsigned char *)data;
  while (length--) *p++ = 0;
}

static unsigned char melvi_transport_lower_(unsigned char c) {
  return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

static int melvi_transport_equal_(const char *a, size_t length, const char *b) {
  if (length != strlen(b)) return 0;
  for (size_t i = 0; i < length; ++i)
    if (melvi_transport_lower_((unsigned char)a[i]) !=
        melvi_transport_lower_((unsigned char)b[i])) return 0;
  return 1;
}

static int melvi_transport_uri_(const mulib_elvi_scrape_program_t *program,
    const char *url, mulib_uri_t *uri) {
  /* The profile helper includes MuLIB fetch policy and exact-host validation. */
  return mulib_elvi_script_url_allowed(program, url) &&
    mulib_uri_parse(url, strlen(url), uri) &&
    uri->scheme.length == 5 && !memcmp(uri->scheme.data, "https", 5) &&
    uri->host.length && uri->host.length < sizeof(((mulib_elvi_script_cookies_t *)0)->host) &&
    !uri->has_userinfo && !uri->has_port && !uri->has_fragment;
}

static int melvi_transport_home_(const mulib_elvi_scrape_program_t *program,
                                mulib_uri_t *uri) {
  return program && memchr(program->home, 0, sizeof program->home) &&
    melvi_transport_uri_(program, program->home, uri);
}

/* The cookie format is a small flat JSON schema. A dedicated reader rejects
 * duplicate members before interpretation and reads seconds without floating
 * point conversion. No general JSON DOM copies credential material. */
typedef struct melvi_cookie_json_ {
  const unsigned char *p, *end;
} melvi_cookie_json_;

static void melvi_cookie_ws_(melvi_cookie_json_ *j) {
  while (j->p < j->end && (*j->p == ' ' || *j->p == '\t' ||
         *j->p == '\r' || *j->p == '\n')) ++j->p;
}

static int melvi_cookie_take_(melvi_cookie_json_ *j, unsigned char c) {
  melvi_cookie_ws_(j);
  if (j->p == j->end || *j->p != c) return 0;
  ++j->p;
  return 1;
}

static int melvi_cookie_literal_(melvi_cookie_json_ *j, const char *s) {
  size_t n = strlen(s);
  melvi_cookie_ws_(j);
  if ((size_t)(j->end - j->p) < n || memcmp(j->p, s, n)) return 0;
  j->p += n;
  return 1;
}

static int melvi_cookie_hex_(unsigned char c) {
  c = melvi_transport_lower_(c);
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

/* The schema's strings are all printable ASCII after unescaping. */
static int melvi_cookie_string_(melvi_cookie_json_ *j, char *out, size_t cap) {
  size_t used = 0;
  if (!melvi_cookie_take_(j, '"')) return 0;
  while (j->p < j->end) {
    unsigned c = *j->p++;
    if (c == '"') {
      out[used] = 0;
      return 1;
    }
    if (c == '\\') {
      if (j->p == j->end) return 0;
      c = *j->p++;
      if (c == 'u') {
        c = 0;
        for (unsigned i = 0; i < 4; ++i) {
          int d;
          if (j->p == j->end || (d = melvi_cookie_hex_(*j->p++)) < 0) return 0;
          c = 16 * c + (unsigned)d;
        }
      } else if (c != '"' && c != '\\' && c != '/') return 0;
    }
    if (c < 32 || c > 126 || used + 1 >= cap) return 0;
    out[used++] = (char)c;
  }
  return 0;
}

static int melvi_cookie_bool_(melvi_cookie_json_ *j, int *out) {
  if (melvi_cookie_literal_(j, "true")) { *out = 1; return 1; }
  if (melvi_cookie_literal_(j, "false")) { *out = 0; return 1; }
  return 0;
}

static int melvi_cookie_expiry_(melvi_cookie_json_ *j, melvi_cookie_ *c) {
  const int64_t max = INT64_C(253402300799); /* End of year 9999. */
  int negative;
  int64_t n = 0;
  if (melvi_cookie_literal_(j, "null")) return 1;
  negative = melvi_cookie_take_(j, '-');
  if (j->p == j->end || *j->p < '0' || *j->p > '9') return 0;
  if (*j->p == '0') ++j->p; /* No leading zeroes. */
  else while (j->p < j->end && *j->p >= '0' && *j->p <= '9') {
    int d = *j->p++ - '0';
    if (n > (max - d) / 10) return 0;
    n = 10 * n + d;
  }
  c->persistent = 1;
  c->expires = negative ? -n : n;
  return 1;
}

static int melvi_cookie_alnum_(unsigned char c) {
  c = melvi_transport_lower_(c);
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static int melvi_cookie_domain_(melvi_cookie_ *c, const char *host) {
  size_t label = 0, n = strlen(c->domain), h = strlen(host);
  if (!n) return 0;
  if (c->domain[0] == '.') {
    if (c->host_only || n <= 1) return 0;
    memmove(c->domain, c->domain + 1, n);
    --n;
  }
  for (size_t i = 0; i < n; ++i) {
    unsigned char ch = melvi_transport_lower_((unsigned char)c->domain[i]);
    c->domain[i] = (char)ch;
    if (ch == '.') {
      if (!label || c->domain[i - 1] == '-') return 0;
      label = 0;
    } else if ((!melvi_cookie_alnum_(ch) && ch != '-') ||
               (!label && ch == '-') || ++label > 63) return 0;
  }
  if (!label || c->domain[n - 1] == '-' || n > h) return 0;
  if (n == h) return !strcmp(host, c->domain);
  /* Parent domains can only narrow eligibility within this exact-host store.
   * There is no dynamic suffix expansion to any other request destination. */
  return !c->host_only && strchr(c->domain, '.') && host[h - n - 1] == '.' &&
    !strcmp(host + h - n, c->domain);
}

static int melvi_cookie_valid_(melvi_cookie_ *c, const char *host) {
  size_t n = strlen(c->name);
  if (!n || !melvi_cookie_domain_(c, host) || c->path[0] != '/') return 0;
  for (size_t i = 0; i < n; ++i)
    if (!melvi_cookie_alnum_((unsigned char)c->name[i]) &&
        !strchr("!#$%&'*+-.^_`|~", c->name[i])) return 0;
  for (const unsigned char *p = (const unsigned char *)c->value; *p; ++p)
    if (*p <= 32 || *p == '"' || *p == ',' || *p == ';' || *p == '\\') return 0;
  for (const unsigned char *p = (const unsigned char *)c->path; *p; ++p)
    if (*p <= 32 || *p == '?' || *p == '#' || *p == ';' || *p == '\\') return 0;
  if ((c->same_site == 3 ||
       (n >= 9 && melvi_transport_equal_(c->name, 9, "__Secure-"))) && !c->secure) return 0;
  if (n >= 7 && melvi_transport_equal_(c->name, 7, "__Host-") &&
      (!c->secure || !c->host_only || strcmp(c->path, "/"))) return 0;
  return 1;
}

static int melvi_cookie_object_(melvi_cookie_json_ *j, melvi_cookie_ *c, const char *host) {
  static const char *const keys[] = {
    "name", "value", "domain", "path", "secure", "sameSite", "hostOnly", "expires"
  };
  unsigned seen = 0;
  if (!melvi_cookie_take_(j, '{')) return 0;
  c->host_only = 1;
  do {
    char key[16];
    unsigned f;
    if (!melvi_cookie_string_(j, key, sizeof key) || !melvi_cookie_take_(j, ':')) return 0;
    for (f = 0; f < sizeof keys / sizeof *keys && strcmp(key, keys[f]); ++f) {}
    if (f == sizeof keys / sizeof *keys || (seen & (1u << f))) return 0;
    seen |= 1u << f;
    switch (f) {
      case 0: if (!melvi_cookie_string_(j, c->name, sizeof c->name)) return 0; break;
      case 1: if (!melvi_cookie_string_(j, c->value, sizeof c->value)) return 0; break;
      case 2: if (!melvi_cookie_string_(j, c->domain, sizeof c->domain)) return 0; break;
      case 3: if (!melvi_cookie_string_(j, c->path, sizeof c->path)) return 0; break;
      case 4: if (!melvi_cookie_bool_(j, &c->secure)) return 0; break;
      case 5: {
        char site[8];
        if (!melvi_cookie_string_(j, site, sizeof site)) return 0;
        if (!strcmp(site, "Strict")) c->same_site = 1;
        else if (!strcmp(site, "Lax")) c->same_site = 2;
        else if (!strcmp(site, "None")) c->same_site = 3;
        else return 0;
        break;
      }
      case 6: if (!melvi_cookie_bool_(j, &c->host_only)) return 0; break;
      case 7: if (!melvi_cookie_expiry_(j, c)) return 0; break;
    }
  } while (melvi_cookie_take_(j, ','));
  return (seen & 63u) == 63u && melvi_cookie_take_(j, '}') && melvi_cookie_valid_(c, host);
}

void mulib_elvi_script_cookies_destroy(mulib_elvi_script_cookies_t *cookies) {
  if (cookies) {
    melvi_transport_wipe_(cookies, sizeof *cookies);
    free(cookies);
  }
}

mulib_elvi_script_cookies_t *
mulib_elvi_script_cookies_parse(const mulib_elvi_scrape_program_t *program,
                               const char *json, size_t length) {
  mulib_elvi_script_cookies_t *store;
  melvi_cookie_json_ j;
  mulib_uri_t home;
  if (!json || !length || length > MULIB_ELVI_SCRIPT_MAX_COOKIE_JSON ||
      !melvi_transport_home_(program, &home)) return NULL;
  j.p = (const unsigned char *)json;
  j.end = j.p + length;
  if (!melvi_cookie_take_(&j, '[')) return NULL;
  store = (mulib_elvi_script_cookies_t *)calloc(1, sizeof *store);
  if (!store) return NULL;
  for (size_t i = 0; i < home.host.length; ++i)
    store->host[i] = (char)melvi_transport_lower_((unsigned char)home.host.data[i]);
  if (!melvi_cookie_take_(&j, ']')) {
    do {
      melvi_cookie_ *c;
      if (store->count >= MULIB_ELVI_SCRIPT_MAX_COOKIES) goto fail;
      c = &store->entries[store->count];
      if (!melvi_cookie_object_(&j, c, store->host)) goto fail;
      for (size_t i = 0; i < store->count; ++i) {
        const melvi_cookie_ *old = &store->entries[i];
        if (!strcmp(old->name, c->name) && !strcmp(old->domain, c->domain) &&
            !strcmp(old->path, c->path)) goto fail;
      }
      ++store->count;
    } while (melvi_cookie_take_(&j, ','));
    if (!melvi_cookie_take_(&j, ']')) goto fail;
  }
  melvi_cookie_ws_(&j);
  if (j.p != j.end) goto fail;
  return store;
fail:
  mulib_elvi_script_cookies_destroy(store);
  return NULL;
}

#ifdef MULIB_ELVI_SCRIPT_POSIX_COOKIE_FILES_
static int melvi_cookie_dir_(int fd, uid_t user) {
  struct stat st;
  /* S_ISVTX is XSI and may not be exposed with only POSIX.1-2008 selected. */
#ifdef S_ISVTX
  const mode_t sticky = S_ISVTX;
#else
  const mode_t sticky = 01000;
#endif
  /* A trusted sticky directory (e.g. /tmp) protects child entries owned by
   * the trusted owner or effective user, which we check at every step. */
  return !fstat(fd, &st) && S_ISDIR(st.st_mode) &&
    (st.st_uid == 0 || st.st_uid == user) &&
    (!(st.st_mode & (S_IWGRP | S_IWOTH)) || (st.st_mode & sticky));
}

static int melvi_cookie_open_(const char *path) {
  int base, fd = -1;
  uid_t user = geteuid();
  if (!path || path[0] != '/' || !path[1]) return -1;
  for (size_t i = 1; ; ++i) {
    if (i >= 4096) return -1;
    if (!path[i]) break;
  }
  base = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (base < 0) return -1;
  ++path;
  while (melvi_cookie_dir_(base, user)) {
    char name[256];
    size_t n = strcspn(path, "/");
    int next;
    if (!n || n >= sizeof name || (n == 1 && path[0] == '.') ||
        (n == 2 && path[0] == '.' && path[1] == '.')) break;
    memcpy(name, path, n);
    name[n] = 0;
    if (!path[n]) {
      int file = openat(base, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
      if (file >= 0) {
        struct stat st;
        if (!fstat(file, &st) && S_ISREG(st.st_mode) && st.st_uid == user &&
            !(st.st_mode & (S_IRWXG | S_IRWXO)) && st.st_nlink == 1 &&
            st.st_size > 0 && st.st_size <= MULIB_ELVI_SCRIPT_MAX_COOKIE_JSON) fd = file;
        else close(file);
      }
      break;
    }
    next = openat(base, name, O_RDONLY | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC);
    if (next < 0) break;
    close(base);
    base = next;
    path += n + 1;
  }
  close(base);
  return fd;
}
#endif

mulib_elvi_script_cookies_t *
mulib_elvi_script_cookies_load(const mulib_elvi_scrape_program_t *program,
                               const char *requested_path, const char *approved_path) {
#ifdef MULIB_ELVI_SCRIPT_POSIX_COOKIE_FILES_
  mulib_elvi_script_cookies_t *store = NULL;
  mulib_uri_t home;
  const size_t cap = MULIB_ELVI_SCRIPT_MAX_COOKIE_JSON + 1u;
  size_t length = 0;
  char *bytes;
  int fd;
  if (!requested_path || !approved_path || strncmp(requested_path, approved_path, 4096) ||
      !melvi_transport_home_(program, &home)) return NULL;
  fd = melvi_cookie_open_(approved_path);
  if (fd < 0) return NULL;
  bytes = (char *)malloc(cap);
  if (bytes) {
    while (length < cap) {
      ssize_t got = read(fd, bytes + length, cap - length);
      if (got < 0) {
        if (errno == EINTR) continue;
        break;
      }
      if (!got) {
        store = mulib_elvi_script_cookies_parse(program, bytes, length);
        break;
      }
      length += (size_t)got;
    }
    melvi_transport_wipe_(bytes, length);
    free(bytes);
  }
  close(fd);
  return store;
#else
  (void)program; (void)requested_path; (void)approved_path;
  return NULL;
#endif
}

static int melvi_cookie_path_(const char *scope, const char *path, size_t n) {
  size_t len = strlen(scope);
  return len <= n && !memcmp(scope, path, len) &&
    (len == n || scope[len - 1] == '/' || path[len] == '/');
}

static int melvi_cookie_header_(const mulib_elvi_script_cookies_t *store,
                                const mulib_uri_t *uri, char *header, size_t cap) {
  size_t order[MULIB_ELVI_SCRIPT_MAX_COOKIES], count = 0, used = 0;
  time_t now = 0;
  int have_time = 0;
  const char *path = uri->path.length ? uri->path.data : "/";
  size_t path_length = uri->path.length ? uri->path.length : 1;
  header[0] = 0;
  if (!store) return 1;
  if (!melvi_transport_equal_(uri->host.data, uri->host.length, store->host)) return 0;
  for (size_t i = 0; i < store->count; ++i) {
    const melvi_cookie_ *c = &store->entries[i];
    size_t j;
    /* All transactions originate in the profile's fixed first-party HTTPS
     * context and can only target that precise host. Thus all three SameSite
     * modes permit these requests; None was required to carry Secure. */
    if (!c->secure || !melvi_cookie_path_(c->path, path, path_length)) continue;
    if (c->persistent) {
      if (!have_time) {
        now = time(NULL);
        if (now == (time_t)-1 || difftime(now, (time_t)0) < 0) return 0;
        have_time = 1;
      }
      if (difftime(now, (time_t)0) >= (double)c->expires) continue;
    }
    for (j = count; j && strlen(store->entries[order[j - 1]].path) < strlen(c->path); --j)
      order[j] = order[j - 1];
    order[j] = i;
    ++count;
  }
  for (size_t i = 0; i < count; ++i) {
    const melvi_cookie_ *c = &store->entries[order[i]];
    size_t n = strlen(c->name), v = strlen(c->value);
    if (n + v + 1u + (i ? 2u : 0u) >= cap - used) return 0;
    if (i) { header[used++] = ';'; header[used++] = ' '; }
    memcpy(header + used, c->name, n); used += n;
    header[used++] = '=';
    memcpy(header + used, c->value, v); used += v;
  }
  header[used] = 0;
  return 1;
}

int mulib_elvi_script_fetch_https(void *context,
    const mulib_elvi_script_request_t *request, mulib_elvi_script_response_t *response) {
  const mulib_elvi_script_transport_t *t = (const mulib_elvi_script_transport_t *)context;
  mulib_uri_t url, home;
  mulib_http_message_t message = {0};
  mulib_webclient_options_t options = {{32768, 100, MULIB_ELVI_SCRIPT_MAX_BODY, 0},
                                      MULIB_ELVI_SCRIPT_MAX_BODY + 65536u};
  char cookie[MULIB_ELVI_SCRIPT_MAX_COOKIE_HEADER + 1u];
  char cookie_name[] = "Cookie";
  mulib_http_header_t header = {cookie_name, cookie};
  const char *encoding;
  size_t cursor = 0;
  int ok = 0;
  if (!t || !t->https || !t->https->connect || !t->https->peer || !request || !response ||
      !memchr(request->url, 0, sizeof request->url) ||
      !melvi_transport_home_(t->program, &home) ||
      !melvi_transport_uri_(t->program, request->url, &url)) return 0;
  if (!melvi_cookie_header_(t->cookies, &url, cookie, sizeof cookie)) goto done;
  if (mulib_https_request(request->url, "GET", cookie[0] ? &header : NULL, cookie[0] ? 1u : 0u,
                          NULL, 0, &options, mulib_elvi_scrape_https_connect_verified, (void *)t->https,
                          &message) != MULIB_WEBCLIENT_OK) goto done;
  if (message.status < 200 || (message.status >= 300 && message.status < 400) ||
      message.status >= 600) goto done;
  if (mulib_http_header_next(&message, "Content-Type", &cursor) &&
      mulib_http_header_next(&message, "Content-Type", &cursor)) goto done;
  cursor = 0;
  encoding = mulib_http_header_next(&message, "Content-Encoding", &cursor);
  if (encoding && (!melvi_transport_equal_(encoding, strlen(encoding), "identity") ||
        mulib_http_header_next(&message, "Content-Encoding", &cursor))) goto done;
  response->message = message;
  memcpy(response->effective_url, request->url, strlen(request->url) + 1);
  ok = 1;
done:
  melvi_transport_wipe_(cookie, sizeof cookie);
  if (!ok) mulib_http_message_free(&message);
  return ok;
}
#endif
