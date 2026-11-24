#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 [--cloudflared | --backend-only] root@SERVER [PUBLIC_MCP_HOST]" >&2
    echo "Use --cloudflared instead of PUBLIC_MCP_HOST for a temporary public Quick Tunnel." >&2
    echo "Use --backend-only without PUBLIC_MCP_HOST to redeploy the existing backend without changing its Host or managing proxies." >&2
    exit 2
}
cloudflared=0
backend_only=0
positional=()
for arg in "$@"; do
    case "$arg" in
        --cloudflared) (( ! cloudflared )) || usage; cloudflared=1 ;;
        --backend-only) (( ! backend_only )) || usage; backend_only=1 ;;
        -*) usage ;;
        *) positional+=("$arg") ;;
    esac
done
if (( ${#positional[@]} < 1 || ${#positional[@]} > 2 ||
      (cloudflared && backend_only) ||
      ((cloudflared || backend_only) && ${#positional[@]} == 2) )); then
    usage
fi
target=${positional[0]}
public_host=${positional[1]:-}
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
git -C "$repo" archive --format=tar "$revision" README.md MANIFEST.yaml kernels manual on1x >"$archive"

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
quick=$3
backend_only=$4
service=on1x-knowledge.service
tunnel=on1x-knowledge-tunnel.service
tunnel_host=on1x-mcp-tunnel.invalid:8787
unit=/etc/systemd/system/$service
base=/opt/on1x-knowledge

[[ $(id -u) == 0 ]] || { echo "Root access is required." >&2; exit 1; }
systemctl show --property=Version --value >/dev/null
python3 -c 'import sys; sys.exit("Python 3.11+ required") if sys.version_info < (3, 11) else None'
if (( quick )); then
    cloudflared_bin=$(type -P cloudflared || true)
    if [[ ! $cloudflared_bin =~ ^/[A-Za-z0-9_./+-]+$ || ! -x $cloudflared_bin ]]; then
        echo "Install cloudflared on the remote host before using --cloudflared." >&2
        exit 1
    fi
    command -v systemd-run journalctl timeout >/dev/null
fi
install -d -m 0755 "$base"
exec 9>"$base/.deploy.lock"
flock -n 9 || { echo "Another deployment is running." >&2; exit 1; }

release=$(mktemp -d "$base/$revision.XXXXXXXX")
work=$(mktemp -d "$base/.install.XXXXXXXX")
installed=0
committed=0
tunnel_started=0
was_active=0
was_enabled=0
cleanup() {
    status=$?
    trap - EXIT
    if (( ! committed )); then
        if (( tunnel_started )); then
            systemctl stop "$tunnel" || echo "Could not stop $tunnel; stop it manually." >&2
        fi
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
origin_host=$public_host
if [[ -e $unit ]]; then cp -- "$unit" "$work/previous"; fi
if (( backend_only )); then
    # Recover the exact Host from the installer-generated unit, not the proxy.
    # Never source the unit or reuse unchecked text in the new ExecStart.
    previous_exec=$(grep '^ExecStart=' "$work/previous" 2>/dev/null) || previous_exec=
    managed_exec='^ExecStart=/usr/bin/python3 -B [^[:space:]]+/on1x/addons/mcp/server\.py'
    managed_exec+=' --root [^[:space:]]+ --revision [0-9a-f]+ --port 8787'
    managed_exec+='( --public-data --allowed-host ([a-z0-9]+([.-][a-z0-9]+)*(:[0-9]{1,5})?))?[[:blank:]]*$'
    if [[ ! $previous_exec =~ $managed_exec ]]; then
        echo "--backend-only requires an existing installer-managed $service with a recognized ExecStart." >&2
        exit 1
    fi
    origin_host=${BASH_REMATCH[2]}
elif (( quick )); then
    origin_host=$tunnel_host
fi
if [[ -n $origin_host ]]; then
    host_options="--public-data --allowed-host $origin_host"
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

if systemctl is-active --quiet "$service"; then was_active=1; fi
if systemctl is-enabled --quiet "$service"; then was_enabled=1; fi
# Do not leave a previous public URL forwarding to a replacement backend.
# A Quick Tunnel cannot resume the old public URL after being stopped.
# Backend-only deployments deliberately keep existing forwarding in place.
if (( ! backend_only )) && systemctl is-active --quiet "$tunnel"; then
    systemctl stop "$tunnel"
fi
installed=1
install -m 0644 "$work/$service" "$unit"
systemctl daemon-reload
systemctl restart "$service"

python3 -B "$addon/deploy_probe.py" --host "$origin_host"

if (( quick )); then
    echo "Starting a temporary, unauthenticated public Cloudflare Quick Tunnel." >&2
    tunnel_started=1
    systemd-run --quiet --unit="$tunnel" --collect \
        --property=Type=exec --property=DynamicUser=yes --property=NoNewPrivileges=yes \
        --property=ProtectSystem=strict --property=ProtectHome=yes --property=PrivateTmp=yes \
        --property=TimeoutStopSec=10s --property=StandardOutput=journal --property=StandardError=journal \
        "$cloudflared_bin" tunnel --config /dev/null --no-autoupdate --metrics 127.0.0.1:0 \
        --url http://127.0.0.1:8787 --http-host-header "$tunnel_host"
    invocation=$(systemctl show --property=InvocationID --value "$tunnel")
    [[ $invocation =~ ^[a-f0-9]{32}$ && $invocation != 00000000000000000000000000000000 ]] || {
        echo "Could not identify the running Quick Tunnel. See: journalctl -u $tunnel" >&2
        exit 1
    }
    tunnel_url=
    url_pattern='(^|[[:space:]|])(https://[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?\.trycloudflare\.com)([[:space:]|]|$)'
    for ((attempt=0; attempt<30; ++attempt)); do
        systemctl is-active --quiet "$tunnel" || break
        logs=$(journalctl --quiet --no-pager --output=cat --lines=100 "_SYSTEMD_INVOCATION_ID=$invocation")
        if [[ $logs =~ $url_pattern ]]; then
            tunnel_url=${BASH_REMATCH[2]}/mcp
            break
        fi
        sleep 1
    done
    if [[ -z $tunnel_url ]]; then
        echo "Cloudflare did not report a Quick Tunnel URL. See: journalctl -u $tunnel" >&2
        exit 1
    fi
    if ! timeout 45s python3 -B "$addon/deploy_probe.py" --quick-url "$tunnel_url"; then
        echo "MCP discovery failed through the Quick Tunnel. See: journalctl -u $tunnel" >&2
        exit 1
    fi
    systemctl is-active --quiet "$tunnel"
fi

systemctl is-active --quiet "$service"
systemctl enable "$service"
committed=1
echo "Deployed $revision; MCP discovery passed on the loopback backend."
if (( backend_only )); then
    echo "Existing tunnels and proxies were left unchanged; public access was not checked."
elif (( quick )); then
    echo "Cloudflare MCP URL: $tunnel_url"
    echo "Use this URL in ChatGPT with No authentication. Anyone with the URL can access it."
    echo "Temporary URL; to stop it on the server: systemctl stop $tunnel"
elif [[ -n $public_host ]]; then
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
    "bash -c '$remote' bash '$revision' '$public_host' '$cloudflared' '$backend_only'" <"$archive"
