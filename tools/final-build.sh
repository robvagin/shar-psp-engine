#!/bin/bash
# Финальная сборка одним запуском (Sonnet-Сборка3, 10.10): из текущей ветки
#   1) 7 уровней последовательно: level.shr (tools/build_level.sh: экспорт + PVS + wind)
#      (в level.shr входит SPR1 стейтпропов из tools/prp2_export.py: кованый забор кладбища по массе, 5480c283, пересобирается здесь на всех семи уровнях)
#   2) звук каждого уровня: sfx.sab + music.sap (tools/sound_export.py; в нём сирена полиции №426)
#   3) будка и пакеты машин: phone.shr, phonecars.txt, msncars-lN.shr (tools/shop_export.py, tools/msncar_export.py)
#   4) 7 бонус-трасс b01..b07 (.shr/.bns), msncars-b, звук бонуса (tools/bonus/export.py --all, tools/bonus/audio_export.py)
#   5) EBOOT (psp-cmake + cmake --build)
#   6) пакет: копия базы (BASE, по умолчанию build/stage2 соседней сборки) + всё выше поверх, манифест SHA-256
#   7) итог одной строкой: «сборка <sha> EBOOT <sha256> пакет <папка> msncars ... трассы ...»
# Ключи:
#   --list     только показать шаги (ничего не запускает)
#   --resume   пропускать шаги, чей результат уже лежит в рабочей папке и отмечен .done (повтор после обрыва)
#   --skip X   пропустить группу: levels | ui | audio | phone | bonus | eboot (можно несколько раз)
#   --release  РЕЛИЗНАЯ сборка EBOOT: -DSHAR_RELEASE=ON (viewer/release.h) — служебные файлы-флаги, журналы, SELECT=RESET CAR и счётчик выключены, в пакет не кладутся
#              dev-файлы; по умолчанию ВЫКЛЮЧЕН (релиз — только по слову Роба). Что именно, список и решения Роба: docs/handoff/РЕЛИЗ_СЛУЖЕБНОЕ_2026-10-10.md.
#              Решения Роба — переменными: RELEASE_KEEP_DIAG=1 (самописец blackbox/crash и sharexc.prx), RELEASE_KEEP_HUD=1 (счётчик FPS/RAM у игрока),
#              RELEASE_KEEP_RESET=1 (SELECT=RESET CAR в машине)
# Переменные: BASE (папка базы пакета), PKG (куда класть пакет), PSPDEV, MAXJOBS (по умолчанию 1: последовательно)
# Правила: тяжёлые процессы по одному; перед каждым экспортом ждём, пока не закончится ЧУЖОЙ psp_export (pgrep); без push; на PSP не кладёт.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
LIST=0; RESUME=0; SKIP=" "; RELEASE=0
while [ $# -gt 0 ]; do case "$1" in
  --list) LIST=1;; --resume) RESUME=1;; --release) RELEASE=1;; --skip) shift; SKIP="$SKIP$1 ";; *) echo "неизвестный ключ $1" >&2; exit 2;; esac; shift; done
BASE="${BASE:-$HOME/Downloads/shar-psp-build2-w/build/stage2}"
STAMP="$(date +%Y%m%d-%H%M)"
WORK="$ROOT/build/final-work"; PKG="${PKG:-$ROOT/build/final-$STAMP}"
EBUILD="viewer/build"; RELFLAGS=""; EBTAG="eboot"
RELFLAGS="-DSHAR_JITTER_DEFAULT=OFF"; [ "${JITTER_DEFAULT:-0}" = 1 ] && RELFLAGS="-DSHAR_JITTER_DEFAULT=ON"   # слово Роба 10.10 вечер: сглаживание дали (viewer/jitter.h) в финале ВЫКЛЮЧЕНО (по умолчанию не включается в коде, файла jitter нет, в пакете nojitter); JITTER_DEFAULT=1 вернёт 9D в коде
if [ $RELEASE = 1 ]; then   # отдельная папка сборки и пакета: релизный EBOOT не смешивается с обычным
  EBUILD="viewer/build-release"; PKG="${PKG}-release"; EBTAG="eboot-release"   # данные уровней те же (WORK общий, --resume их не пересобирает), отличается только EBOOT
  RELFLAGS="$RELFLAGS -DSHAR_RELEASE=ON"; [ "${RELEASE_KEEP_DIAG:-0}" = 1 ] && RELFLAGS="$RELFLAGS -DSHAR_RELEASE_KEEP_DIAG=ON"
  [ "${RELEASE_KEEP_HUD:-0}" = 1 ] && RELFLAGS="$RELFLAGS -DSHAR_RELEASE_KEEP_HUD=ON"; [ "${RELEASE_KEEP_RESET:-0}" = 1 ] && RELFLAGS="$RELFLAGS -DSHAR_RELEASE_KEEP_RESET=ON"
  echo "РЕЛИЗ: $RELFLAGS (сборка $EBUILD, пакет $PKG)"
