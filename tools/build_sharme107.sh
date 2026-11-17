#!/bin/bash
# 107б P0: модуль ядра sharme.prx с поправкой «задание в ME дольше 500 мс = ME свободен, слово состояния сброшено» (сон оставлял «занято» навсегда). Только эта поправка, ничего другого.
# Основа: исходники коммита 2f8fbc02, из которых собран модуль, что лежит в наборах 106/107 (SHA-256 c53224b0…, 61294 Б); сначала собирается он сам и сверяется, потом ставится viewer/me/sharme_stale107.patch.
# Выход: build/sharme_107.prx (новый, в набор как sharme.prx) и build/sharme_old.prx (прежний, в набор как sharme_old.prx: игра берёт его при файле sharmeold или SHARME_STALE107B_OLD в rollback.txt).
set -eu; cd "$(dirname "$0")/.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools; export PSPDEV="$PWD/pspdev" PATH="$PWD/pspdev/bin:$PATH"
C=2f8fbc02; BASE=c53224b04453e1a0; W=build/sharme107_src; rm -rf "$W"; mkdir -p "$W" build
git archive "$C" viewer | tar -x -C "$W"
( cd "$W/viewer/me" && make >/dev/null 2>&1 )
G=$(shasum -a 256 "$W/viewer/me/sharme.prx" | cut -c1-16)
[ "$G" = "$BASE" ] || { echo "FAIL: базовая сборка $G не совпала с модулем наборов $BASE (тулчейн или исходники другие)"; exit 1; }
cp "$W/viewer/me/sharme.prx" build/sharme_old.prx
( cd "$W" && patch -p1 < "$OLDPWD/viewer/me/sharme_stale107.patch" >/dev/null && cd viewer/me && make clean >/dev/null 2>&1; make >/dev/null 2>&1 )
cp "$W/viewer/me/sharme.prx" build/sharme_107.prx
echo "OK: базовая сборка воспроизводит набор ($G); build/sharme_old.prx $(stat -f %z build/sharme_old.prx) Б, build/sharme_107.prx $(stat -f %z build/sharme_107.prx) Б sha $(shasum -a 256 build/sharme_107.prx | cut -c1-12)"
