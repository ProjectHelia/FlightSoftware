#!/usr/bin/env bash
# Build + test everything, flash every board, then monitor Master.
# Each board's port is set in platformio.ini, so each firmware goes to the right board.
set -euo pipefail
cd "$(dirname "$0")"
export PATH="$HOME/.local/bin:$HOME/.platformio/penv/bin:$PATH"

NODES="master"   # add nodes as they're enabled in platformio.ini; keep master last

cmake -B build -DHELIA_HOST=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

for node in $NODES; do
    echo "==> Build + flash: $node"
    pio run -e "$node" -t upload
done

pio device monitor -e master