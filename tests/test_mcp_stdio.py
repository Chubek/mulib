#!/usr/bin/env python3
"""Exercise real newline-delimited MCP over a child process using Python JSON."""
import argparse
import json
import subprocess


def request(serial, method, params=None):
    message = {"jsonrpc": "2.0", "method": method}
    if serial is not None:
        message["id"] = serial
    if params is not None:
        message["params"] = params
    return json.dumps(message).encode("utf-8")


def exchange(server, frames):
    child = subprocess.run([server], input=b"\n".join(frames) + b"\n",
                           capture_output=True, timeout=10, check=True)
    assert child.stdout.endswith(b"\n"), child.stdout
    return [json.loads(frame) for frame in child.stdout.splitlines()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", required=True)
    parser.add_argument("--client", required=True)
    args = parser.parse_args()

    probe = subprocess.check_output([args.client], timeout=10).removesuffix(b"\n")
    read = subprocess.check_output([args.client, "--read"], timeout=10).removesuffix(b"\n")
    meta = {"_meta": {"io.modelcontextprotocol/protocolVersion": "2026-07-28",
                      "io.modelcontextprotocol/clientCapabilities": {}}}
    results = exchange(args.server, [
        probe,
        request("list", "tools/list", meta),
        request(None, "tools/call", {**meta, "name": "greet"}),
        request(7, "tools/call", {**meta, "name": "greet", "arguments": {}}),
        request(8, "tools/call", {**meta, "name": "greet", "arguments": {"x": "a\nb"}}),
        request("resources", "resources/list", meta),
        read,
        request(9, "resources/read", {**meta, "uri": "demo:/unknown"}),
        request(10, "resources/templates/list", meta),
        request(11, "resources/templates/list", {**meta, "cursor": "not-issued"}),
    ])
    assert [response["id"] for response in results] == [
        json.loads(probe)["id"], "list", 7, 8, "resources", json.loads(read)["id"], 9, 10, 11]
    discover, listed, greeted, invalid, resources, content = [
        response["result"] for response in results[:6]]
    assert discover["supportedVersions"] == ["2026-07-28", "2025-11-25"]
    assert discover["capabilities"] == {"tools": {}, "resources": {}}
    assert all(r["resultType"] == "complete" for r in (discover, listed, greeted, invalid))
    assert listed["ttlMs"] == 0 and listed["cacheScope"] == "private"
    assert listed["tools"][0]["inputSchema"] == {"type": "object", "additionalProperties": False}
    assert greeted["content"] == [{"type": "text", "text": "Hello"}]
    assert invalid["isError"] and invalid["content"][0]["text"] == "Expected no arguments"
    assert resources["resources"] == [{"name": "Greeting", "uri": "demo:/greeting", "mimeType": "text/plain"}]
    assert content["contents"] == [{"uri": "demo:/greeting", "mimeType": "text/plain", "text": "Hello"}]
    assert results[6]["error"]["code"] == -32602
    assert results[7]["result"]["resourceTemplates"] == []
    assert results[8]["error"]["code"] == -32602

    legacy = exchange(args.server, [
        request(1, "initialize", {"protocolVersion": "2025-11-25",
                                  "capabilities": {},
                                  "clientInfo": {"name": "python", "version": "1"}}),
        request(None, "notifications/initialized"),
        request(2, "ping"),
        request(3, "tools/call", {"name": "greet"}),
        request(4, "resources/read", {"uri": "demo:/greeting"}),
    ])
    assert [r["id"] for r in legacy] == [1, 2, 3, 4]
    assert legacy[0]["result"]["protocolVersion"] == "2025-11-25"
    assert legacy[1]["result"] == {}
    assert legacy[2]["result"]["content"][0]["text"] == "Hello"
    assert legacy[3]["result"]["contents"][0]["text"] == "Hello"
    assert "ttlMs" not in legacy[3]["result"]


if __name__ == "__main__":
    main()
