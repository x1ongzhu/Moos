#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/moss-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
cc -std=c89 -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=address,undefined -g \
 "$ROOT/src/protocol.c" "$ROOT/tests/protocol_test.c" -o "$TEST_DIR/protocol_test"
"$TEST_DIR/protocol_test"
cc -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=address,undefined -g \
 "$ROOT/src/protocol.c" "$ROOT/tests/stream_test.c" -o "$TEST_DIR/stream_test"
"$TEST_DIR/stream_test"
cc -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-unused-parameter -Wno-multichar \
 -fsanitize=address,undefined -g -I"$ROOT/tests/palm_stub" \
 "$ROOT/src/protocol.c" "$ROOT/tests/network_test.c" -o "$TEST_DIR/network_test"
"$TEST_DIR/network_test"
python3 "$ROOT/tests/relay_test.py"
