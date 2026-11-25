"""Run only inside the Cephyr MCP sandbox. Never import or load SDK hooks."""

import json
import resource
import sys

MAX_RESULT = 20 * 1024
MAX_DIAGNOSTICS = 12
DRIVER_ARGS = ("-x", "c", "-std=c11", "--target=x86_64-linux-gnu", "-nostdinc",
               "--diagnostics-format=sarif", "-")
ROOTS = {"resource_root": "/toolchain/cephyr", "archon_root": "/toolchain/archon"}


def _limit(which, value):
    _, hard = resource.getrlimit(which)
    if hard != resource.RLIM_INFINITY:
        value = min(hard, value)
    resource.setrlimit(which, (value, value))


def _diagnostics(raw):
    """Project SARIF onto a small location/message schema; never follow its URIs."""
    if not raw:
        return [], False
    if len(raw) > 128 * 1024:
        return [], True
    items, truncated = [], False
    try:
        for run in json.loads(raw)["runs"]:
            for result in run["results"]:
                if len(items) == MAX_DIAGNOSTICS:
                    return items, True
                code = result["ruleId"]
                message = result["message"]["text"]
                level = result["level"]
                truncated |= len(code) > 100 or len(message) > 384
                item = {"code": code[:100], "message": message[:384],
                        "severity": level if level in ("error", "warning", "note") else "note"}
                locations = result.get("locations")
                if locations:
                    region = locations[0]["physicalLocation"].get("region", {})
                    for key, name in (("startLine", "line"), ("startColumn", "column")):
                        value = region.get(key)
                        if type(value) is int and 0 < value <= 2147483647:
                            item[name] = value
                items.append(item)
    except (TypeError, KeyError, ValueError, RecursionError, AttributeError):
        return items, True
    return items, truncated


def main():
    _limit(resource.RLIMIT_CORE, 0)
    _limit(resource.RLIMIT_AS, 1024 * 1024 * 1024)
    _limit(resource.RLIMIT_CPU, 5)
    _limit(resource.RLIMIT_FSIZE, 64 * 1024)
    _limit(resource.RLIMIT_NOFILE, 32)
    _limit(resource.RLIMIT_NPROC, 0)

    source = sys.stdin.buffer.read(8193)
    if not source or len(source) > 8192 or b"\0" in source:
        return 2
    sys.path.insert(0, "/app")
    import cephyr

    compiler = cephyr.Compiler(library="/toolchain/libcephyr.so")
    if sys.argv[1:] == ["parse_c11"]:
        result = compiler.parse_c11(DRIVER_ARGS, source, format="sexp", **ROOTS)
    elif sys.argv[1:] == ["emit_interface"]:
        result = compiler.emit_interface(DRIVER_ARGS, source, **ROOTS)
    elif sys.argv[1:] == ["emit_mir"]:
        result = compiler.emit_mir(DRIVER_ARGS, source, **ROOTS)
    else:
        return 2
    if result.status not in (cephyr.Status.OK, cephyr.Status.COMPILE_ERROR):
        return 2
    if len(result.output) > MAX_RESULT:
        return 3
    diagnostics, truncated = _diagnostics(result.diagnostics)
    if not result.ok and not diagnostics:
        diagnostics = [{"severity": "error", "code": "cephyr.compile_error",
                        "message": "Cephyr rejected the source without a usable diagnostic."}]
    response = json.dumps({
        "status": "ok" if result.ok else "compile_error",
        "output": result.output.decode("utf-8"),
        "diagnostics": diagnostics,
        "diagnostics_truncated": truncated,
    }, ensure_ascii=True, separators=(",", ":"), allow_nan=False).encode("ascii")
    if len(response) > MAX_RESULT:
        return 3
    sys.stdout.buffer.write(response)
    return 0


if __name__ == "__main__":
    sys.exit(main())
