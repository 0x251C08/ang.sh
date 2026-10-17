#!/bin/sh
# angsh installer
#
#   curl -fsSL https://raw.githubusercontent.com/0x251C08/ang.sh/main/install.sh | sh
#
# Installs the prebuilt static binary from GitHub Releases.
# Flags (with curl, pass them after -s --):
#   sh -s -- --version v0.1.0 --dir ~/bin
set -eu

REPO="0x251C08/ang.sh"
VERSION=""
DEST=""

# ---- pretty output (only when stdout is a terminal) ------------------------

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    B=$(printf '\033[1m'); G=$(printf '\033[32m'); R=$(printf '\033[31m')
    Y=$(printf '\033[33m'); D=$(printf '\033[2m'); N=$(printf '\033[0m')
else
    B='' G='' R='' Y='' D='' N=''
fi
info() { printf '%s\n' "$*"; }
ok()   { printf '%s✓%s %s\n' "$G" "$N" "$*"; }
warn() { printf '%s!%s %s\n' "$Y" "$N" "$*" >&2; }
die()  { printf '%s✗%s %s\n' "$R" "$N" "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
angsh installer

Usage: install.sh [--version TAG] [--dir PATH]

  --version TAG   install a specific release (default: latest), e.g. v0.1.0
  --dir PATH      where to put the binary
                  (default: /usr/local/bin if writable, else ~/.local/bin)
  -h, --help      show this help

Examples:
  curl -fsSL https://raw.githubusercontent.com/0x251C08/ang.sh/main/install.sh | sh
  curl -fsSL https://raw.githubusercontent.com/0x251C08/ang.sh/main/install.sh | sh -s -- --version v0.1.0
EOF
}

# ---- args -------------------------------------------------------------------

while [ $# -gt 0 ]; do
    case "$1" in
        --version) VERSION="${2:?--version needs a tag}"; shift 2 ;;
        --dir)     DEST="${2:?--dir needs a path}";       shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; die "unknown option: $1" ;;
    esac
done

command -v curl >/dev/null 2>&1 || die "curl is required"

# ---- platform ----------------------------------------------------------------

os=$(uname -s)
arch=$(uname -m)
case "$os" in
    Linux)  os_part=linux ;;
    Darwin) os_part=darwin ;;
    *) die "no prebuilt binary for $os yet — build from source: https://github.com/$REPO#-install" ;;
esac
case "$arch" in
    x86_64|amd64) arch_part=x86_64 ;;
    aarch64|arm64) arch_part=$arch ;;  # keep native spelling: aarch64 (Linux) / arm64 (macOS)
    *) die "no prebuilt binary for $arch yet — build from source: https://github.com/$REPO#-install" ;;
esac

# ---- resolve version -----------------------------------------------------------

if [ -z "$VERSION" ]; then
    info "${D}resolving latest release…${N}"
    VERSION=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
        | sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
        | head -n 1)
    [ -n "$VERSION" ] || die "could not resolve the latest release tag"
fi
asset="angsh-$VERSION-$arch_part-$os_part"
base="https://github.com/$REPO/releases/download/$VERSION"

# ---- download ------------------------------------------------------------------

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM

info "${D}downloading${N} $asset ${D}($VERSION)…${N}"
curl -fsSL -o "$tmp/$asset" "$base/$asset" \
    || die "download failed — does $VERSION have a $arch_part-$os_part asset? (https://github.com/$REPO/releases)"
curl -fsSL -o "$tmp/$asset.sha256" "$base/$asset.sha256" \
    || die "checksum file not found for $asset"

# ---- verify ----------------------------------------------------------------------

if command -v sha256sum >/dev/null 2>&1; then
    (cd "$tmp" && sha256sum -c "$asset.sha256") >/dev/null 2>&1 \
        || die "checksum verification failed — download may be corrupt"
elif command -v shasum >/dev/null 2>&1; then
    (cd "$tmp" && shasum -a 256 -c "$asset.sha256") >/dev/null 2>&1 \
        || die "checksum verification failed — download may be corrupt"
else
    warn "no sha256sum/shasum found — skipping checksum verification"
fi
ok "checksum verified"

# ---- install ----------------------------------------------------------------------

if [ -z "$DEST" ]; then
    if [ -w /usr/local/bin ]; then
        DEST=/usr/local/bin
    else
        DEST="$HOME/.local/bin"
    fi
fi
mkdir -p "$DEST" || die "cannot create $DEST"
[ -w "$DEST" ] || die "$DEST is not writable — rerun with --dir <path> or fix permissions"

cp "$tmp/$asset" "$DEST/angsh"
chmod 755 "$DEST/angsh"
ok "installed to $B$DEST/angsh$N"

# ---- sanity check + PATH hint -------------------------------------------------------

"$DEST/angsh" --version >/dev/null 2>&1 || die "installed binary failed to run"
case ":$PATH:" in
    *":$DEST:"*) ;;
    *) warn "$DEST is not in your PATH. Add this to your shell config:"
       info "    export PATH=\"$DEST:\$PATH\"" ;;
esac

info ""
ok "angsh $VERSION is ready — run ${B}${G}angsh${N}"
info "${D}  try: ${N}angsh -c 'echo hello | tr a-z A-Z'${D}   ·   ${N}theme rice${D} inside${N}"
