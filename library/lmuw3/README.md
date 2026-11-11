# lmuw3

Mune-hosted HTTP applications are **Weblets**. `lmuw3.lua` provides request
workflows, sessions, collections, JSON/YAML, forms and MicroChan channels.
The C engine embeds this module, so `require('lmuw3')` works independently of cwd.

```sh
make muw3
build/muw3 http://localhost:8000/
build/muw3 --weblet library/lmuw3/examples/request.lua http://localhost:8000/
make check-muw3
```

For C embedding, link the CMake `lmuw3` target (which links `mulib_microchan`),
or use the explicit implementation switches in `runtime.c` and
`../microchan/runtime.c` in your own implementation TU. Dependencies are all MuLIB
headers plus the C/POSIX runtime; no external Lua, curl or data-parser library.

* [Client manual](../../manual/mulib-muw3.md)
* [Weblet API and embedding](../../manual/mulib-muw3-mune.md)
* [Worker example](examples/worker.lua) and [IPC client](examples/client.lua)
* [MicroChan service](../microchan/README.md)

After editing Lua, run `python3 tools/embed_lmuw3.py`; `--check` verifies the snapshot.
HTTPS uses MuLIB's native TLS 1.3 client and system CA bundle by default.
Use `--cacert roots.pem` for an explicit trust bundle, including before
`--weblet`. Embedded clients can set `options.ca_file` or `options.trust`.
See the [TLS profile](../../manual/mulib-tls.md). `make check-tls` runs local
interoperability tests against Python/OpenSSL; neither is a runtime dependency.
