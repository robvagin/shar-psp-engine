#!/bin/bash
# №1162 («чужой экспорт = наш»): картинки и звук игры в меню PSP (XMB) — ICON0, ICON1.PMF, SND0.AT3 (PIC1 см. ниже) — из диска игрока, по тем же
# правилам и с теми же числами, что у автора. CMakeLists.txt viewer/ кладёт их в EBOOT, если лежат в build/assets/, поэтому ship.sh зовёт этот скрипт ДО сборки движка.
#   tools/xmb_assets.sh            делает только отсутствующее; готовые файлы в build/assets/ (у автора принятые Робом) не трогает
#   tools/xmb_assets.sh --force    пересобрать всё из диска
# Принятые автором значения (дока ПРАВКИ_РОБА №110, 130, 131, 146, 262, 344): ICON1 — монтаж демо-ролика диска, 9 кусков, 20,02 с, crf 27 (умолчание tools/icon1.py);
# SND0 — «Sunday Drive» с диска, с начала, 16 с, затухание 2 с, ATRAC3 132 кбит/с (нужен atracdenc, его ставит setup.sh).
# ICON0 — знак gamelogo с диска (карточка Роба — тот же знак, но его файл в репозиторий не кладётся).
# PIC1 (фон XMB, 480×272) — обложка игры: на диске её нет; готовый PIC1.PNG (ровно наш, sha256 b529e282…) лежит в репозитории: assets/xmb/PIC1.PNG, идёт в EBOOT как есть, без обрезки.
# Свой файл кладётся в build/assets/ (PIC1.PNG или pic1.png) и сильнее. Нет такого файла, но есть большая картинка (build/assets/pic1-source.png
# или PIC1_SOURCE=путь, обложка 1280×720 и крупнее) — tools/pic1.py режет её окном и затемнением автора (приближённо: наш PIC1 доделан вручную, побайтно так не выйдет).
# Нет ничего — строка в журнале, фон XMB стандартный. Готовый ICON0.PNG (144×80) кладётся так же и берётся как есть.
# XMB1162_OLD=1 — ничего не делать (прежнее: публичная сборка без этих файлов)
cd "$(dirname "$0")/.." || exit 1
[ "${XMB1162_OLD:-0}" = 1 ] && exit 0
FORCE=0; [ "${1:-}" = "--force" ] && FORCE=1
PY=.venv/bin/python; [ -x "$PY" ] || PY=python3
A=build/assets; mkdir -p "$A"
need() { [ $FORCE = 1 ] || [ ! -s "$A/$1" ]; }
if need icon1.pmf; then
  $PY tools/icon1.py --out "$A/icon1.pmf" > "$A/icon1.log" 2>&1 && echo "xmb: icon1.pmf готов" || { echo "xmb: ICON1 не собрался (см. $A/icon1.log; нужен ffmpeg с декодером IPU)"; rm -f "$A/icon1.pmf"; }
fi
if need snd0.at3; then
  $PY tools/snd0.py --out "$A/snd0.at3" --start 0 --dur 16 --fade-out 2 > "$A/snd0.log" 2>&1 && echo "xmb: snd0.at3 готов" || { echo "xmb: SND0 пропущен (см. $A/snd0.log; нужен atracdenc: setup.sh)"; rm -f "$A/snd0.at3"; }
fi
# готовые картинки автора: PIC1.PNG и ICON0.PNG в любом регистре имени (на Linux регистр различается), размер проверяется
ready() {   # ready ИМЯ_БЕЗ_РАСШИРЕНИЯ ШИРИНА ВЫСОТА: готовый <имя>.png (любой регистр имени) — сперва в build/assets/ (переопределение), потом assets/xmb/ (лежит в репозитории);
  # нужного размера — ложится как build/assets/<имя>.png; не того размера — отложен как .bad
  local d n f
  for d in "$A" assets/xmb; do
    for n in $(ls "$d" 2>/dev/null | grep -i "^$1\.png$"); do
      f="$d/$n"
      if $PY - "$f" "$2" "$3" <<'PYEOF'
import sys
from PIL import Image
w, h = Image.open(sys.argv[1]).size
sys.exit(0 if (w, h) == (int(sys.argv[2]), int(sys.argv[3])) else 1)
PYEOF
      then [ "$f" -ef "$A/$1.png" ] || cp "$f" "$A/$1.png"; echo "xmb: $1.png = $f (готовый файл, как есть)"; return 0
      else mv "$f" "$f.bad"; echo "xmb: $f не $2×$3 PNG, отложен как .bad"; fi
    done
  done
  return 1
}
ready pic1 480 272 || true
ready icon0 144 80 || true
if [ ! -s "$A/pic1.png" ]; then
  SRC1="${PIC1_SOURCE:-$A/pic1-source.png}"
  if [ -f "$SRC1" ]; then $PY tools/pic1.py --src "$SRC1" --out "$A/pic1.png" > "$A/pic1.log" 2>&1 && echo "xmb: pic1.png из $SRC1 (окно и затемнение автора, приближённо)" || { echo "xmb: PIC1 не вышел (см. $A/pic1.log)"; rm -f "$A/pic1.png"; }
  else echo "xmb: PIC1 нет: нет assets/xmb/PIC1.PNG (480×272) и build/assets/pic1.png; без него в меню PSP стандартный фон"; fi
fi
if need icon0.png; then
  if [ -f "$A/icon0-source-rob.png" ]; then $PY tools/icon0.py --out "$A/icon0.png" > "$A/icon0.log" 2>&1
  else $PY tools/icon0.py --src disc --out "$A/icon0.png" > "$A/icon0.log" 2>&1; fi \
    && echo "xmb: icon0.png готов" || { echo "xmb: ICON0 пропущен (см. $A/icon0.log)"; rm -f "$A/icon0.png"; }
fi
exit 0
