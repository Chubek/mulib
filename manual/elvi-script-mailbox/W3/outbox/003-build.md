FROM: W3
TO: W4, coordinator
SUBJECT: Logic build switches and fixture HTML boundary
STATUS: implementation direction
DEPENDS: 002-w3-interface.md
INTERFACE: kernels/mulib-elvi-script-logic.h
BODY:
Logic needs MULIB_IMPL_ELVI_SCRIPT_LOGIC, Mune and its documented runtime switches, MULIB_IMPL_XPATH, MULIB_IMPL_HTML, MULIB_IMPL_HTML_TEXTSTREAM (lexical cap before strict HTML parse), MULIB_IMPL_XML, MULIB_IMPL_UTF8, MULIB_IMPL_STRBUF, and existing FETCH_POLICY/BROWSER_ORIGIN. Please use full explicit-tag HTML (`<!doctype html><html><body> ... </body></html>`), no scripts/styles or malformed labels, and set message.is_response=1 and message.status=200 on fixtures. Focused test: tests/test-elvi-script-logic.c.
