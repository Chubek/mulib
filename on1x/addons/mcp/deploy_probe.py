"""Check MCP discovery on loopback or through a Cloudflare Quick Tunnel."""
import argparse
from contextlib import closing
import http.client
import json
import re
import time

from core import MAX_RESPONSE

VERSION = "2025-11-25"


def probe(port, public_host="", *, quick_url=None):
    if quick_url is not None:
        match = re.fullmatch(
            r"https://([a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.trycloudflare\.com)/mcp",
            quick_url,
        )
        if not match or public_host:
            raise ValueError("expected a Quick Tunnel HTTPS /mcp URL without a Host override")
        host = match[1]
        # The default HTTPS context verifies the certificate and hostname.
        connection = http.client.HTTPSConnection(host, timeout=5)
    else:
        host = public_host or f"127.0.0.1:{port}"
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
    headers = {
        "Host": host,
        "Content-Type": "application/json",
        "Accept": "application/json, text/event-stream",
    }
    with closing(connection):
        def post(method, identifier=None, params=None):
            request = {"jsonrpc": "2.0", "method": method}
            if identifier is not None:
                request["id"] = identifier
            if params is not None:
                request["params"] = params
            connection.request("POST", "/mcp", json.dumps(request), headers)
            with connection.getresponse() as response:
                expected = 202 if identifier is None else 200
                if response.status != expected:
                    raise ValueError(f"{method}: HTTP {response.status} (expected {expected})")
                body = response.read(MAX_RESPONSE + 1)
            if identifier is None:
                if body:
                    raise ValueError(f"{method}: expected empty response")
                return
            if len(body) > MAX_RESPONSE:
                raise ValueError(f"{method}: response too large")
            message = json.loads(body)
            if (not isinstance(message, dict) or message.get("jsonrpc") != "2.0"
                    or message.get("id") != identifier or not isinstance(message.get("result"), dict)):
                raise ValueError(f"{method}: invalid JSON-RPC response")
            return message["result"]

        result = post("initialize", 1, {
            "protocolVersion": VERSION, "capabilities": {},
            "clientInfo": {"name": "deploy-check", "version": "1"},
        })
        info = result.get("serverInfo")
        if (result.get("protocolVersion") != VERSION or not isinstance(info, dict)
                or info.get("name") != "on1x-mulib-knowledge"):
            raise ValueError("initialize: unexpected server or protocol version")
        headers["MCP-Protocol-Version"] = VERSION
        post("notifications/initialized")
        tools = post("tools/list", 2).get("tools")
        if not isinstance(tools, list) or not {"search", "fetch"} <= {
                tool.get("name") for tool in tools
                if isinstance(tool, dict) and isinstance(tool.get("name"), str)}:
            raise ValueError("tools/list: missing search or fetch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--host", default="", help="Exact Host forwarded by the public proxy.")
    mode.add_argument("--quick-url", help="Verify discovery at a Cloudflare Quick Tunnel HTTPS URL.")
    args = parser.parse_args()
    for attempt in range(20):
        try:
            probe(8787, args.host, quick_url=args.quick_url)
            return
        except (OSError, http.client.HTTPException, ValueError) as exc:
            if attempt == 19:
                endpoint = "the Quick Tunnel" if args.quick_url is not None else "loopback port 8787"
                raise SystemExit(f"MCP discovery failed on {endpoint}: {exc}") from None
            time.sleep(1)


if __name__ == "__main__":
    main()
