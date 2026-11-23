#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 root@SERVER [PUBLIC_MCP_HOST]" >&2
    exit 2
fi
target=$1
public_host=${2:-}
if [[ ! $target =~ ^root@[A-Za-z0-9][A-Za-z0-9.-]*$ ||
      ( -n $public_host && ! $public_host =~ ^[a-z0-9]+([.-][a-z0-9]+)*$ ) ]]; then
    echo "Expected root@SERVER and, optionally, a lowercase MCP hostname." >&2
    exit 2
fi

# Deploy committed sources so the service can cite their exact revision.
repo=$(git -C "$(dirname -- "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)
revision=$(git -C "$repo" rev-parse HEAD)
remote=$(cat <<'REMOTE'
set -euo pipefail
umask 022
revision=$1
public_host=$2
service=on1x-knowledge.service
unit=/etc/systemd/system/$service
base=/opt/on1x-knowledge

[[ $(id -u) == 0 ]] || { echo "Root access is required." >&2; exit 1; }
systemctl show --property=Version --value >/dev/null
python3 -c 'import sys; sys.exit("Python 3.11+ required") if sys.version_info < (3, 11) else None'
install -d -m 0755 "$base"
exec 9>"$base/.deploy.lock"
flock -n 9 || { echo "Another deployment is running." >&2; exit 1; }

release=$(mktemp -d "$base/$revision.XXXXXXXX")
work=$(mktemp -d "$base/.install.XXXXXXXX")
installed=0
committed=0
was_active=0
was_enabled=0
cleanup() {
    status=$?
    trap - EXIT
    if (( ! committed )); then
        if (( installed )); then
            set +e
            systemctl stop "$service"
            if (( ! was_enabled )); then systemctl disable "$service"; fi
            if [[ -f $work/previous ]]; then
                install -m 0644 "$work/previous" "$unit"
            else
                rm -f -- "$unit"
            fi
            systemctl daemon-reload
            if (( was_active )); then systemctl start "$service"; fi
        fi
        rm -rf -- "$release"
    fi
    rm -rf -- "$work"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

tar -xf - -C "$release"
chmod 0755 "$release"
addon=$release/on1x/addons/chatgpt/server.py
python3 -B "$addon" --root "$release" --revision "$revision" --check

host_options=
if [[ -n $public_host ]]; then
    host_options="--public-data --allowed-host $public_host"
fi
cat >"$work/$service" <<UNIT
[Unit]
Description=On1x MuLIB knowledge MCP server

[Service]
Type=simple
DynamicUser=yes
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
IPAddressDeny=any
IPAddressAllow=localhost
ExecStart=/usr/bin/python3 -B $addon --root $release --revision $revision --port 8787 $host_options
Restart=on-failure

[Install]
WantedBy=multi-user.target
UNIT

if [[ -e $unit ]]; then cp -- "$unit" "$work/previous"; fi
if systemctl is-active --quiet "$service"; then was_active=1; fi
if systemctl is-enabled --quiet "$service"; then was_enabled=1; fi
installed=1
install -m 0644 "$work/$service" "$unit"
systemctl daemon-reload
systemctl restart "$service"

python3 - <<'PY'
import json
import time
from urllib.error import URLError
from urllib.request import ProxyHandler, Request, build_opener

request = Request(
    "http://127.0.0.1:8787/mcp",
    data=json.dumps({
        "jsonrpc": "2.0", "id": 1, "method": "initialize",
        "params": {"protocolVersion": "2025-11-25", "capabilities": {},
                   "clientInfo": {"name": "deploy-check", "version": "1"}},
    }).encode(),
    headers={"Content-Type": "application/json",
             "Accept": "application/json, text/event-stream"},
)
opener = build_opener(ProxyHandler({}))
for attempt in range(20):
    try:
        with opener.open(request, timeout=2) as response:
            result = json.load(response)["result"]
        if result["serverInfo"]["name"] == "on1x-mulib-knowledge":
            break
    except (URLError, TimeoutError, ValueError, KeyError):
        pass
    time.sleep(1)
else:
    raise SystemExit("MCP initialize failed on port 8787")
PY

systemctl is-active --quiet "$service"
systemctl enable "$service"
committed=1
echo "Deployed $revision at http://127.0.0.1:8787/mcp on the server."
REMOTE
)

# SSH joins its arguments into one command. Quote the script for the remote shell.
remote=${remote//\'/\'\\\'\'}
git -C "$repo" archive "$revision" README.md MANIFEST.yaml include manual on1x |
    ssh -T -o BatchMode=yes -o StrictHostKeyChecking=yes "$target" \
        "bash -c '$remote' bash '$revision' '$public_host'"