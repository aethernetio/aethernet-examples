#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

DEP="build-debug/_deps/aether-client-cpp-src"
PATCH="$SCRIPT_DIR/aether-prepared-packet.patch"

if [ ! -d "$DEP" ]; then
  echo "[error] $DEP does not exist. Run cmake configure first."
  exit 1
fi

if [ -f "$DEP/aether/prepared_packet/packet_encoder.h" ]; then
  echo "[ok] prepared-packet patch already appears to be applied"
  exit 0
fi

git -C "$DEP" apply "$PATCH"
echo "[ok] applied prepared-packet patch to $DEP"
