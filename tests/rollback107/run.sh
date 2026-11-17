#!/bin/bash
# Гейт 107б: rollback.txt рядом с EBOOT возвращает старый путь по имени без пересборки. tests/rollback107/run.sh DIR_С_EBOOT_И_ДАННЫМИ
# 1) без файла: ролик гэга в рамке (зелёный, tests/movietv1271) и карточка с картинкой (tests/briefpic1269);
# 2) файл «MOVIETV1271_OLD» + неизвестное имя: в журнале ROLLBACK … applied и unknown, ролик гэга без рамки;
# 3) файл «BRIEFPIC1269_OLD»: карточка без картинки (lost=1).
set -u; cd "$(dirname "$0")/../.."; SRC="$1"; BAD=0; T=$(mktemp -d /tmp/rb107.XXXXXX)
mk() { local D="$T/$1"; mkdir -p "$D"; for f in "$SRC"/*; do ln -s "$(readlink "$f" 2>/dev/null || echo "$f")" "$D/$(basename "$f")" 2>/dev/null; done; rm -f "$D/EBOOT.PBP"; cp "$SRC/EBOOT.PBP" "$D/EBOOT.PBP"; [ -n "${2:-}" ] && printf '%b' "$2" > "$D/rollback.txt"; echo "$D"; }
run() { SMOKE_EXTRA=rollback.txt SMOKE_MAXT=300 SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_SCN="$PWD/$2" tools/smoke.sh "$1" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p'; }
# сцены: ролик гэга (movietv1271) и карточка (briefpic1269)
D0=$(mk none); D1=$(mk movie '# проверка\nMOVIETV1271_OLD\nNOSUCHNAME_OLD\n'); D2=$(mk pic 'BRIEFPIC1269_OLD\n')
R=$(run "$D0" tests/movietv1271/scenario.txt); echo -n "без файла, гэг: "; .venv/bin/python tests/movietv1271/check.py "$R/mid.png" || BAD=1; grep -c "^ROLLBACK" "$R/smoke.txt" | sed 's/^/  строк ROLLBACK (ждём 0): /'; rm -rf "${R:?}"
R=$(run "$D1" tests/movietv1271/scenario.txt); echo -n "MOVIETV1271_OLD: "; if .venv/bin/python tests/movietv1271/check.py "$R/mid.png"; then echo "FAIL: рамка осталась"; BAD=1; else echo "OK: рамки нет, как в 106"; fi
grep -h "^ROLLBACK" "$R/smoke.txt"; grep -q "ROLLBACK MOVIETV1271_OLD applied" "$R/smoke.txt" || { echo "FAIL: нет строки applied"; BAD=1; }; grep -q "ROLLBACK NOSUCHNAME_OLD unknown" "$R/smoke.txt" || { echo "FAIL: нет строки unknown"; BAD=1; }; rm -rf "${R:?}"
R=$(run "$D2" tests/briefpic1269/scenario.txt); echo -n "BRIEFPIC1269_OLD: "; if .venv/bin/python tests/briefpic1269/check.py "$R"; then echo "FAIL: картинка осталась"; BAD=1; else echo "OK: карточка без картинки, как в 106"; fi; grep -h "^ROLLBACK" "$R/smoke.txt"; rm -rf "${R:?}"
# 4) файл со ВСЕМИ именами таблицы rollback.h и тремя особыми: каждое имя — «applied», неизвестных нет, итог applied=N (замечание Fable: проверять не два имени из сорока четырёх)
ALL=$(python3 - <<'PY'
import re
t=open('viewer/rollback.h',encoding='utf-8').read()
names=[]
for n in re.findall(r'\{"([A-Z0-9_]+_OLD)"',t):
    if n not in names: names.append(n)
for n in ('SBHEAL107B_OLD','MEWAKE107B_OLD','SHARME_STALE107B_OLD'):
    if n not in names: names.append(n)
print(' '.join(names))
PY
)
NN=$(echo $ALL | wc -w | tr -d ' '); D3=$(mk all "$(echo $ALL | tr ' ' '\n')")
SC=$(mktemp /tmp/rbs.XXXXXX); printf 'scene RB_ALL\nwait 4\nend\n' > "$SC"
R=$(SMOKE_EXTRA=rollback.txt SMOKE_MAXT=120 SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_SCN="$SC" tools/smoke.sh "$D3" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p'); rm -f "$SC"
APP=$(grep -c "^ROLLBACK .* applied" "$R/smoke.txt"); UNK=$(grep -c "^ROLLBACK .* unknown" "$R/smoke.txt"); MOD=$(grep -c "^ROLLBACK SHARME_STALE107B_OLD" "$R/smoke.txt"); FIL=$(grep -h "^ROLLBACK file applied=" "$R/smoke.txt" | sed 's/.*applied=//')
echo "все имена: в файле $NN, applied-строк $APP (+ модуль $MOD), unknown $UNK, итог file applied=$FIL"
{ [ "$UNK" = 0 ] && [ "$((APP+MOD))" = "$NN" ] && [ "${FIL:-0}" = "$NN" ]; } || { echo "FAIL: не все имена применены"; BAD=1; }; rm -rf "${R:?}"
rm -rf "${T:?}"; [ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