fi
export PSPDEV="${PSPDEV:-$ROOT/pspdev}"; export PATH="$PSPDEV/bin:$PATH"
want() { case "$SKIP" in *" $1 "*) return 1;; esac; return 0; }
step() { # step ИМЯ команда...   (--list: печатает; иначе выполняет, лог в WORK/logs)
  local name="$1"; shift
  if [ $LIST = 1 ]; then echo "шаг: $name :: $*"; return 0; fi
  if [ $RESUME = 1 ] && [ -f "$WORK/done/$name" ]; then echo "есть: $name"; return 0; fi
  echo "== $name"; mkdir -p "$WORK/logs" "$WORK/done"
  if "$@" > "$WORK/logs/$name.log" 2>&1; then touch "$WORK/done/$name"; else echo "СБОЙ: $name (лог $WORK/logs/$name.log)" >&2; tail -3 "$WORK/logs/$name.log" >&2; exit 1; fi
}
wait_foreign_export() { # чужой psp_export (не из этого дерева) идёт — ждать
  while pgrep -f "psp_export.py" | while read p; do
      cwd=$(lsof -a -p "$p" -d cwd -Fn 2>/dev/null | sed -n 's/^n//p'); [ "$cwd" != "$ROOT" ] && echo "$p"; done | grep -q .; do
    echo "чужой psp_export идёт, жду"; sleep 30; done
}
guard() { [ $LIST = 1 ] && return 0; wait_foreign_export; }

# ---- проверки окружения
if [ $LIST = 0 ]; then
  for l in .venv hit-and-run-web pspdev donut; do [ -e "$l" ] || ln -s "$HOME/Downloads/shar-psp/$l" "$l"; done
  [ -d "$BASE" ] || { echo "нет базы пакета $BASE" >&2; exit 1; }
  git diff --quiet HEAD -- viewer tools || echo "ВНИМАНИЕ: в viewer/tools есть незакоммиченные правки" >&2
fi
# json панели типографики Роба (build/ui_typo.json читают ui_export, load_export, menu_export, shop_export, screens_export): по умолчанию свежий из копии типографики
TYPO_JSON="${TYPO_JSON:-$HOME/Downloads/shar-psp-sonnet-typo-w/build/ui_typo.json}"; [ -f "$TYPO_JSON" ] || TYPO_JSON="$HOME/Downloads/shar-psp/build/ui_typo.json"
if [ $LIST = 0 ]; then [ -f "$TYPO_JSON" ] || { echo "нет ui_typo.json ($TYPO_JSON)" >&2; exit 1; }; ln -sf "$TYPO_JSON" build/ui_typo.json; fi
echo "панель типографики: $TYPO_JSON"
HEAD_SHA="$(git rev-parse --short HEAD)"
echo "сборка из $(git rev-parse --abbrev-ref HEAD) @ $HEAD_SHA; база $BASE; работа $WORK; пакет $PKG"

# ---- 1. уровни (последовательно)
if want levels; then for L in 1 2 3 4 5 6 7; do
  guard; step "level-$L" env LEVEL=$L tools/build_level.sh "$WORK/l$L/level.shr"
done; fi
# ---- 1b. трафик и стоящие машины (traffic.shr: правило парковок №490 живёт в экспорте)
if want levels; then for L in 1 2 3 4 5 6 7; do
  step "traffic-$L" bash -c ". .venv/bin/activate && SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 SHR_LAYERS=1 python tools/traffic_export.py --level $L --out '$WORK/l$L/traffic.shr'"
done; fi
# ---- 1c. гэги, осы, карточки уровня (collect-lN.shr: сценки AddGagBinding в интерьерах Мо/DMV/L3, №483)
if want levels; then for L in 1 2 3 4 5 6 7; do
  step "collect-$L" bash -c ". .venv/bin/activate && SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 SHR_LAYERS=1 python tools/collect_export.py --level $L --out '$WORK/l$L/collect-l$L.shr'"
