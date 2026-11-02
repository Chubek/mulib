# Request policy

Define `MULIB_IMPL_WAF`, `SECURITY` and the [HTTP](mulib-http.md) dependency
closure. On an already parsed request, checks size limits, an exact method
allowlist and strict origin path. Query/body attack signatures are not inspected;
this is not a comprehensive WAF. Invalid policy denies. [Example](../examples/modules/waf.c),
[security boundary](storage-server.md#http-and-policies).
