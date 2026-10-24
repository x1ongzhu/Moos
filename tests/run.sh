#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/moss-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
cc -std=c89 -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=address,undefined -g \
 "$ROOT/src/protocol.c" "$ROOT/tests/protocol_test.c" -o "$TEST_DIR/protocol_test"
"$TEST_DIR/protocol_test"
python3 "$ROOT/tests/rsa_public_test.py"
