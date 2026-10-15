#!/usr/bin/env sh
set -eu

# WriteCam One-Click Installer (Arch Linux and derivatives)
# Usage:
#   curl -fsSL https://razodin137.github.io/omawrite-personal/install.sh | sh
#   or
#   curl -fsSL https://raw.githubusercontent.com/razodin137/omawrite-personal/master/install.sh | sh

# If running via pipe (curl | sh), restore interactive terminal for sudo/prompts
if [ -e /dev/tty ]; then
  exec < /dev/tty 2>/dev/null || true
fi

# makepkg must not be run as root
if [ "$(id -u)" -eq 0 ]; then
  echo "Error: Do not run this installer as root or with sudo." >&2
  echo "Run it as your regular user; sudo password will be requested by pacman when installing." >&2
  exit 1
fi

# Verify Arch Linux environment (pacman & makepkg)
if ! command -v makepkg >/dev/null 2>&1 || ! command -v pacman >/dev/null 2>&1; then
  echo "===================================================================" >&2
  echo " WriteCam One-Click Installer: Arch Linux Only" >&2
  echo "===================================================================" >&2
  echo "This one-click installer is designed specifically for Arch Linux" >&2
  echo "and Arch-based distributions (EndeavourOS, Manjaro, CachyOS, etc.)" >&2
  echo "which use Arch's 'makepkg' and 'pacman'." >&2
  echo "" >&2
  echo "For other distributions (Ubuntu, Fedora, Debian), clone and build:" >&2
  echo "  git clone https://github.com/razodin137/omawrite-personal.git" >&2
  echo "  cd omawrite-personal && ./bin/build" >&2
  echo "===================================================================" >&2
  exit 1
fi

if ! command -v git >/dev/null 2>&1; then
  echo "Error: 'git' is required to download WriteCam." >&2
  echo "Install it with: sudo pacman -S git" >&2
  exit 1
fi

REPO_URL="https://github.com/razodin137/omawrite-personal.git"
TEMP_DIR="$(mktemp -d /tmp/writecam-install-XXXXXX)"

cleanup() {
  rm -rf "$TEMP_DIR"
}
trap cleanup EXIT INT TERM

echo "==> Downloading WriteCam repository..."
git clone --depth 1 "$REPO_URL" "$TEMP_DIR/writecam"

echo "==> Building and installing WriteCam..."
cd "$TEMP_DIR/writecam"
./bin/install

echo ""
echo "==> ✓ WriteCam successfully installed!"
echo "Launch it with 'writecam' or search for WriteCam in your application launcher."
