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
staging=$(mktemp -d "${TMPDIR:-/tmp}/on1x-mcp-deploy.XXXXXXXX")
trap 'rm -rf -- "$staging"' EXIT
trap 'exit 1' HUP INT TERM
archive=$staging/release.tar
git -C "$repo" archive --format=tar "$revision" README.md MANIFEST.yaml include manual on1x >"$archive"

# Check the actual archive, not just the tree: export attributes can omit or
# rewrite files. Nothing from the working tree is added to the archive.
python3 -I -B - "$archive" "$repo" "$revision" "${BASH_SOURCE[0]}" <<'PY'
from pathlib import Path
import sys
import tarfile

archive_path, repo, revision, installer = sys.argv[1:]
errors = []
with tarfile.open(archive_path, "r:") as archive:
    for name in ("deploy_probe.py", "deploy.sh", "server.py", "core.py"):
        path = f"on1x/addons/mcp/{name}"
        try:
            member = archive.getmember(path)
        except KeyError:
            errors.append(f"{path}: missing from the archive")
            continue
        if not member.isfile():
            errors.append(f"{path}: not a regular file in the archive")
            continue
        local = Path(installer) if name == "deploy.sh" else Path(repo) / path
        if local.is_symlink() or not local.is_file():
            errors.append(f"{path}: not a regular file in the working tree")
            continue
        with archive.extractfile(member) as archived:
            if archived.read() != local.read_bytes():
                errors.append(f"{path}: working-tree contents differ from the archive")

if errors:
    sys.exit(
        f"Deployment preflight failed for HEAD {revision} (no SSH attempted):\n"
        + "\n".join(f"  {error}" for error in errors)
        + "\nUse an audited checkout with matching committed deployment files included in git archive."
    )
PY

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
addon=$release/on1x/addons/mcp
python3 -B "$addon/server.py" --root "$release" --revision "$revision" --check

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
ExecStart=/usr/bin/python3 -B $addon/server.py --root $release --revision $revision --port 8787 $host_options
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

python3 -B "$addon/deploy_probe.py" --host "$public_host"

systemctl is-active --quiet "$service"
systemctl enable "$service"
committed=1
echo "Deployed $revision; MCP discovery passed on the loopback backend."
if [[ -n $public_host ]]; then
    echo "After configuring the HTTPS reverse proxy to preserve Host: $public_host,"
    echo "use https://$public_host/mcp in ChatGPT (No authentication for approved public data)."
    echo "This deployment did not configure or verify public HTTPS."
else
    echo "Loopback only: http://127.0.0.1:8787/mcp is not reachable from ChatGPT."
    echo "Configure a Secure MCP Tunnel or redeploy with PUBLIC_MCP_HOST and configure HTTPS."
fi
REMOTE
)

# SSH joins its arguments into one command. Quote the script for the remote shell.
remote=${remote//\'/\'\\\'\'}
ssh -T -o BatchMode=yes -o StrictHostKeyChecking=yes "$target" \
    "bash -c '$remote' bash '$revision' '$public_host'" <"$archive"
