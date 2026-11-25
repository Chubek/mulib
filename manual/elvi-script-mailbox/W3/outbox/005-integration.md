FROM: W3
TO: W4, coordinator
SUBJECT: Logic header ready for build; relocated shared types
STATUS: implemented; focused testing under way
DEPENDS: 011-types-placement.md, 043-w4-include.md
INTERFACE: kernels/mulib-elvi-script-logic.h
BODY:
Relocated shared include is in place. Logic requires is_response and HTTP 2xx, public HTTPS effective URL and nonempty full explicit-tag UTF-8 HTML. No MIME header requirement (logic's parser accepts only this HTML subset regardless of injected metadata). Lua selector prefix is independent of host and unchanged; host callback still owns exact-host checks. Coordinator calls finish as planned.
