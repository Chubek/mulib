#!/usr/bin/env python3
"""Generate the supported OpenCode V2 route metadata from the pinned OpenAPI."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
SPEC = ROOT / "standards/opencode-v2/openapi.json"
TARGET = ROOT / "include/detail/mulib-opencode-operations.inc"

# Deliberate client surface; no raw routes, shell, credentials, permission
# changes, or server/process management. The scripting binding is GET-only.
OPERATIONS = """
server.info location.get agent.list agent.get
session.list session.create session.active session.get session.update
session.prompt session.interrupt session.context session.diff
session.message.get session.message.list experimental.session.log
model.list model.default provider.list provider.get project.list
integration.list integration.get mcp.list mcp.resource.catalog
command.list skill.list reference.list worktree.list vcs.get vcs.base
vcs.status vcs.branch.list vcs.diff websearch.providers event.subscribe
experimental.usage.limits
""".split()

SESSION_WRITES = {
    "session.create": "POST",
    "session.update": "PATCH",
    "session.prompt": "POST",
    "session.interrupt": "POST",
}


def generate():
    source = SPEC.read_bytes()
    spec = json.loads(source)
    routes = {}
    for route, path in spec["paths"].items():
        for method, operation in path.items():
            name = operation["operationId"]
            if name in routes:
                raise ValueError(f"duplicate operation ID: {name}")
            routes[name] = method.upper(), route, operation
    lines = [f"/* Generated from {SPEC.relative_to(ROOT)}; do not edit.",
             f" * SHA-256: {hashlib.sha256(source).hexdigest()} */"]
    for name in sorted(OPERATIONS):
        method, route, op = routes[name]
        if method != SESSION_WRITES.get(name, "GET"):
            raise ValueError(f"unexpected HTTP method for {name}: {method}")
        query = []
        for param in op.get("parameters", []):
            if param["in"] == "path":
                if not param.get("required") or "{" + param["name"] + "}" not in route:
                    raise ValueError(f"unsupported path parameter in {name}")
            elif param["in"] == "query":
                key = param["name"]
                if param.get("style") == "deepObject":
                    if not param["explode"] or set(param["schema"]["properties"]) != {"directory"}:
                        raise ValueError(f"unsupported deepObject in {name}")
                    key += "[directory]"
                elif any(item.get("type") == "array" for item in param["schema"].get("anyOf", [])):
                    key += "*"
                if not re.fullmatch(r"[A-Za-z0-9_]+(?:\[directory\]|\*)?", key):
                    raise ValueError(f"unsupported query parameter in {name}")
                if param.get("required"):
                    if key.endswith("*"):
                        raise ValueError(f"unsupported required repeated query parameter in {name}")
                    key += "!"
                query.append(key)
            else:
                raise ValueError(f"unsupported parameter in {name}")
        paths = set(re.findall(r"\{([^}]+)\}", route))
        if paths != {p["name"] for p in op.get("parameters", []) if p["in"] == "path"} or "*" in route:
            raise ValueError(f"unsupported route {route}")
        body = op.get("requestBody")
        if body and (set(body["content"]) != {"application/json"} or not body["required"]):
            raise ValueError(f"unsupported request body in {name}")
        required = [] if not body else body["content"]["application/json"]["schema"].get("required", [])
        successes = [(int(k), v) for k, v in op["responses"].items() if k.startswith("2")]
        if len(successes) != 1:
            raise ValueError(f"unsupported success statuses in {name}")
        status, response = successes[0]
        media = list(response.get("content", {}))
        if media not in ([], ["application/json"], ["text/event-stream"]):
            raise ValueError(f"unsupported response body in {name}")
        flags = (1 if body else 0) | (2 if media == ["text/event-stream"] else 0)
        attrs = [name, method, route, ",".join(query), ",".join(required)]
        lines.append("  {" + ", ".join(map(json.dumps, attrs)) + f", {status}, {flags}}},")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output = generate()
    if args.check:
        if not TARGET.is_file() or TARGET.read_text() != output:
            parser.exit(1, "OpenCode route catalog is stale\n")
    else:
        TARGET.write_text(output)


if __name__ == "__main__":
    main()
