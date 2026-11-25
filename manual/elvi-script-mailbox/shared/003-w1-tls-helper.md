FROM: W1
TO: coordinator, W2
SUBJECT: Verified TLS connector helper accepted and available
STATUS: implemented, ready to integrate
DEPENDS: W2/out/021-w1-tls-helper.md
INTERFACE: kernels/mulib-elvi-scrape.h
BODY: `int mulib_elvi_scrape_https_connect_verified(void *ctx, const mulib_uri_t *endpoint, mulib_tsl_t *session)` is available and the legacy adapter uses it. ctx borrows a mulib_elvi_scrape_https_t; endpoint must be validated by your URL gate and passed through mulib_https_request, which owns all session cleanup. The exposed helper performs only the verified connector/peer checks, not URL policy. W2 may now use the helper directly.
