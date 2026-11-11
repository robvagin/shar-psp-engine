#!/bin/bash
# №bake-3А: испечь texbake.shr (таблица текстур: tstat + палитры после тона) тем же кодом движка, что считает на PSP (эмулятор Мака, без AI).
#   tools/texbake.sh SET OUT [УРОВНИ]    SET = папка с EBOOT.PBP и данными уровней (как для tools/smoke.sh); OUT/l<N>/texbake.shr; по умолчанию уровни «1 2 3 4 5 6 7»
# Движок в режиме texbakedump (файл-флаг) считает по-старому и пишет texbake_L<N>.out; smoke.sh копирует его в build/smoke/<время>/.
# Ключи входа (палитра, выборка точек, метки) и ключ тона внутри файла: поменялись данные или настройки по умолчанию — движок сам считает заново, перепечь нужна для скорости, не для верности.
# Класть OUT/l<N>/texbake.shr рядом с level.shr (для L1 — рядом с EBOOT). Сверка: tools/texbake_gate.sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; SET="$1"; OUT="$2"; shift 2; LV="${*:-1 2 3 4 5 6 7}"
mkdir -p "$OUT"; SCN="$(mktemp -d)"
for n in $LV; do
  printf 'flag level %s\nscene bake%s\nwait 14\nend\n' "$n" "$n" > "$SCN/l$n.txt"
  # 09.11 (Публичный-93): ключ записи несёт flatFromFile и метки notoon (tstat[4]); печь без flat912.bin/notoon.txt (smoke.sh берёт их не из SET) давала ключи, которые на PSP не совпали ни у одной текстуры L2-L7 (тур 100: used=0 miss=все)
  EX=""; for f in flat912.bin marks912.bin notoon.txt zones1007.bin zones1007p.bin; do if [ "$n" = 1 ]; then q="$f"; else q="l$n/$f"; fi; [ -f "$SET/$q" ] && EX="$EX $q"; done
  SMOKE_EXTRA="$EX" SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_FLAGS="texbakedump" SMOKE_SCN="$SCN/l$n.txt" SMOKE_MAXT="${SMOKE_MAXT:-600}" "$ROOT/tools/smoke.sh" "$SET" >"$SCN/run$n.log" 2>&1 || true
  d=$(sed -n 's/^отчёт: \(.*\)\/summary.txt.*/\1/p' "$SCN/run$n.log" | tail -1); case "$d" in /*) ;; *) d="$ROOT/$d" ;; esac   # папка своего прогона (параллельные прогоны)
  if [ -f "$d/texbake_L$n.out" ]; then mkdir -p "$OUT/l$n"; cp "$d/texbake_L$n.out" "$OUT/l$n/texbake.shr"; echo "L$n: $(wc -c < "$OUT/l$n/texbake.shr") байт; $(grep -a '^TEXBAKE' "$d/smoke.txt" | head -1)"; else echo "L$n: НЕТ texbake_L$n.out (см. $d)"; fi
done
