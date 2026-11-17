#!/bin/bash
# ev.sh ПАПКА — сводка событий миссии и затемнения (без шума)
awk '/^SCENE /||/^MSN (stage|pass|retry|restart|mission|briefing)/||/^MSNEND/||/restart-force/||/WATCHDOG fade/{print} /^IRIS/{ph=$3; if(ph!=last){print; last=ph}}' "$1/smoke.txt" | grep -v "^MSN plan\|WATCHDOG spawn" | cut -c1-150
