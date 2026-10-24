#!/usr/bin/env bash
set -euo pipefail
TOOLS=/cygdrive/c/Handspring/prc-tools
export PATH="$TOOLS/bin:/cygdrive/c/cygnus/cygwin-b20/H-i586-cygwin32/bin:$PATH"
mkdir -p build/tls
CC="$TOOLS/bin/m68k-palmos-gcc.exe"
MOSS_TLS_INSECURE="${MOSS_TLS_INSECURE:-0}"
case "$MOSS_TLS_INSECURE" in 0|1) ;; *) exit 2 ;; esac
CFLAGS=(-specs=tls/core.specs -mnoshort -mno-pcrel -Wa,-S -fno-pic -O2 -fomit-frame-pointer -fno-builtin -DBR_SLOW_MUL15=1 -DBR_64=0 -DBR_INT128=0 -DBR_UMUL128=0 -DBR_USE_URANDOM=0 -DBR_USE_WIN32_RAND=0 -Itls/compat -Itls -Ivendor/bearssl-0.6/inc -Ivendor/bearssl-0.6/src)
FILES=(tls/core.c tls/rsa_verify.c tls/deferred_x509.c vendor/bearssl-0.6/src/codec/*.c vendor/bearssl-0.6/src/hash/sha*.c vendor/bearssl-0.6/src/hash/multihash.c vendor/bearssl-0.6/src/hash/dig_oid.c vendor/bearssl-0.6/src/mac/hmac.c vendor/bearssl-0.6/src/mac/hmac_ct.c vendor/bearssl-0.6/src/rand/hmac_drbg.c vendor/bearssl-0.6/src/int/i15*.c vendor/bearssl-0.6/src/int/i32_div32.c vendor/bearssl-0.6/src/rsa/rsa_i15_pub.c vendor/bearssl-0.6/src/rsa/rsa_i15_pkcs1_vrfy.c vendor/bearssl-0.6/src/rsa/rsa_pkcs1_sig_unpad.c vendor/bearssl-0.6/src/ec/ec_p256_m15.c vendor/bearssl-0.6/src/symcipher/aes_ct_enc.c vendor/bearssl-0.6/src/symcipher/aes_ct.c vendor/bearssl-0.6/src/symcipher/aes_ct_ctr.c vendor/bearssl-0.6/src/hash/ghash_ctmul32.c vendor/bearssl-0.6/src/ssl/ssl_client.c vendor/bearssl-0.6/src/ssl/ssl_engine.c vendor/bearssl-0.6/src/ssl/ssl_hs_client.c vendor/bearssl-0.6/src/ssl/ssl_rec_gcm.c vendor/bearssl-0.6/src/ssl/prf.c vendor/bearssl-0.6/src/ssl/prf_sha256.c vendor/bearssl-0.6/src/ssl/prf_sha384.c vendor/bearssl-0.6/src/x509/x509_minimal.c vendor/bearssl-0.6/src/x509/x509_decoder.c)
OBJS=()
CFLAGS+=(-DMOSS_TLS_INSECURE="$MOSS_TLS_INSECURE")
for f in "${FILES[@]}"; do
 name="$(basename "$f" .c)"; obj="build/tls/$name.o"; OBJS+=("$obj")
 if [[ "$f" == tls/core.c || ! -f "$obj" || "$f" -nt "$obj" || tls/core_abi.h -nt "$obj" || tls/preverified_deepseek.h -nt "$obj" || tls/deferred_x509.h -nt "$obj" || tls/core.specs -nt "$obj" || scripts/build-tls-core.sh -nt "$obj" || tls/moss_mul15.h -nt "$obj" || vendor/bearssl-0.6/src/inner.h -nt "$obj" || vendor/bearssl-0.6/src/int/i15_modpow2.c -nt "$obj" || vendor/bearssl-0.6/src/int/i15_montmul.c -nt "$obj" || vendor/bearssl-0.6/src/rsa/rsa_i15_pub.c -nt "$obj" || vendor/bearssl-0.6/src/rsa/rsa_i15_pkcs1_vrfy.c -nt "$obj" ]]; then
  printf 'TLS compile %s\n' "$name"
  EXTRA=()
  case "$f" in */i15_ninv15.c) EXTRA=(-DMOSS_TLS_MUL15_FALLBACK=1) ;; esac
  "$CC" "${CFLAGS[@]}" "${EXTRA[@]}" -c "$f" -o "$obj"
 fi
done
rm -f build/tls/bear.a
"$TOOLS/bin/m68k-palmos-ar.exe" rcs build/tls/bear.a "${OBJS[@]:1}"
"$TOOLS/bin/m68k-palmos-ld.exe" -r -T tls/core.ld build/tls/core.o --start-group build/tls/bear.a /prc-tools/lib/gcc-lib/m68k-palmos/2.95.2-kgpd/libgcc.a --end-group -o build/tls/core.rel
"$TOOLS/bin/m68k-palmos-objcopy.exe" -O binary build/tls/core.rel build/tls/core.bin
"$TOOLS/bin/m68k-palmos-objdump.exe" -r build/tls/core.rel > build/tls/core.relocs
"$TOOLS/bin/m68k-palmos-nm.exe" -n build/tls/core.rel > build/tls/core.symbols
wc -c build/tls/core.bin

SDK="C:/Handspring/prc-tools/PalmDev/sdk-3.5/include"
"$CC" -O1 -Wall -Werror -Wno-unknown-pragmas -I"$SDK" -I"$SDK/Core" -I"$SDK/Core/System" -I"$SDK/Core/UI" -I"$SDK/Libraries" -I"$SDK/Core/Hardware" -Itls -c tls/library.c -o build/tls/library.o
"$CC" -c tls/dispatch.S -o build/tls/dispatch.o
"$CC" -nostartfiles build/tls/dispatch.o build/tls/library.o -o build/tls/shim.sym
"$TOOLS/bin/m68k-palmos-objcopy.exe" -R .data -R .bss -R .reloc -O binary build/tls/shim.sym build/tls/shim.bin
