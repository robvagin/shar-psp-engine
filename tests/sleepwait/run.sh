#!/bin/bash
# Сон PSP во время ожиданий (Fable 6д): служебный flag simsleepat 1 (ожидание персонажей комнаты) и 2 (загрузка L5 под газетой). Гейт: после пробуждения ожидание завершается (вход в комнату / уровень загружен), игра не виснет, сон был (PWR suspend, resume).
#   tests/sleepwait/run.sh EBOOT_ТУРА [НАБОР]      Красного контроля нет: проверка «не висит» (без сна те же строки есть, зелёный на старом коде тоже).
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; cd $W; E=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); O=${OUT:-$W/build/sleepwait}; mkdir -p $O/ovl_empty
export SET=${2:-$W/build/test108tour/PSP/GAME/SHARTEST15} OUT=$O OVL=$O/ovl_empty SMOKE_HEADLESS=1 SHIP_LEVELS="2 3 4 5 6 7" EMU_OWNER=Sleepwait EMU_MINE=1
python3 - <<'PY' > $O/s.txt
t=open('tests/smoke_scenes/test108_game.txt',encoding='utf-8').read()
a=t.index('scene F_ZT_SLEEP_ROOM'); b=t.index('### TEST69: все 16 роликов')
print("\n".join(["scene C_BOOT","flag cfgdefaults 1","wait 6","scene L4_load","flag golevel 4","wait 45",""])+t[a:b]+"\nend")
PY
bash tools/emu_run.sh $O/s.txt s $E 600 > /dev/null 2>&1; f=$O/out_s/smoke.txt; bad=0
sl=$(grep -ac '^PWR sim sleep\|^PWR suspend' $f); en=$(grep -ac '^ROOM ev=enter room=KwikEMart' $f); l5=$(grep -ac '^LEVEL ev=load n=5 ' $f); ex=$(grep -ac '^END\|^EXIT' $f)
echo "сон: $sl строк PWR; вход в комнату: $en; загрузка L5: $l5; конец сценария: $ex"
[ "$sl" -ge 2 ] || { echo "FAIL: сна не было"; bad=1; }; [ "$en" -ge 1 ] || { echo "FAIL: после сна в ожидании комнаты вход не состоялся"; bad=1; }; [ "$l5" -ge 1 ] || { echo "FAIL: после сна L5 не загрузился"; bad=1; }
[ $bad = 0 ] && echo "SLEEPWAIT PASS" || echo "SLEEPWAIT FAIL"; exit $bad
