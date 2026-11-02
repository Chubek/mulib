# Incremental HTTP connection core

Historical filename retained; define `MULIB_IMPL_WEB_CONNECTION`, `HTTP1` and
the HTTP closure. Zero-initialize, set byte limit/options, feed bounded chunks,
then next parses/removes one message. EOF is permanent; pipelined data remains.
No sockets, TLS or timeout enforcement. [Example](../examples/modules/web_conntection.c),
[contracts](storage-server.md#http-and-policies).
