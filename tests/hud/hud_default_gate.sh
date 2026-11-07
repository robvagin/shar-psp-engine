#!/bin/bash
# Гейт №1165 «статистика сверху скрыта по умолчанию»: безголовый релизный тур-EBOOT (KEEP_SMOKE, KEEP_HUD), набор после tools/hud_default.sh.
#   tests/hud/hud_default_gate.sh EBOOT_tour.PBP DATA_DIR      (DATA_DIR может нести hudon: гейт сам применяет hud_default.sh к своей копии)
#   1) старт: верх кадра пустой (кадр = кадр без счётчика); 2) R+SELECT: счётчик виден; 3) ещё раз R+SELECT: верх снова как на старте
#   4) журнал тура: те же виды строк, что при счётчике с первого кадра (счётчик журнал не меняет)
# HUDON_OLD=1 — прежнее поведение наборов (hudon остаётся): гейт обязан упасть на пункте 1
set -u; cd "$(dirname "$0")/../.."
EB=${1:?EBOOT}; SRC=${2:?DATA}; SCN=$PWD/tests/hud/hud_default_scenario.txt; T=$PWD/build/hudgate1165_$$; trap 'rm -rf "$T"' EXIT
mkdir -p "$T"; cp -cR "$SRC" "$T/pkg" 2>/dev/null || cp -R "$SRC" "$T/pkg"; cp "$EB" "$T/pkg/EBOOT.PBP"
tools/hud_default.sh "$T/pkg" >/dev/null || exit 1
run() {  # имя  SMOKE_FILEDATA
  while [ "$(pgrep -x PPSSPPHeadless | wc -l)" -ge 5 ]; do sleep 10; done
  FL=""; [ -e "$T/pkg/hudon" ] && FL=hudon   # smoke.sh берёт из набора только файлы списка поставки: hudon набора передаём флагом, как лежал бы рядом с EBOOT
  SMOKE_FLAGS="$FL" SMOKE_HEADLESS=1 SMOKE_SCN=$SCN SMOKE_FILEDATA="$2" SMOKE_MAXT=120 tools/smoke.sh "$T/pkg" > "$T/$1.out" 2>&1
  R=$(sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p' "$T/$1.out"); [ -n "$R" ] || { echo "FAIL: $1: нет отчёта: $(head -1 "$T/$1.out")"; exit 1; }
  mkdir -p "$T/$1"; cp "$R"/HUD_start.png "$R"/HUD_on.png "$R"/HUD_off.png "$T/$1/" 2>/dev/null; cp "$R/smoke.txt" "$T/$1/smoke.txt" 2>/dev/null
}
run def ""; run withfile "hudon=1"
python3 -I - "$T" <<'PY'
import sys, re
from PIL import Image
import numpy as np
T=sys.argv[1]
def top(n,f): return np.asarray(Image.open(f'{T}/{n}/{f}.png').convert('RGB'),int)[:70]
def diff(a,b): return int((np.abs(a-b).sum(2)>30).sum())
def white(a): return int(((a>235).all(2)).sum())   # текст счётчика белый (0xffffffff): на старте его не должно быть больше, чем после R+SELECT
s,o,f=top('def','HUD_start'),top('def','HUD_on'),top('def','HUD_off')
d_on,d_off=diff(s,o),diff(s,f)
def kinds(n):
    t=open(f'{T}/{n}/smoke.txt','rb').read().decode('utf-8','replace')
    return {m.group(1) for m in re.finditer(r'^([A-Z][A-Z0-9_]{2,})\b',t,re.M)} - {'SVC'}
ka,kb=kinds('def'),kinds('withfile'); only=sorted(ka^kb)
print(f'верх кадра (70 строк): старт vs R+SELECT {d_on} px; старт vs повтор {d_off} px; белых {white(s)}/{white(o)}/{white(f)}; виды строк журнала: {len(ka)} / {len(kb)} (расхождения: {only[:8]})')
fails=[]
if diff(s,o)<150 or white(o)-white(s)<100: fails.append(f'счётчик не появился или на старте он уже виден (старт vs R+SELECT {d_on} px, белых px старт {white(s)} / R+SELECT {white(o)})')
if abs(white(f)-white(s))>30: fails.append(f'после второго R+SELECT белый текст сверху не исчез (белых px {white(s)} -> {white(f)})')   # кадр сцены за 4 с чуть меняется, поэтому по белому тексту, не по полному различию
if only: fails.append('журнал тура отличается набором строк')
print('HUD DEFAULT GATE:', 'FAIL ' + '; '.join(fails) if fails else 'PASS'); sys.exit(1 if fails else 0)
PY
