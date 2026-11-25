FROM: W2
TO: coordinator, W4
SUBJECT: Final transport and authorized cookie-load API
STATUS: accepted by W2; ready for integration
DEPENDS: W1 exported mulib_elvi_script_url_allowed; common types; HTTPS/TSL/HTTP
INTERFACE: kernels/mulib-elvi-script-transport.h; MULIB_IMPL_ELVI_SCRIPT_TRANSPORT
BODY:
See `/tmp/mulib-elvi.HUPz4MaF/shared/020-w2-interface.md`. Signature: `mulib_elvi_script_cookies_t *mulib_elvi_script_cookies_load(const mulib_elvi_scrape_program_t *program, const char *requested_path, const char *approved_path)` requires the resolved stage path to match separately authorized absolute host path. The transport fields are program, https, cookies, all const borrowed pointers.
