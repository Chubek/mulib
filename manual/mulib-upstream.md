# Upstream endpoints

Define `MULIB_IMPL_UPSTREAM`. Host names are borrowed, ports validated on
availability checks. Healthy endpoints with free capacity can be acquired;
release refuses underflow. Zero max_active means unlimited. No sockets/DNS.
[Example](../examples/modules/upstream.c), [contracts](storage-server.md).