done; fi
# ---- 1d. экран загрузки с газетой (load.shr: огонёк фитиля №499 в tools/load_export.py)
if want levels; then
  [ $LIST = 1 ] || [ -f build/ui_typo.json ] || ln -s "$HOME/Downloads/shar-psp/build/ui_typo.json" build/ui_typo.json 2>/dev/null
  step "load-pack" bash -c ". .venv/bin/activate && python tools/load_export.py --pack load --out '$WORK/load.shr'"
fi
# ---- 1e. интерфейс и экраны (по панели типографики Роба): ui, menu, pmenu, pextra, fsave, psave, shop, бонус-экраны (mini)
if want ui; then
  step "ui-pack-root" bash -c ". .venv/bin/activate && python tools/ui_export.py --out '$WORK/ui.shr' && python tools/menu_export.py --pack menu --out '$WORK/menu.shr' && python tools/menu_export.py --pack pmenu --out '$WORK/pmenu.shr' && python tools/menu_export.py --pack pextra --out '$WORK/pextra.shr' && python tools/save_export.py --pack fsave --out '$WORK/fsave.shr' && python tools/save_export.py --pack psave --out '$WORK/psave.shr' && python tools/shop_export.py --ui-only --out '$WORK/shop.shr'"
  for L in 2 3 4 5 6 7; do
    step "ui-pack-$L" bash -c ". .venv/bin/activate && mkdir -p '$WORK/l$L' && LEVEL=$L python tools/ui_export.py --out '$WORK/l$L/ui.shr' && LEVEL=$L python tools/menu_export.py --pack pmenu --out '$WORK/l$L/pmenu.shr' && LEVEL=$L python tools/menu_export.py --pack pextra --out '$WORK/l$L/pextra.shr' && LEVEL=$L python tools/save_export.py --pack psave --out '$WORK/l$L/psave.shr' && python tools/shop_export.py --level $L --ui-only --out '$WORK/l$L/shop.shr'"
  done
  step "ui-pack-mini" bash -c ". .venv/bin/activate && mkdir -p '$WORK/bonusui' && python tools/bonus/screens_export.py --out '$WORK/bonusui/mini.shr'"
fi
# ---- 2. звук уровней
if want audio; then for L in 1 2 3 4 5 6 7; do
  step "audio-$L" bash -c ". .venv/bin/activate && python tools/sound_export.py --level $L --out '$WORK/l$L/audio'"
done; fi
# ---- 3. будка и пакеты машин
if want phone; then for L in 1 2 3 4 5 6 7; do
  step "phonecars-$L" bash -c ". .venv/bin/activate && python tools/shop_export.py --level $L --phone-list --out '$WORK/l$L/phonecars.txt'"
  step "phone-$L"     bash -c ". .venv/bin/activate && python tools/shop_export.py --level $L --phone-ui --out '$WORK/l$L/phone.shr'"
  step "msncars-$L"   bash -c ". .venv/bin/activate && python tools/msncar_export.py --level $L --out '$WORK/l$L/msncars-l$L.shr'"
done; fi
# ---- 4. бонус
if want bonus; then
  guard; step "bonus-tracks" bash -c ". .venv/bin/activate && python tools/bonus/export.py --all --out '$WORK/bonus'"
  step "bonus-audio" bash -c ". .venv/bin/activate && python tools/bonus/audio_export.py --out '$WORK/bonus/audio'"
fi
# ---- 4b. лучи мигалок полиции (№495): viewer/police_beam_data.h закоммичен; шаг проверяет, что генератор с диска даёт ровно его (иначе остановка до EBOOT)
if want eboot; then
  step "police-beam-check" bash -c ". .venv/bin/activate && python tools/police_beam_export.py --out '$WORK/police_beam_data.h' && cmp '$WORK/police_beam_data.h' viewer/police_beam_data.h"
