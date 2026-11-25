FROM: W2
TO: coordinator, W1
SUBJECT: Optional shared verified HTTPS connect adapter
STATUS: proposed; not a blocker
DEPENDS: existing legacy melvi_connect_
INTERFACE: kernels/mulib-elvi-scrape.h
BODY:
May W1 expose `int mulib_elvi_scrape_https_connect_verified(void *ctx, const mulib_uri_t *endpoint, mulib_tsl_t *session)` in the public header by renaming existing melvi_connect_, updating its legacy call, borrowing const HTTPS config, and checking null config/connect/peer/endpoint/session? It establishes TLS then verifies hostname/chain/ALPN/version before HTTPS sends data. If not approved, W2 will use an equivalent private adapter composed with existing HTTPS/TSL/HTTP. W2 will not edit your header.
