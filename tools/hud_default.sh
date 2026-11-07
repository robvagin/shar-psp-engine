#!/bin/bash
# №1165: счётчик FPS/треугольников/RAM сверху по умолчанию скрыт (R+SELECT показывает и прячет). Движок показывает его с первого кадра,
# только если рядом с EBOOT лежит файл hudon; наборы TEST тянули его цепочкой от TEST27. Генератор набора вызывает эту команду на папке
# с EBOOT (игровая и тур-сборки): файл убирается. HUDON_OLD=1 — прежнее (файл остаётся; нужен, если Робу надо счётчик с первого кадра).
#   tools/hud_default.sh build/test87/PSP/GAME/SHARTEST15
set -e
D=${1:?папка набора (рядом с EBOOT.PBP)}
[ -f "$D/EBOOT.PBP" ] || { echo "нет $D/EBOOT.PBP"; exit 1; }
if [ "${HUDON_OLD:-0}" = 1 ]; then echo "hud_default: HUDON_OLD=1, hudon не трогаю ($( [ -e "$D/hudon" ] && echo есть || echo нет ))"; exit 0; fi
if [ -e "$D/hudon" ]; then rm -f "$D/hudon"; echo "hud_default: hudon убран ($D)"; else echo "hud_default: hudon уже нет ($D)"; fi
