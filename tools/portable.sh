# Переносимые обёртки для macOS и Linux/WSL: . tools/portable.sh (ship.sh и другие скрипты сборки).
# macOS: stat -f (BSD), shasum из системы. Linux/WSL: stat -c (GNU), shasum или sha256sum — что есть.
if [ "$(uname -s)" = Darwin ]; then
  file_size() { stat -f %z "$1"; }          # размер файла в байтах
  file_mtime() { stat -f %m "$1"; }         # время изменения, секунды эпохи
  cpu_count() { sysctl -n hw.ncpu; }
else
  file_size() { stat -c %s "$1"; }
  file_mtime() { stat -c %Y "$1"; }
  cpu_count() { nproc 2>/dev/null || echo 1; }
fi
# корень карты памяти PSP по USB (доработка noname8723 для Ubuntu, 06.11): SHIP_PSP — свой путь; macOS /Volumes/PSP; Linux /media/$USER/PSP,
# /run/media/$USER/PSP, /mnt/PSP; иначе любой том с папкой PSP/GAME (у карты может быть другая метка). Не нашли — путь по умолчанию системы
psp_root() {
  [ -n "${SHIP_PSP:-}" ] && { echo "$SHIP_PSP"; return; }
  local d u="${USER:-$(id -un)}"
  for d in /Volumes/PSP "/media/$u/PSP" "/run/media/$u/PSP" /media/PSP /mnt/PSP /mnt/psp; do [ -d "$d/PSP" ] && { echo "$d"; return; }; done
  for d in /Volumes/* /media/*/* /run/media/*/* /mnt/*; do [ -d "$d/PSP/GAME" ] && { echo "$d"; return; }; done
  if [ "$(uname -s)" = Darwin ]; then echo /Volumes/PSP; else echo "/media/$u/PSP"; fi
}
sha256() {                                  # как «shasum -a 256 файлы…»: строки «хеш  имя»
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$@"; else sha256sum "$@"; fi
}
