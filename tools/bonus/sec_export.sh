#!/bin/bash
# №1185: линия секторов трасс бонус-игры — bonus/bNN.sec (след круга лидера гонки полигона: настоящие модули движка, дороги трассы, водитель соперников).
#   tools/bonus/sec_export.sh [ПАПКА_С_b0N.shr/.bns/msncars-b.shr] [ВЫХОД]      по умолчанию набор integ test87tour -> build/secdata
# Кладётся рядом с b0N.shr в папку bonus/ набора. Детерминированно (полигон на шаге 1/60 с). Нет файла в наборе — движок строит линию по плану маршрута (запасной путь, хуже совпадает с дорогой).
cd "$(dirname "$0")/../.."; export DEVELOPER_DIR="${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}"
S="${1:-$HOME/Downloads/shar-psp-integ/build/test87tour/PSP/GAME/SHARTEST15/bonus}"; O="${2:-build/secdata}"; mkdir -p "$O" build/bonus/bin
bash tools/bonus/build_host.sh build/bonus/bin/race_host >/dev/null 2>&1 || { echo "FAIL: сборка полигона"; exit 2; }
for t in 1 2 3 4 5 6 7; do build/bonus/bin/race_host "$S/b0$t.shr" "$S/b0$t.bns" "$S/msncars-b.shr" --laps 1 --secs 120 --sec-out "$O/b0$t.sec" | grep "SEC-OUT" || { echo "FAIL: b0$t"; exit 1; }; done
