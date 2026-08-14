#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if ! command -v emcmake >/dev/null 2>&1; then
  emsdk_env="${EMSDK:-$HOME/emsdk}/emsdk_env.sh"
  if [[ -f "$emsdk_env" ]]; then
    # shellcheck disable=SC1090
    source "$emsdk_env"
  else
    echo "Emscripten not found. Install emsdk or set EMSDK." >&2
    exit 1
  fi
fi

emcmake cmake -S engine -B engine/build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build engine/build-wasm --target chess_engine_wasm

mkdir -p client/public/engine
cp engine/build-wasm/chess_engine.js engine/build-wasm/chess_engine.wasm client/public/engine/
echo "Wrote client/public/engine/chess_engine.{js,wasm}"
