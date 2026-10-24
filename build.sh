#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
MOSS_BUILD_HOST="${MOSS_BUILD_HOST:-drew@192.168.50.167}"
MOSS_REMOTE_DIR="${MOSS_REMOTE_DIR:-/home/drew/workspace/Moss}"
SSH_OPTIONS=(-o BatchMode=yes -o ConnectTimeout=10)
MOSS_TLS_INSECURE="${MOSS_TLS_INSECURE:-0}"
case "$MOSS_TLS_INSECURE" in 0|1) ;; *) echo 'MOSS_TLS_INSECURE must be 0 or 1' >&2; exit 2 ;; esac
export MOSS_TLS_INSECURE
TLS_FILE=MossTLS.prc
if [[ "$MOSS_TLS_INSECURE" == 1 ]]; then TLS_FILE=MossTLS-insecure.prc; fi
python3 "$ROOT/scripts/preverify_deepseek.py" --check
# Remote paths are placed in shell single quotes. Reject unsafe overrides.
[[ "$MOSS_REMOTE_DIR" =~ ^/[a-zA-Z0-9_./-]+$ && "$MOSS_REMOTE_DIR" != / ]] || { echo 'Invalid MOSS_REMOTE_DIR' >&2; exit 2; }
mkdir -p "$ROOT/dist"
RUN_DIR="$(mktemp -d "$ROOT/dist/.build.XXXXXX")"
trap 'rm -rf "$RUN_DIR"' EXIT
printf 'Uploading source to %s\n' "$MOSS_BUILD_HOST"
# Copy only the project build inputs. Never delete remote files or touch other projects.
COPYFILE_DISABLE=1 tar --no-xattrs -C "$ROOT" -cf - src resources tls vendor scripts/remote-build.sh scripts/build-tls-core.sh |
 ssh "${SSH_OPTIONS[@]}" "$MOSS_BUILD_HOST" "mkdir -p '$MOSS_REMOTE_DIR' && cd '$MOSS_REMOTE_DIR' && tar -xf -"
printf 'Building on Windows 7\n'
ssh "${SSH_OPTIONS[@]}" "$MOSS_BUILD_HOST" "cd '$MOSS_REMOTE_DIR' && /bin/bash scripts/remote-build.sh" 2>&1 | tee "$ROOT/dist/build.log"
# SSH binary output avoids SCP/SFTP version differences on Windows.
ssh "${SSH_OPTIONS[@]}" "$MOSS_BUILD_HOST" "cat '$MOSS_REMOTE_DIR/build/Moss.prc'" > "$RUN_DIR/Moss.prc"
test -s "$RUN_DIR/Moss.prc"
ssh "${SSH_OPTIONS[@]}" "$MOSS_BUILD_HOST" "cd '$MOSS_REMOTE_DIR' && MOSS_TLS_INSECURE=$MOSS_TLS_INSECURE /bin/bash scripts/build-tls-core.sh" 2>&1 | tee "$ROOT/dist/tls-build.log"
ssh "${SSH_OPTIONS[@]}" "$MOSS_BUILD_HOST" "cd '$MOSS_REMOTE_DIR' && tar -cf - build/tls/core.bin build/tls/core.relocs build/tls/core.symbols build/tls/shim.bin" | tar -C "$ROOT" -xf -
python3 "$ROOT/scripts/package_tls.py"
mv "$RUN_DIR/Moss.prc" "$ROOT/dist/Moss.prc"
printf 'PRCs: %s / %s\n' "$ROOT/dist/Moss.prc" "$ROOT/dist/$TLS_FILE"
