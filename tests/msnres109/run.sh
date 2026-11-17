#!/bin/bash
# Гейт читателя списка ресурсов миссий: хост-сборка msnres.c читает файлы всех семи уровней набора DATA; красный контроль — испорченный байт заголовка и обрезанный файл дают отказ.
# tests/msnres109/run.sh DATA   (DATA — папка набора, уровни 2–7 в l<N>/)
set -u; cd "$(dirname "$0")/../.."; DATA="${1:?папка набора}"; T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
cc -O1 -Wall -Itests/msnres109/stub -o "$T/host" tests/msnres109/host.c viewer/msnres.c || exit 1
rc=0
for n in 1 2 3 4 5 6 7; do d="$DATA/"; [ $n = 1 ] && f="$DATA/msnres-l1.shr" || f="$DATA/l$n/msnres-l$n.shr"
  "$T/host" "$d" $n | tail -1 | grep -q "MSNRES ok level=$n" && echo "L$n зелёный" || { echo "L$n КРАСНЫЙ"; rc=1; }; done
# красный: порча магии и обрезка
mkdir -p "$T/bad"; cp "$DATA/msnres-l1.shr" "$T/bad/msnres-l1.shr"; printf 'X' | dd of="$T/bad/msnres-l1.shr" bs=1 seek=1 conv=notrunc 2>/dev/null
"$T/host" "$T/bad/" 1 >/dev/null && { echo "красный контроль (магия): НЕ пойман"; rc=1; } || echo "красный контроль (магия): пойман"
head -c 700 "$DATA/msnres-l1.shr" > "$T/bad/msnres-l1.shr"
"$T/host" "$T/bad/" 1 >/dev/null && { echo "красный контроль (обрезка): НЕ пойман"; rc=1; } || echo "красный контроль (обрезка): пойман"
exit $rc
