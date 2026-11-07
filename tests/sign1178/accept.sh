#!/bin/bash
# №1178 гейт: вывески L5 (tools/far1177/signs.py, viewer/sign1178.h).
#   tests/sign1178/accept.sh LEVEL5.shr            — данные: таблица собирается, 69 записей, ntex совпал; красный контроль: испорченный список -> отказ
#   EMU=1 NEW=EBOOT_с_таблицей OLD=EBOOT_SIGN1178_OLD SET=папка_набора tests/sign1178/accept.sh LEVEL5.shr
#                                                  — + эмулятор: журнал SIGN1178 ok=1 n=69, у OLD строки нет, снимки OLD и NEW отличаются только внутри щитов
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; LV="${1:?level.shr уровня 5}"; T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; fail=0
ok() { echo "  ok: $1"; }; bad() { echo "  FAIL: $1"; fail=1; }
python3 -I "$ROOT/tools/far1177/signs.py" "$LV" "$T/sign1178.bin" --list "$ROOT/tools/far1177/signs_L5.json" >/dev/null && ok "таблица собрана" || bad "таблица не собралась"
n=$(python3 -I -c "import struct,sys;b=open('$T/sign1178.bin','rb').read();print(b[:4].decode(),struct.unpack_from('<III',b,4),len(b))")
echo "  $n"; case "$n" in "SG78 (1, 478, 69) 292") ok "заголовок: версия 1, ntex 478, записей 69, 292 Б";; *) bad "заголовок";; esac
# красный контроль: список с чужой подписью не должен молча собраться
python3 -I - "$ROOT/tools/far1177/signs_L5.json" "$T/bad.json" <<'P'
import json,sys; d=json.load(open(sys.argv[1])); d['signs'][3]['sig']='0'*16; json.dump(d,open(sys.argv[2],'w'))
P
if python3 -I "$ROOT/tools/far1177/signs.py" "$LV" "$T/bad.bin" --list "$T/bad.json" >/dev/null 2>&1; then bad "красный контроль: испорченный список собрался"; else ok "красный контроль: испорченный список отвергнут"; fi
if [ "${EMU:-0}" = 1 ]; then
  : "${NEW:?}" "${OLD:?}" "${SET:?}"; cp "$T/sign1178.bin" "$T/sign1178.bin.keep"
  mkdir -p "$SET/l5"; cp "$T/sign1178.bin" "$SET/l5/sign1178.bin"
  for v in old new; do
    if [ $v = old ]; then cp "$OLD" "$SET/EBOOT.PBP"; else cp "$NEW" "$SET/EBOOT.PBP"; fi
    ( cd "$ROOT" && SMOKE_EXTRA="l5/sign1178.bin" SHIP_LEVELS="5" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN="$ROOT/tests/sign1178/l5_ab.txt" SMOKE_MAXT=260 tools/smoke.sh "$SET" >/dev/null 2>&1 )
    ln -sfn "$ROOT/$(cd "$ROOT" && ls -dt build/smoke/* | head -1)" "$T/run_$v"
  done
  grep -aq "SIGN1178 level=5 ok=1 n=69" "$T/run_new/smoke.txt" && ok "NEW: SIGN1178 ok=1 n=69" || bad "NEW: нет строки SIGN1178"
  grep -aq "SIGN1178" "$T/run_old/smoke.txt" && bad "OLD: строка есть (выключатель не работает)" || ok "OLD: строки нет (выключатель работает)"
  python3 -I - "$T/run_old" "$T/run_new" <<'P' || fail=1
import sys; from PIL import Image, ImageChops
a_, b_ = sys.argv[1:3]; diff = 0; same = 0; wide = 0
for n in ('ab_153_pz100','ab_153_mz100','ab_191_b','ab_153_pz180','ab_191_a','ab_119_a'):
    a = Image.open(f'{a_}/{n}.png').convert('RGB'); b = Image.open(f'{b_}/{n}.png').convert('RGB'); bb = ImageChops.difference(a, b).getbbox()
    if bb: diff += 1; wide += (bb[2] - bb[0]) > 120 or (bb[3] - bb[1]) > 120   # отличие только на щите: ≤ 120 px по любой стороне при камерах ≥100 м
    else: same += 1
print(f'  снимков с отличием {diff}, без {same}, с «широким» отличием {wide}')
sys.exit(0 if diff >= 2 and wide == 0 else 1)
P
  [ $? -eq 0 ] && ok "снимки: отличие есть и только на щитах" || bad "снимки: нет отличия или оно по всему кадру"
fi
[ $fail -eq 0 ] && echo "sign1178: PASS" || { echo "sign1178: FAIL"; exit 1; }
