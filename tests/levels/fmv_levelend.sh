#!/bin/bash
# Гейт «конец уровня как в оригинале» в эмуляторе: уровень 2 -> m8 (последняя сюжетная: ролик SetFMVInfo("fmv3.rmv") + таймер и
# SetLevelOver) -> ролик movies/fmv3.smv сыгран (viewer/main.c msnMovie) -> LEVEL COMPLETE -> уровень 3 (газета, старт из данных).
# Сначала КРАСНЫЙ: копия набора с обрезанным movies/fmv3.smv (первые 64 КБ: ролик битый) — сверка «ролик сыгран» обязана упасть
# (движок не падает, цель засчитывается); потом ЗЕЛЁНЫЙ. Сценарий — tests/levels/fmv_levelend.txt.
#   tests/levels/fmv_levelend.sh [DIR]   DIR — EBOOT + данные (по умолчанию build/ship_stage); первая строка итог, код 0 = OK
# 08.11: набор из символьных ссылок (movies/ целой папкой) превращает «обрезать копию» в порчу оригинала; копия получает свою папку movies и свой файл fmv3.smv, перед обрезкой ссылка снимается.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
SRC="${1:-$ROOT/build/ship_stage}"; case "$SRC" in /*) ;; *) SRC="$PWD/$SRC" ;; esac
[ -f "$SRC/EBOOT.PBP" ] && [ -f "$SRC/l2/level.shr" ] && [ -f "$SRC/l3/level.shr" ] && [ -f "$SRC/movies/fmv3.smv" ] || {
  echo "FAIL: в $SRC нет EBOOT.PBP / l2, l3 / movies/fmv3.smv (tools/ship.sh --no-install)"; exit 1; }
T="$ROOT/build/gates/fmv_levelend/$(date +%Y%m%d-%H%M%S)"; mkdir -p "$T"
run() {
  local log; log=$(SMOKE_SCN="$1" "$ROOT/tools/smoke.sh" "$2" 2>&1)
  echo "$ROOT/$(echo "$log" | sed -n 's/^отчёт: \(.*\)\/summary.txt.*/\1/p' | tail -1)"
}
check() {   # папка прогона -> 0, если ролик сыгран и уровень 3 начался после конца уровня 2
  "$ROOT/.venv/bin/python" - "$1/smoke.txt" <<'PY'
import sys, re
log = open(sys.argv[1], errors='replace').read().splitlines()
kv = lambda l: dict(p.split('=', 1) for p in l.split()[1:] if '=' in p)
bad = []
fmv = [kv(l) for l in log if l.startswith('MSNFMV ')]
if not fmv or fmv[0].get('name') != 'fmv3' or fmv[0].get('played') != '1' or int(fmv[0].get('shown', 0)) < 25:
    bad.append('ролик fmv3 не сыгран: %s' % (fmv[0] if fmv else 'нет MSNFMV'))
if not any(l.startswith('MSN level-complete level=2') for l in log): bad.append('нет конца уровня 2 (SetLevelOver)')
if not any(l.startswith('LEVELUP from=2 to=3') for l in log): bad.append('нет перехода 2 -> 3')
ld = [kv(l) for l in log if l.startswith('LEVEL ') and kv(l).get('ev') == 'load']
if not ld or ld[-1].get('n') != '3' or ld[-1].get('msn') != '3': bad.append('уровень 3 не прочитан (%s)' % (ld[-1] if ld else '-'))
if not any(l.startswith('END') for l in log): bad.append('нет END')
for b in bad: print('  ПРОВАЛ: ' + b)
if not bad: print('  ролик %s: кадров показано %s (START — пропуск), конец уровня 2 -> уровень 3' % (fmv[0]['name'], fmv[0]['shown']))
print('PASS' if not bad else 'FAIL (%d)' % len(bad)); sys.exit(0 if not bad else 1)
PY
}
res=""; ok=1
P="$T/badmovie"; mkdir -p "$P"; cp -c -R "$SRC/." "$P/" && { [ ! -L "$P/movies" ] || { t=$(readlink "$P/movies"); rm "$P/movies"; mkdir "$P/movies"; for f in "$t"/*; do ln -s "$f" "$P/movies/$(basename "$f")"; done; }; } && rm -f "$P/movies/fmv3.smv" 2>/dev/null; [ -f "$P/movies/fmv3.smv" ] || cp "$SRC/movies/fmv3.smv" "$P/movies/fmv3.smv" && head -c 65536 "$SRC/movies/fmv3.smv" > "$P/movies/fmv3.smv.cut" && mv "$P/movies/fmv3.smv.cut" "$P/movies/fmv3.smv"
D=$(run tests/levels/fmv_levelend.txt "$P"); check "$D" > "$T/red.txt" 2>&1
if [ $? = 0 ]; then ok=0; res="$res; КРАСНЫЙ НЕ ПОЙМАН: битый ролик"; else res="$res; красный (битый ролик): $(grep -m1 'ПРОВАЛ' "$T/red.txt" | sed 's/^ *ПРОВАЛ: //' | cut -c1-110)"; fi
G=$(run tests/levels/fmv_levelend.txt "$SRC"); check "$G" > "$T/green.txt" 2>&1
if [ $? = 0 ]; then res="$res; зелёный: PASS"; else ok=0; res="$res; НЕ ЗЕЛЁНЫЙ: $(grep -m1 'ПРОВАЛ' "$T/green.txt" | sed 's/^ *ПРОВАЛ: //' | cut -c1-140)"; fi
echo "$([ $ok = 1 ] && echo PASS || echo FAIL)${res}"
echo "прогоны: красный ${D#$ROOT/}, зелёный ${G#$ROOT/} (снимки fmv3, loading, l3_after_fmv); журнал ${T#$ROOT/}"
cat "$T/green.txt"
[ $ok = 1 ]
