#!/usr/bin/env bash
# Runs on Windows 7's modern Cygwin shell; compiler uses its original b20 mounts.
set -euo pipefail
TOOLS_WIN="${MOSS_TOOLS_WIN:-C:/Handspring/prc-tools}"
TOOLS_POSIX="$(cygpath -u "$TOOLS_WIN")"
export PATH="$TOOLS_POSIX/bin:/cygdrive/c/cygnus/cygwin-b20/H-i586-cygwin32/bin:$PATH"
mkdir -p build
rm -f build/Moss.prc
CC="$TOOLS_POSIX/bin/m68k-palmos-gcc.exe"
RC="$TOOLS_POSIX/bin/Palm-RC.exe"
SDK="$TOOLS_WIN/PalmDev/sdk-3.5/include"
"$CC" -v
for source in main store network protocol; do
 printf 'Compiling %s.c\n' "$source"
 "$CC" -O1 -Wall -Werror -Wno-unknown-pragmas -Wno-multichar -I"$SDK" -I"$SDK/Core" -I"$SDK/Core/System" -I"$SDK/Core/UI" -I"$SDK/Libraries" -I"$SDK/Core/Hardware" -Isrc -c "src/$source.c" -o "build/$source.o"
done
"$CC" build/main.o build/store.o build/network.o build/protocol.o -o build/Moss.code.1.sym
"$RC" -rcp resources/Moss.rcp -gccApp build/Moss.code.1.sym \
 -I resources -o build/Moss.prc -name Moss -cr Moss -version 1
test -s build/Moss.prc
wc -c build/Moss.prc
