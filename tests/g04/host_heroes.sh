#!/bin/bash
# Круг 6, g04-homer: те же проверки для героя каждого уровня (L1 Гомер, L2 Барт, L3 Лиза, L4 Мардж, L5 Апу, L6 Барт, L7 Гомер).
#   tests/g04/host_heroes.sh ПАПКА [level.shr]   — в ПАПКЕ лежат l1.shr..l7.shr (tools/homer_export.py --level N --out ПАПКА/lN.shr)
cd "$(dirname "$0")/../.."
D=${1:?папка с l1.shr..l7.shr}; L=${2:-$HOME/Downloads/shar-psp/build/level.shr}; r=0
HN=(x homer bart lisa marge apu bart homer)
for n in 1 2 3 4 5 6 7; do echo "=== уровень $n (${HN[$n]})"; G04_HERO=${HN[$n]} tests/g04/host.sh "$L" "$D/l$n.shr" | grep -E "FAIL|провалов" || r=1; done
exit $r
