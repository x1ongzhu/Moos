#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
MOSS_ROM="${MOSS_ROM:-/Users/drew/workspace/palmos_roms/Palm-IIIx-3.1.rom}"
MOSS_PRC="${MOSS_PRC:-$ROOT/dist/Moss.prc}"
[[ -f "$MOSS_ROM" ]] || { echo 'ROM missing. Set MOSS_ROM.' >&2; exit 1; }
if [[ "${1:-}" == --no-install ]]; then
 exec '/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac' --rom "$MOSS_ROM"
fi
if [[ $# != 0 ]]; then
 echo 'Usage: ./run.sh [--no-install]' >&2; exit 2
fi
[[ -f "$MOSS_PRC" ]] || { echo 'PRC missing. Run ./build.sh first.' >&2; exit 1; }
exec '/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac' --rom "$MOSS_ROM" --install "$MOSS_PRC" --reset