fi
# ---- 5. EBOOT
PIC1_SRC="${PIC1_SRC:-$HOME/Downloads/psp-studio/_crops/package/PIC1.PNG}"   # фон игры в меню PSP (слово Роба 10.10): кадрирование с обложкой, 480x272; ICON0 не трогаем (прежняя иконка)
if want eboot; then
  [ $LIST = 1 ] || { [ -f "$PIC1_SRC" ] || { echo "нет фона PIC1: $PIC1_SRC" >&2; exit 1; }; mkdir -p build/assets; cp "$PIC1_SRC" build/assets/pic1.png; }   # ресурс сборки, в git не попадает
  # ICON0 / ICON1 (видео-превью в XMB) / SND0 (музыка при наведении): ресурсы сборки лежат в build/assets (в git не попадают); в чистой копии их нет, и EBOOT выходил без них (12.10, пакет 0131: Роб потерял видео).
  # Берём из основной копии (ASSETS_SRC), без них сборка останавливается; после сборки проверка ниже
  ASSETS_SRC="${ASSETS_SRC:-$HOME/Downloads/shar-psp/build/assets}"
  [ $LIST = 1 ] || { mkdir -p build/assets; for f in icon0.png icon1.pmf snd0.at3; do [ -f "build/assets/$f" ] || { [ -f "$ASSETS_SRC/$f" ] && cp "$ASSETS_SRC/$f" "build/assets/$f"; }; [ -s "build/assets/$f" ] || { echo "нет build/assets/$f (источник $ASSETS_SRC): EBOOT был бы без значка/видео/музыки XMB" >&2; exit 1; }; done; }
  step "$EBTAG-configure" psp-cmake -S viewer -B $EBUILD $RELFLAGS
  step "$EBTAG-build" cmake --build $EBUILD --parallel 3
