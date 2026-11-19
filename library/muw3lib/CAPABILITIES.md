# MuW3 adapter capabilities

| API | Supported subset | Not included |
|---|---|---|
| REST | HTTP(S) methods, headers, query/form fields, buffered JSON or binary bodies | HTTP caching, retries, automatic page traversal |
| Fetch-style adapter | Synchronous request, including non-2xx HTTP responses | Browser Fetch, CORS, promises, streaming and abort signals |
| GraphQL | JSON-over-HTTP POST, envelope and error handling | Query/schema validation, subscriptions and incremental delivery |
| JSON-RPC | HTTP JSON-RPC 2.0 requests, notifications and correlated replies | Non-HTTP transport and method hosting |
| SOAP | SOAP envelopes and raw XML over HTTP | XML parsing, WSDL, schemas, WS-Security, automatic Fault processing |
| SSE | Complete buffered `text/event-stream` bodies | Live subscription, automatic reconnect and replay |
| gRPC | Unsupported in Mune | No HTTP/2 gRPC, gRPC-Web or Protobuf binding |
| WebSocket | Unsupported in Mune | No upgraded duplex connection binding |
| GitHub, GitLab, Google Maps, OpenAI, OpenCode V2, OpenWeather, Slack, Stripe, Twilio and Wikipedia | Selected convenience operations; see [README.md](README.md) | Complete provider SDKs and live-service compatibility certification |
| DOM, Web Storage, IndexedDB, Service Workers, Canvas, WebGL, WebRTC, Web Audio, Geolocation, Web Crypto, File and Workers | Unsupported browser APIs | No browser execution context, permission model, rendering, storage or event loop |

A default MuW3 Weblet has a bounded synchronous HTTP transport, not a
browser. API capability lookup is available through `muw3lib.browser` and
`muw3lib.unsupported`. The [native C protocol kernels](../../manual/api-kernels.md)
have a different feature set; including those C headers does not expose
new APIs to a Weblet.
