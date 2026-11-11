#!/bin/bash
# Гейт «таран тонкой стенки под углом» (сверка PS2, плита b47 L4, Публичный-93): после удара о стену под 15–35° к нормали на ~72 км/ч оригинал стоит на четырёх колёсах (крен ≤ 10°),
# порт клал машину на два колеса на секунду. Красный: -DCAR_CRASH_ANG_OLD=1. Нужны каталог данных уровня 4 (level.shr, player.shr). tests/cars/wallram_gate.sh LEVEL_DIR
set -u; ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; D="${1:?каталог с level.shr и player.shr}"; OUT="${OUT_DIR:-build/pk}"; mkdir -p "$OUT"
run() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $1 -I viewer -I tools/bots tools/bots/wallram.c \$BOTS_LIB -lm -o $OUT/wallram_gate" || exit 2
  bad=0; for hd in 108 115 125 135; do r=$(cd "$D" && "$ROOT/$OUT/wallram_gate" level.shr player.shr 135.0 608.9 $hd 85 2); echo "  $r" | cut -c1-140
    tw=$(echo "$r" | sed -n 's/.*подряд \([0-9]*\) кадров.*/\1/p'); [ "${tw:-1}" -gt 0 ] && bad=1; done; return $bad; }
echo "новое:"; run "" && echo "PASS" || { echo "FAIL"; exit 1; }
echo "красный (-DCAR_CRASH_ANG_OLD=1), обязан упасть:"; run "-DCAR_CRASH_ANG_OLD=1" && { echo "КРАСНЫЙ НЕ ПОЙМАН"; exit 1; } || echo "упал, как должен"

# РЕШЕНИЕ РОБА 10.11 (тур 102, Р6): физика скольжения о стену как в TEST99 (CAR_SCRAPE_OLD=1 по умолчанию): 82 → ~43, 57 → ~61 км/ч за 24 кадра (PS2 18 и 17, допуск расширен до 30…60 и 40…80); красный = правки 100–102 (-DCAR_SCRAPE_OLD=0: 27 и 20).
# Скользящий удар о стену (сверка PS2, плита b166 L4, Публичный-93, три заезда бота с газом и рулём влево): оригинал трёт стенку и теряет скорость быстро: 82 → 18 км/ч за 24 кадра, 57 → 17, головой 43 → 1,7.
# Порт стартует у узла PS2 (центр сдвинут к стене так, чтобы удар пришёл на кадр 0–2), газ держится, руль -0,8. Допуск: 82 → 3…30, 57 → 3…30, головой ≤ 6 км/ч. Красный: -DCAR_SCRAPE_OLD=1 (82 → ~45, 57 → ~40) и -DCAR_CRASH_ANG_OLD=1.
slide() { bash -c "V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST $1 -I viewer -I tools/bots tools/bots/wallram.c \$BOTS_LIB -lm -o $OUT/wallram_slide" || exit 2
  bad=0; while read -r nm x z h kmh lo hi; do r=$(cd "$D" && WR_STEER=-0.8 WR_START="$x $z $h" "$ROOT/$OUT/wallram_slide" level.shr player.shr $x $z $h $kmh 1 </dev/null); v=$(echo "$r" | sed -nE 's/.*v через 24 кадра (-?[0-9]+).*/\1/p')
    echo "  $nm: v через 24 кадра ${v:-?} км/ч (допуск ${lo}…${hi})"; [ -z "$v" ] || [ "$v" -lt "$lo" ] || [ "$v" -gt "$hi" ] && bad=1; done <<'EOT'
82 -187.92 236.72 115.8 82 30 60
57 -194.45 236.23 107.6 54 40 80
головой -198.71 236.97 179.5 44 -3 6
EOT
  return $bad; }
echo "скользящий удар (b166):"; slide "" && echo "PASS" || { echo "FAIL"; exit 1; }
echo "красный (-DCAR_SCRAPE_OLD=0, правки 100–102 вернули), обязан упасть:"; slide "-DCAR_SCRAPE_OLD=0" && { echo "КРАСНЫЙ НЕ ПОЙМАН"; exit 1; } || echo "упал, как должен"