fi
# ---- 6-7. пакет, манифест, итог
pack() {
  rm -rf "$PKG"; cp -cR "$BASE" "$PKG"
  for L in 1 2 3 4 5 6 7; do d="$PKG"; [ $L != 1 ] && d="$PKG/l$L"
    [ -f "$WORK/l$L/level.shr" ] && cp "$WORK/l$L/level.shr" "$WORK/l$L/level.shr.texsrc.json" "$d/"
    [ -d "$WORK/l$L/audio" ] && cp "$WORK/l$L/audio/sfx.sab" "$WORK/l$L/audio/music.sap" "$d/audio/"
    for f in "collect-l$L.shr" traffic.shr phone.shr phonecars.txt "msncars-l$L.shr"; do [ -f "$WORK/l$L/$f" ] && cp "$WORK/l$L/$f" "$d/"; done
  done
  mkdir -p "$PKG/bonus"
  for f in "$WORK"/bonus/b0*.shr "$WORK"/bonus/b0*.bns "$WORK"/bonus/b0*.texsrc.json "$WORK"/bonus/msncars-b.shr; do [ -f "$f" ] && cp "$f" "$PKG/bonus/"; done
  [ -d "$WORK/bonus/audio" ] && { mkdir -p "$PKG/bonus/audio"; cp "$WORK/bonus/audio/sfx.sab" "$WORK/bonus/audio/music.sap" "$PKG/bonus/audio/"; }
  [ -f "$WORK/load.shr" ] && cp "$WORK/load.shr" "$PKG/load.shr"
  for f in ui.shr menu.shr scrap.shr pmenu.shr pextra.shr scrapgal.shr fsave.shr psave.shr shop.shr; do [ -f "$WORK/$f" ] && cp "$WORK/$f" "$PKG/$f"; done
  for L in 2 3 4 5 6 7; do for f in ui.shr pmenu.shr pextra.shr scrapgal.shr psave.shr shop.shr; do [ -f "$WORK/l$L/$f" ] && cp "$WORK/l$L/$f" "$PKG/l$L/$f"; done; done
  [ -f "$WORK/bonusui/mini.shr" ] && { mkdir -p "$PKG/bonus"; cp "$WORK/bonusui/mini.shr" "$PKG/bonus/mini.shr"; }
  # шрифты Patrick H. Lauke (CC BY 3.0, 11.10): если пакет интерфейса использует хоть один (рядом с пакетом <пакет>.fonts, tools/pixel_fonts.py) — страница «FONT CREDITS» после
  # дисклеймера (viewer/bootscreen.h читает fontcredit.txt) и файлы лицензий рядом с EBOOT; не использует — ни страницы, ни файлов
  rm -rf "$PKG/fontcredit.txt" "$PKG/fonts-license"; FC=$(.venv/bin/python tools/pixel_fonts.py "$WORK")
  [ -n "$FC" ] && { mkdir -p "$PKG/fonts-license"; cp assets/fonts/pixel/*license* assets/fonts/pixel/*readme* "$PKG/fonts-license/"; }   # 11.10: отдельной страницы при запуске нет, авторы — в Options → View Credits; лицензии рядом с EBOOT остаются
  # ME (слово Роба 500): звук на ME включён в коде по умолчанию (viewer/me/meprobe.c meAudioBoot), ручные файлы не нужны; в пакете обязателен модуль ME,
  # а откатные флаги игрока (meaudiooff, nome) в пакет не кладутся
  [ -f "$PKG/sharme.prx" ] || { echo "в пакете нет sharme.prx: звук на ME не поднимется" >&2; exit 1; }
  rm -f "$PKG/meaudiooff" "$PKG/nome" "$PKG/mepeak" "$PKG/mefx" "$PKG/meaudio" "$PKG/mefxmax"
  # джиттер (слово Роба 10.10 вечер): ВЫКЛЮЧЕН. Файла jitter в пакете нет, лежит nojitter (явное выключение); в релизе служебные файлы вырезаются (release.h), там выключено кодом (JITTER_DEFAULT=0)
  rm -f "$PKG/jitter"; [ "${JITTER_DEFAULT:-0}" = 1 ] && { [ $RELEASE = 1 ] || printf "40 80" > "$PKG/jitter"; rm -f "$PKG/nojitter"; } || { [ $RELEASE = 1 ] || : > "$PKG/nojitter"; }
  cp $EBUILD/EBOOT.PBP "$PKG/EBOOT.PBP"
  # PIC1 в собранном EBOOT побайтно равен выбранному кадру (слово Роба 10.10)
  PCHK="$WORK/pbp-check"; rm -rf "$PCHK"; mkdir -p "$PCHK"; (cd "$PCHK" && unpack-pbp "$ROOT/$PKG/EBOOT.PBP" >/dev/null 2>&1 || unpack-pbp "$PKG/EBOOT.PBP" >/dev/null 2>&1)
  for pr in ICON0.PNG:icon0.png ICON1.PMF:icon1.pmf SND0.AT3:snd0.at3; do   # значок, видео-превью и музыка XMB обязаны быть в EBOOT и совпасть с build/assets (иначе остановка)
    [ -s "$PCHK/${pr%%:*}" ] && cmp -s "$PCHK/${pr%%:*}" "build/assets/${pr##*:}" || { echo "в EBOOT нет ${pr%%:*} или он не равен build/assets/${pr##*:}" >&2; exit 1; }
  done
  if [ -f "$PCHK/PIC1.PNG" ]; then cmp -s "$PCHK/PIC1.PNG" "$PIC1_SRC" && echo "PIC1 в EBOOT совпадает с $PIC1_SRC" || { echo "PIC1 в EBOOT НЕ совпадает с $PIC1_SRC" >&2; exit 1; }; else echo "в EBOOT нет PIC1.PNG" >&2; exit 1; fi
  if [ $RELEASE = 1 ]; then   # служебные файлы в пакет релиза не попадают (список — docs/handoff/РЕЛИЗ_СЛУЖЕБНОЕ_2026-10-10.md, viewer/release.h)
    for f in service toon fb32 view60 slowmo slowio autobench autodrive autostraight pvscheck pvscheck.txt bench.txt goto.txt opts.txt camang.txt carseq.txt \
             testdmg testboom clearchk flypath fpeprobe fputrap gagskinstreamold mefxmax mepeak near07 nolayer oldfog oldstream pcold seamchk seamsold \
             streamlog stream.txt smokelog scenario.txt ballast smoke.txt intro.txt sfxlog sfxlog.txt trace.txt; do rm -f "$PKG/$f"; done
    rm -f "$PKG"/shot_*.ok
    [ "${RELEASE_KEEP_DIAG:-0}" = 1 ] || rm -f "$PKG/blackbox.txt" "$PKG/blackbox_prev.txt" "$PKG/crash.txt" "$PKG/crash_prev.txt" "$PKG/sharexc.prx"
  fi
  (cd "$PKG" && find . -type f ! -name .DS_Store -print0 | sort -z | xargs -0 shasum -a 256 > "$PKG.manifest.txt")
  E=$(shasum -a 256 "$PKG/EBOOT.PBP" | cut -d' ' -f1)
  MS=""; for L in 1 2 3 4 5 6 7; do d="$PKG"; [ $L != 1 ] && d="$PKG/l$L"; MS="$MS L$L=$(stat -f %z "$d/msncars-l$L.shr")"; done
  TR=""; for n in 1 2 3 4 5 6 7; do TR="$TR b0$n=$(stat -f %z "$PKG/bonus/b0$n.shr")"; done
  echo "$([ $RELEASE = 1 ] && echo "RELEASE ")сборка $HEAD_SHA EBOOT $E пакет $PKG манифест $PKG.manifest.txt msncars:$MS трассы:$TR"
}
if [ $LIST = 1 ]; then echo "шаг: pack :: копия базы + результаты + EBOOT + манифест + итоговая строка"; exit 0; fi
pack | tee "$WORK/final-line.txt"
echo "На PSP не клал. Ждём «клади»."
