#!/bin/bash
# Гейт №1270: номер сборки на загрузочном экране = viewer/BUILD_NUMBER, а не число, вписанное руками.
#   tests/bootbuild1270/check.sh [EXTRA_CPP_FLAGS]   зелёный: BOOT_W_BUILDS из -DSHAR_BUILD_NUM, взятого CMake из viewer/BUILD_NUMBER (viewer/build/CMakeFiles/sharview.dir/flags.make) == BUILD_NUMBER
#   tests/bootbuild1270/check.sh -DBOOT_BUILDNUM1270_OLD=1   красный: число 102 руками
set -u; cd "$(dirname "$0")/../.."
FM=viewer/build/CMakeFiles/sharview.dir/flags.make
want=$(tr -d ' \n' < viewer/BUILD_NUMBER)
dnum=$(grep -o 'SHAR_BUILD_NUM=[0-9]*' "$FM" | head -1)
[ -n "$dnum" ] || { echo "FAIL: CMake не передал SHAR_BUILD_NUM ($FM)"; exit 1; }
got=$(printf '#include "viewer/buildnum.h"\nBOOT_W_BUILDS\n' | cc -E -P -D$dnum "$@" -x c - | tail -1 | tr -d ' ')
got=$(echo "$got" | sed 's/(.*?102:\(.*\))/\1/')   # на случай нераскрытого тернарника
v=$(printf '#include "viewer/buildnum.h"\nint v = BOOT_W_BUILDS;\n' | cc -E -P -D$dnum "$@" -x c - | tail -1)
case "$v" in *"1 ? 102"*|*"(1 ?"*) got=102;; esac
case "$v" in *"(0 ? 102 :"*) got=$(echo "$v" | sed 's/.*: *\([0-9]*\) *).*/\1/');; esac
[ "$got" = "$want" ] && echo "OK: на загрузочном экране build $got = viewer/BUILD_NUMBER ($dnum)" || { echo "FAIL: на загрузочном экране build $got, а в viewer/BUILD_NUMBER $want"; exit 1; }
