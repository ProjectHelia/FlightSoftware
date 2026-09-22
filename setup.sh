#!/usr/bin/env bash
# HELIA flight software 
set -euo pipefail

FIRMWARE=0
for arg in "$@"; do
    case "$arg" in
        --firmware) FIRMWARE=1 ;;
        -h|--help)  sed -n '2,7p' "$0"; exit 0 ;;
        *)          echo "Unknown option: $arg (try --help)"; exit 1 ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

step() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
ok()   { printf '    \033[32m✓\033[0m %s\n' "$*"; }
warn() { printf '    \033[33m!\033[0m %s\n' "$*"; }
die()  { printf '\n\033[31mError:\033[0m %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

step "Installing system packages"

case "$(uname -s)" in
    Darwin)
        if ! xcode-select -p >/dev/null 2>&1; then
            xcode-select --install || true
            die "Finish installing the Xcode Command Line Tools in the pop-up, then re-run this script"
        fi
        ok "Xcode Command Line Tools"
        have brew || die "Homebrew is required: https://brew.sh"
        brew install cmake git python3 pipx doxygen graphviz
        ;;
    Linux)
        SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
        if have apt-get; then
            $SUDO apt-get update
            $SUDO apt-get install -y build-essential cmake git python3 python3-venv pipx doxygen graphviz
        elif have dnf; then
            $SUDO dnf install -y gcc gcc-c++ make cmake git python3 pipx doxygen graphviz libasan libubsan
        elif have pacman; then
            $SUDO pacman -S --needed --noconfirm base-devel cmake git python python-pipx doxygen graphviz
        else
            die "Can't find your package manager :( Please Install manually: C/C++ compiler, cmake, git, python3, pipx, doxygen, graphviz."
        fi
        ;;
    *)
        die "Unsupported OS: $(uname -s) :( 
        use macOS or Linux (on Windows, use WSL2"
        ;;
esac
ok "System packages"

step "Installing PlatformIO"

# pipx puts tools in ~/.local/bin; the VS Code extension uses ~/.platformio/penv/bin
# for nvim platofmrio uses a different path I believe
export PATH="$HOME/.local/bin:$HOME/.platformio/penv/bin:$PATH"

if have pio; then
    ok "PlatformIO already installed ($(pio --version))"
else
    pipx install platformio
    pipx ensurepath >/dev/null
    ok "PlatformIO installed ($(pio --version))"
fi

if [ "$(uname -s)" = "Linux" ]; then
    step "Checking USB serial access"
    SERIAL_GROUP="dialout"
    getent group uucp >/dev/null && ! getent group dialout >/dev/null && SERIAL_GROUP="uucp"
    if id -nG "$USER" | grep -qw "$SERIAL_GROUP"; then
        ok "$USER is in the $SERIAL_GROUP group"
    else
        ${SUDO:-} usermod -aG "$SERIAL_GROUP" "$USER"
        warn "Added $USER to $SERIAL_GROUP. Log out and back in before flashing boards."
    fi
fi

step "Building and running host tests"

cmake -B build -DHELIA_HOST=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
ok "Host tests pass"

if [ "$FIRMWARE" -eq 1 ]; then
    step "Downloading ESP32 toolchain and building firmware "
    pio run
    ok "Firmware builds"
fi

step "Done"
cat <<MSG
    Run the tests:     cmake --build build && ctest --test-dir build
    Build firmware:    pio run -e eps
    Flash + monitor:   pio run -e eps -t upload && pio device monitor -e eps

If 'pio' isn't found in a new terminal and open a new shell ;)


- Love Remy
MSG

