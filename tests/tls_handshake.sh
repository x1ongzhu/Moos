#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/moss-tls.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
HOST="${MOSS_TLS_HOST:-api.deepseek.com}"
PORT="${MOSS_TLS_PORT:-443}"
SOURCE="${MOSS_TLS_TEST_SOURCE:-$ROOT/tests/tls_handshake.c}"
EXTRA=()
if [[ "$SOURCE" == */tls_insecure_handshake.c ]]; then EXTRA+=(-DMOSS_TLS_INSECURE=1); fi
if [[ -n "${MOSS_EXPORT_CACHE:-}" || "$SOURCE" == */tls_cache_handshake.c ]]; then
 EXTRA=(-DMOSS_NO_PREVERIFIED=1)
else
 python3 "$ROOT/scripts/preverify_deepseek.py" --check
fi

cc -std=c99 -O2 -Wall -Wextra -Werror ${EXTRA[@]+"${EXTRA[@]}"} \
 -Wno-unused-function -Wno-unused-parameter \
 -I"$ROOT/tls" -I"$ROOT/vendor/bearssl-0.6/inc" -I"$ROOT/vendor/bearssl-0.6/src" \
 "$ROOT/tls/core.c" "$ROOT/tls/rsa_verify.c" "$ROOT/tls/deferred_x509.c" "$SOURCE" \
 "$ROOT"/vendor/bearssl-0.6/src/codec/*.c \
 "$ROOT"/vendor/bearssl-0.6/src/hash/sha*.c \
 "$ROOT/vendor/bearssl-0.6/src/hash/multihash.c" \
 "$ROOT/vendor/bearssl-0.6/src/hash/dig_oid.c" \
 "$ROOT/vendor/bearssl-0.6/src/hash/ghash_ctmul32.c" \
 "$ROOT/vendor/bearssl-0.6/src/mac/hmac.c" \
 "$ROOT/vendor/bearssl-0.6/src/mac/hmac_ct.c" \
 "$ROOT/vendor/bearssl-0.6/src/rand/hmac_drbg.c" \
 "$ROOT"/vendor/bearssl-0.6/src/int/i15*.c \
 "$ROOT/vendor/bearssl-0.6/src/int/i32_div32.c" \
 "$ROOT/vendor/bearssl-0.6/src/rsa/rsa_i15_pub.c" \
 "$ROOT/vendor/bearssl-0.6/src/rsa/rsa_i15_pkcs1_vrfy.c" \
 "$ROOT/vendor/bearssl-0.6/src/rsa/rsa_pkcs1_sig_unpad.c" \
 "$ROOT/vendor/bearssl-0.6/src/symcipher/aes_ct_enc.c" \
 "$ROOT/vendor/bearssl-0.6/src/symcipher/aes_ct.c" \
 "$ROOT/vendor/bearssl-0.6/src/symcipher/aes_ct_ctr.c" \
 "$ROOT/vendor/bearssl-0.6/src/ssl/ssl_client.c" \
 "$ROOT/vendor/bearssl-0.6/src/ssl/ssl_engine.c" \
 "$ROOT/vendor/bearssl-0.6/src/ssl/ssl_hs_client.c" \
 "$ROOT/vendor/bearssl-0.6/src/ssl/ssl_rec_gcm.c" \
 "$ROOT/vendor/bearssl-0.6/src/ssl/prf.c" \
 "$ROOT/vendor/bearssl-0.6/src/ssl/prf_sha256.c" \
 "$ROOT/vendor/bearssl-0.6/src/ssl/prf_sha384.c" \
 "$ROOT/vendor/bearssl-0.6/src/x509/x509_minimal.c" \
 "$ROOT/vendor/bearssl-0.6/src/x509/x509_decoder.c" \
 -o "$TEST_DIR/tls_handshake"

if [[ "$SOURCE" == */tls_cache_handshake.c || "$SOURCE" == */tls_preverified_handshake.c || "$SOURCE" == */tls_insecure_handshake.c ]]; then
 "$TEST_DIR/tls_handshake" "$HOST" "$PORT" >"$TEST_DIR/cache.log" 2>&1 || { cat "$TEST_DIR/cache.log" >&2; exit 1; }
 tail -1 "$TEST_DIR/cache.log"
 exit 0
fi

printf 'Verifying the real certificate chain for %s:%s\n' "$HOST" "$PORT"
if ! "$TEST_DIR/tls_handshake" "$HOST" "$PORT" >"$TEST_DIR/handshake.log" 2>&1; then
 cat "$TEST_DIR/handshake.log" >&2
 exit 1
fi
tail -1 "$TEST_DIR/handshake.log"

printf 'Verifying certificate-hostname rejection\n'
set +e
"$TEST_DIR/tls_handshake" "$HOST" "$PORT" invalid.invalid >"$TEST_DIR/hostname.log" 2>&1
status=$?
set -e
if [[ $status -ne 6 ]] || ! grep -q 'X509=56$' "$TEST_DIR/hostname.log"; then
 cat "$TEST_DIR/hostname.log" >&2
 echo 'Hostname mismatch was not rejected by certificate validation' >&2
 exit 1
fi

printf 'Verifying certificate-expiry rejection\n'
set +e
"$TEST_DIR/tls_handshake" "$HOST" "$PORT" "$HOST" 946684800 >"$TEST_DIR/expiry.log" 2>&1
status=$?
set -e
if [[ $status -ne 6 ]] || ! grep -q 'X509=54$' "$TEST_DIR/expiry.log"; then
 cat "$TEST_DIR/expiry.log" >&2
 echo 'Expired certificate was not rejected by certificate validation' >&2
 exit 1
fi

python3 "$ROOT/tests/tls_untrusted_test.py" "$TEST_DIR/tls_handshake"
echo 'TLS handshake and validation checks passed'
