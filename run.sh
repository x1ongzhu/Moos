#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
MOSS_ROM="${MOSS_ROM:-/Users/drew/workspace/palmos_roms/Palm-IIIx-3.1.rom}"
MOSS_PRC="${MOSS_PRC:-$ROOT/dist/Moss.prc}"
MOSS_TLS_PRC="${MOSS_TLS_PRC:-$ROOT/dist/MossTLS.prc}"
[[ -f "$MOSS_ROM" ]] || { echo 'ROM missing. Set MOSS_ROM.' >&2; exit 1; }
if [[ "${1:-}" == --no-install ]]; then
 exec '/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac' --rom "$MOSS_ROM"
fi
if [[ "${1:-}" == --provision ]]; then
 [[ -f "$ROOT/dist/MossTLSEntropy.pdb" ]] || { echo 'Entropy provision missing. Run ./build.sh.' >&2; exit 1; }
 SEED_INSTALL=(--install "$ROOT/dist/MossTLSEntropy.pdb")
elif [[ $# != 0 ]]; then
 echo 'Usage: ./run.sh [--no-install|--provision]' >&2; exit 2
else
 SEED_INSTALL=()
fi
[[ -f "$MOSS_TLS_PRC" ]] || { echo 'TLS library missing. Run ./build.sh first.' >&2; exit 1; }
[[ -f "$MOSS_PRC" ]] || { echo 'PRC missing. Run ./build.sh first.' >&2; exit 1; }
# Default to the local baseline Palm OS 3.1 ROM. Override MOSS_ROM for other versions.
if [[ ${#SEED_INSTALL[@]} -gt 0 ]]; then
 exec '/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac' --rom "$MOSS_ROM" --install "$MOSS_TLS_PRC" "${SEED_INSTALL[@]}" --install "$MOSS_PRC" --reset
fi
exec '/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac' --rom "$MOSS_ROM" --install "$MOSS_TLS_PRC" --install "$MOSS_PRC" --reset
