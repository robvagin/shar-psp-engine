#!/bin/bash
# 108 правка Роба 1294: «вступление уровня» (level_intro, гитара) на старте L2 пешком и в машине. tests/aud1294/run.sh EBOOT [SET] -> PASS/FAIL
# Сцены: L1 → golevel 2 (путь смены уровня), игрок пешком / в машине (flag audforcecar 1), красный — прежний путь (flag levelintrocar 0). Критерий: после LEVELAUDIO стингер звучит ≥ 4 с (AUDV cur=level_intro ≥ 4 отсчётов), в машине следом музыка езды (cur=sunday_drive*).
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 SHIP_LEVELS="1 2 3 4 5 6 7" OUT=${OUT:-$PWD/build/aud1294}; mkdir -p "$OUT"; rm -rf "$OUT"/out_*
for n in car car_old foot; do f=tests/aud1294/scene_up_$n.txt; [ $n = foot ] && f=tests/aud1294/scene_up.txt; FLAGS="" nice -n 20 tools/emu_run.sh $f $n "$EB" 300 > "$OUT/$n.log" 2>&1; done
OUT="$OUT" python3 -I - <<'PY'
import os,re,sys
def run(n):
    c=0; st=0; drive=0
    for ln in open(os.environ['OUT']+'/out_%s/smoke.txt'%n,errors='replace'):
        if ln.startswith('LEVELAUDIO'): c+=1
        if c>=2 and ln.startswith('AUDV'):
            m=re.search(r'cur=(\S+)',ln); cur=m.group(1) if m else '-'
            if cur=='level_intro': st+=1
            if cur.startswith('sunday_drive'): drive+=1
    return st,drive
f=run('foot'); c=run('car'); o=run('car_old'); ok=True
print('пешком: стингер %d отсчётов; в машине (новое): стингер %d, езда %d; в машине (прежнее): стингер %d, езда %d'%(f[0],c[0],c[1],o[0],o[1]))
if f[0]<4: print('FAIL пешком: стингер короче 4 с'); ok=False
if c[0]<4 or c[1]<3: print('FAIL в машине: нет стингера ≥ 4 с и езды следом'); ok=False
if o[0]>0: print('КРАСНЫЙ НЕ ПОЙМАН: прежний путь тоже играет стингер в машине'); ok=False
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
PY
