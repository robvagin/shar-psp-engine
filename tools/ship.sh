#!/bin/bash
# Выпуск сборки для Роба одной командой: движок -> данные -> смоук в эмуляторе -> _ok-копии -> (по просьбе) на PSP.
#   tools/ship.sh                      собрать EBOOT, пересобрать устаревшие данные, смоук, установить в build/
#   tools/ship.sh --level              + принудительно пересобрать level.shr (иначе — только если изменились входы/команда)
#   tools/ship.sh --use level.shr=build/level_ni.shr   взять этот файл как есть, без пересборки (можно несколько --use)
#   tools/ship.sh --deploy             + залить на PSP (/Volumes/PSP/PSP/GAME/SHARVIEW), сверить cmp; копируются только
#                                        изменившиеся файлы (совпавшие по cmp не переписываются), до копирования — проверка места
# Уровни 2–7 (29.09): набор уровня N едет в папку l<N>/ (строки {N} в tools/ship_rules.txt); SHIP_LEVELS="2 3" — только эти
#   tools/ship.sh --no-deploy          не заливать (ПО УМОЛЧАНИЮ, пока не сказано иначе: DEPLOY_DEFAULT ниже)
#   tools/ship.sh --no-install         не трогать build/ (ни _ok, ни установки): только собрать, проверить, показать
#   tools/ship.sh --no-smoke           без смоука (только если эмулятор занят надолго; в отчёте будет помечено)
#   tools/ship.sh --allow-low-quality  разрешить пересборку уровня ниже максимального качества (ТОЛЬКО по слову Роба)
#   tools/ship.sh --allow-seams        выпустить уровень 1, у которого швов больше эталона tools/seams_ref.tsv (ТОЛЬКО по слову Роба)
#   tools/ship.sh --allow-fps          выпустить, если смоук провален ТОЛЬКО по среднему FPS («FPS x < y»; падения, длинные кадры,
#                                        пропуски, память, GE — по-прежнему стоп). ТОЛЬКО по слову Роба (04.10: «мэдж новой версии на
#                                        PSP, полигоны и швы» — правка швов стоит ~2 % FPS в эмуляторе), в отчёте и STATE — пометка
# Какие данные едут и как собираются — tools/ship_rules.txt (+ tools/ship_files.sh: только то, что читает движок).
# Порядок: (a) EBOOT (psp-cmake при первом запуске); (b) данные — во временную build/ship_stage/, подмена только при
# успехе экспорта; (c) tools/smoke.sh на build/ship_stage/ — провал = стоп с причиной одной строкой; (d) прежние
# build/EBOOT.PBP и данные -> build/*_ok.*, новые — в build/, манифест SHA-256 build/ship_manifest.txt; ничего не коммитит;
# (e) --deploy: копия на PSP, cmp каждого файла, удаление macOS-файлов ._* там же; строка «на PSP: …»;
#     PSP не подключена -> «подключи PSP по USB, и я закину». SHIP_PSP=/путь — другой корень карты (для проверки скрипта).
set -u
DEPLOY_DEFAULT=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
export PYTHONPATH="$ROOT/tools/_ci${PYTHONPATH:+:$PYTHONPATH}"   # Linux/WSL: поиск файлов диска без учёта регистра (tools/ci_fs.py); на macOS ничего не меняет
. "$ROOT/tools/portable.sh"   # file_size, sha256: те же команды на macOS и Linux/WSL
STAGE="$ROOT/build/ship_stage"; PSPROOT="$(psp_root)"   # карта PSP: macOS /Volumes/PSP, Linux /media/$USER/PSP и др. (tools/portable.sh)
# временные psp_*.json/.bin конвертера — в свою папку этого запуска: параллельный экспорт из другой копии (worktree) пишет в общий
# hit-and-run-web/public/assets под теми же именами, и они портят друг другу данные (29.09: Bus error в psp_export)
export SHR_TMP="${SHR_TMP:-$ROOT/build/tmp_conv/ship_$$}"
DEPLOY=$DEPLOY_DEFAULT; FORCE_LEVEL=0; SMOKE=1; INSTALL=1; USE=(); ALLOW_LOW=0; ALLOW_SEAMS=0; ALLOW_FPS=0
while [ $# -gt 0 ]; do
  case "$1" in
    --level) FORCE_LEVEL=1 ;;
    --deploy) DEPLOY=1 ;;
    --no-deploy) DEPLOY=0 ;;
    --no-smoke) SMOKE=0 ;;
    --no-install) INSTALL=0 ;;
    --allow-low-quality) ALLOW_LOW=1 ;;
    --allow-seams) ALLOW_SEAMS=1 ;;
    --allow-fps) ALLOW_FPS=1 ;;
    --use) shift; USE+=("$1") ;;
    --use=*) USE+=("${1#--use=}") ;;
    *) echo "ship: неизвестный ключ $1 (см. начало tools/ship.sh)"; exit 2 ;;
  esac; shift
done
fail() { echo "ship: $*"; exit 1; }
# №1162: переносимость (Linux/WSL): stat -f и shasum есть только в macOS/BSD; размер — через wc, хеш — shasum или sha256sum. SHIP_PORTABLE1162_OLD=1 — как было
if [ "${SHIP_PORTABLE1162_OLD:-0}" = 1 ]; then fsize() { stat -f %z "$1"; }; sha256s() { shasum -a 256 "$@"; }
else
  fsize() { wc -c < "$1" | tr -d ' '; }
  if command -v shasum >/dev/null 2>&1; then sha256s() { shasum -a 256 "$@"; }; else sha256s() { sha256sum "$@"; }; fi
fi
# Закон 1 (docs/handoff/LOOP_TERMINAL.md): уровень пересобирается только с максимальным качеством картинки. Флаги читает
# tools/build_level.sh (незаданный = 1); любой другой -> отказ до сборки, build/level.shr не трогается.
# SHR_TEXQ (пусто или 1): 0 включает старый путь текстур FASTOCTREE в tools/psp_export.py
quality_check() {
  local v bad=""
  for v in SHR_TEX_CAP256 SHR_SMALL_MIPS SHR_LAYERS SHR_TEXQ; do [ "${!v:-1}" = 1 ] || bad="$bad $v=${!v}"; done
  [ -z "$bad" ] && return 0
  [ $ALLOW_LOW = 1 ] && { echo "ship: уровень ниже максимального качества:$bad (--allow-low-quality)"; return 0; }
  fail "отказ: уровень собрался бы ниже максимального качества:$bad (нужно 1; обход --allow-low-quality только по слову Роба)"
}
t0=$(date +%s); step() { echo "· $* ($(( $(date +%s) - t0 )) с)"; }
# №1162 (отчёт с GitHub №3): набор прошлого запуска НЕ стирается: правила, чьи входы и команда не менялись, берутся из него по хешам (ship_cache), поэтому упавший на
# середине выпуск (час сборки уровня) при повторном запуске продолжается с места падения, а уже собранный level.shr не теряется. SHIP_RESUME1162_OLD=1 — стирать, как раньше
if [ "${SHIP_RESUME1162_OLD:-0}" = 1 ]; then rm -rf "$STAGE"; fi; mkdir -p "$STAGE"; rm -rf "$STAGE/log"; LOG="$STAGE/log"; mkdir -p "$LOG"

# (a) движок
export PSPDEV="$ROOT/pspdev" PATH="$ROOT/pspdev/bin:$PATH"
# №1162: значок, живой значок и музыка игры в меню PSP из диска игрока (CMake берёт build/assets/* при настройке, поэтому до psp-cmake); XMB1162_OLD=1 — как было
tools/xmb_assets.sh | tee -a "$LOG/xmb.txt"
# чистая проверка 06.11: сборщик (app/core/build.sh) настраивает движок на шаге 2, до распаковки диска, и CMake запоминал «значков нет»
# -> EBOOT без ICON0/ICON1/PIC1/SND0. Файлы build/assets новее настройки -> перенастроить (ключи -D остаются из кеша)
XMB_NEWER=$(find build/assets -maxdepth 1 \( -name pic1.png -o -name icon0.png -o -name icon1.pmf -o -name snd0.at3 \) -newer viewer/build/CMakeCache.txt 2>/dev/null | head -1)
if [ ! -f viewer/build/Makefile ] || [ -n "$XMB_NEWER" ]; then
  (cd viewer && psp-cmake -S . -B build) > "$LOG/cmake.txt" 2>&1 || fail "psp-cmake: $(grep -m1 -i error "$LOG/cmake.txt")"
fi
cmake --build viewer/build > "$LOG/eboot.txt" 2>&1 || fail "сборка движка: $(grep -m1 'error' "$LOG/eboot.txt")"
cp viewer/build/EBOOT.PBP "$STAGE/"
step "EBOOT собран"

# (b) данные
. .venv/bin/activate
# №1162: наши правки в чужом разборщике диска (прозрачность вершин PS2); HRW_PATCH1162_OLD=1 — как было
python tools/hrw_patches/apply.py || fail "hit-and-run-web: правка разборщика не наложилась (tools/hrw_patches/apply.py)"
. tools/ship_data.sh

# (b1) №1162: пакеты языков интерфейса и текстов миссий (французский, немецкий, испанский: lang/F|G|S/…, tools/lang_build.py) и тексты лицензий шрифтов.
# Раньше их собирал отдельный шаг, которого в README не было, и публичный набор выходил только английским. SHIP_LANG1162_OLD=1 — как было
if [ "${SHIP_LANG1162_OLD:-0}" != 1 ]; then
  LV=1; for n in ${SHIP_LEVELS:-}; do LV="$LV,$n"; done
  if python tools/lang_build.py --levels "$LV" > "$LOG/lang.txt" 2>&1; then
    mkdir -p "$STAGE/lang"; for L in F G S; do [ -d "build/lang/$L" ] && rsync -a --exclude '_rows_*' --exclude '*.fonts' "build/lang/$L" "$STAGE/lang/"; done
    step "языки: $(find "$STAGE/lang" -type f | wc -l | tr -d ' ') файлов"
  else step "языки: lang_build.py с ошибками (см. $LOG/lang.txt); набор будет без части переводов"; fi
  mkdir -p "$STAGE/fonts-license"; cp assets/fonts/pixel/*license* assets/fonts/pixel/*readme* "$STAGE/fonts-license/" 2>/dev/null
fi

# (b2) сторож швов (правка Роба 106, 01.10: «не допускай, чтобы пересборка опять возвращала швы»): уровень 1 из ship_stage
# (пересобранный, --use или прежний) — швы кадра PSP на 22 видах (tools/seams_raster.py: дыры, сквозь, на земле, полосы
# 16-бит глубины; GE 24-бит матрицы) против эталона принятой сборки tools/seams_ref.tsv; хуже допуска — выпуск стоп.
# Обход --allow-seams только по слову Роба
if [ -f "$STAGE/level.shr" ]; then
  if python tools/seams_raster.py "$STAGE/level.shr" --gate > "$LOG/seams.txt" 2>&1; then step "швы: $(tail -1 "$LOG/seams.txt")"
  elif [ $ALLOW_SEAMS = 1 ]; then step "швы (--allow-seams): $(tail -1 "$LOG/seams.txt")"
  else fail "$(tail -1 "$LOG/seams.txt")"; fi
  # правки Роба 224/237/251 («швы очень мигают»): щели float24 GE на 12 видах (tools/seams_ge.py, модель железа, ~10 с) — уровень,
  # собранный по-старому (SHR_SEAMS_OLD=1: свой центр у куска, дальние версии), даёт сотни щелей — выпуск стоп
  if python tools/seams_ge.py "$STAGE/level.shr" --gate > "$LOG/seams_ge.txt" 2>&1; then step "швы GE: $(tail -1 "$LOG/seams_ge.txt")"
  elif [ $ALLOW_SEAMS = 1 ]; then step "швы GE (--allow-seams): $(tail -1 "$LOG/seams_ge.txt")"
  else fail "$(tail -1 "$LOG/seams_ge.txt")"; fi
fi

# (b3) правка Роба 268 («машина, которую не видно — въехал в пустоту»): у каждой машины мира (viewer/worldcars.h) есть модель в
# пакете машин миссий — иначе она стоит твёрдой и невидимой; нет — выпуск стоп
if [ -f "$STAGE/msncars-l1.shr" ]; then
  if python tests/msncars/world_cars.py "$STAGE/msncars-l1.shr" > "$LOG/world_cars.txt" 2>&1; then step "машины мира: модели есть"
  else fail "машина мира без модели: $(head -1 "$LOG/world_cars.txt")"; fi
fi

# (b4) №1166 (Роб 03.11: у собравших порт из публичного кода камера идёт сквозь стены): данные, по которым камера считает закон объектива, в наборе есть —
# коллизия COL1 в level.shr, камеры зон / машины / миссий / вырезы, комнаты. Нет любого — камера молча теряет правило, выпуск стоп. CAMDATA1166_OLD=1 — без проверки
if [ "${CAMDATA1166_OLD:-0}" != 1 ]; then
  if python tools/camera_data_gate.py "$STAGE" > "$LOG/camdata.txt" 2>&1; then step "$(tail -1 "$LOG/camdata.txt")"
  else fail "$(tail -1 "$LOG/camdata.txt")"; fi
fi

# (b5) №1162: SHIP_PUBSET=папка — копия набора «как лежит рядом с EBOOT» (данные, lang/, fonts-license, EBOOT без журналов и служебных .sha256/.texsrc.json); нужна,
# чтобы сверить публичный путь с нашим набором (tools/public_path_check.py) и проверить на нём камеру и тур
if [ -n "${SHIP_PUBSET:-}" ]; then
  mkdir -p "$SHIP_PUBSET" && rsync -a --delete --exclude log --exclude 'tmp_*' --exclude '*.inputs.sha256' --exclude '*.texsrc.json' "$STAGE/" "$SHIP_PUBSET/" && step "набор для сверки: $SHIP_PUBSET ($(find "$SHIP_PUBSET" -type f | wc -l | tr -d ' ') файлов)"
fi

# (c) смоук
SMOKE_NOTE="смоук пропущен (--no-smoke)"
# смоук гоняет эмулятор PPSSPP, запуск которого (tools/ppsspp_run.sh) написан под macOS: на Linux/WSL пропускается, игру проверяют руками в PPSSPP
# смоук там, где есть эмулятор: macOS — окно PPSSPPSDL.app, Linux — PPSSPPHeadless ($PPSSPP_HEADLESS, build/twins/bin или PATH; доработка noname8723, 06.11).
# Нет эмулятора — пропуск с пометкой в отчёте, не провал выпуска; SHIP_SMOKE_REQUIRED=1 — без эмулятора стоп
if [ $SMOKE = 1 ] && ! tools/smoke.sh --have-emulator >/dev/null 2>&1; then
  [ "${SHIP_SMOKE_REQUIRED:-0}" != 1 ] || fail "нет эмулятора PPSSPP для смоука (PPSSPP_HEADLESS=/путь/PPSSPPHeadless или --no-smoke)"
  SMOKE=0; SMOKE_NOTE="смоук пропущен: нет эмулятора PPSSPP на этой машине (PPSSPP_HEADLESS=/путь/PPSSPPHeadless включит его)"
fi
if [ $SMOKE = 1 ]; then
  step "смоук в эмуляторе"
  tools/smoke.sh "$STAGE" > "$LOG/smoke.txt" 2>&1; rc=$?
  rep=$(grep -m1 '^отчёт:' "$LOG/smoke.txt")
  SMOKE_NOTE="смоук OK ($rep)"
  if [ $rc != 0 ]; then
    first=$(head -1 "$LOG/smoke.txt")
    # только «FPS x < y» у каждой сцены провала (сцены через «; », причины через «, »)
    other=$(printf '%s\n' "${first#ПРОВАЛ: }" | tr ';' '\n' | sed 's/^ *[^:]*: //' | tr ',' '\n' | sed 's/^ *//' | grep -v -E '^FPS [0-9.]+ < [0-9]+$' | grep -c .)
    if [ $ALLOW_FPS = 1 ] && [ "$first" != "${first#ПРОВАЛ: }" ] && [ "$other" = 0 ]; then
      SMOKE_NOTE="смоук: только FPS ниже цели, выпуск по --allow-fps (слово Роба): ${first#ПРОВАЛ: } ($rep)"; step "$SMOKE_NOTE"
    else fail "смоук провален: $first ($rep)"; fi
  fi
fi

# объём набора: всего и по папкам уровней (место на карте памяти)
SIZE_NOTE=$(cd "$STAGE" && for f in EBOOT.PBP "${FILES[@]}"; do echo "$(file_size "$f") $f"; done | awk '{
  n = $2; k = "корень"; if (n ~ /^l[0-9]\//) k = substr(n, 1, 2); else if (n ~ /^movies\//) k = "ролики"; s[k] += $1; t += $1 }
  END { printf "набор %.0f МБ:", t / 1048576; for (k in s) printf " %s %.0f", k, s[k] / 1048576; printf " (МБ)" }')
step "$SIZE_NOTE"

# (d) прежний набор -> *_ok, новый -> build/
if [ $INSTALL = 1 ]; then
ok_name() { local p="$1" b="${1%.*}" e="${1##*.}"; echo "${b}_ok.$e"; }
[ -f build/EBOOT.PBP ] && cp build/EBOOT.PBP build/EBOOT_ok.PBP
cp "$STAGE/EBOOT.PBP" build/EBOOT.PBP
for nd in "${NEWDATA[@]:-}"; do
  [ -n "$nd" ] || continue; name=${nd%%=*}; path=${nd#*=}
  if [ -f "$path" ] && ! cmp -s "$path" "$STAGE/$name"; then cp -c "$path" "$(ok_name "$path")" 2>/dev/null || cp "$path" "$(ok_name "$path")"; fi
  mkdir -p "$(dirname "$path")"; cp -c "$STAGE/$name" "$path" 2>/dev/null || cp "$STAGE/$name" "$path"
  [ -s "$STAGE/$name.texsrc.json" ] && cp "$STAGE/$name.texsrc.json" "$path.texsrc.json"   # №1162: откуда взята каждая текстура уровня (нужно road934_export)
  if [ -f "$STAGE/$name.inputs.sha256" ]; then
    cp "$STAGE/$name.inputs.sha256" "$path.inputs.sha256" || fail "установка хеша $name"
  fi
done
{ echo "ship $(date '+%Y-%m-%d %H:%M:%S')  $SMOKE_NOTE"; echo "git $(git rev-parse --short HEAD 2>/dev/null)$(git diff --quiet 2>/dev/null || echo ' +изменения')"
  (cd "$STAGE" && sha256 EBOOT.PBP "${FILES[@]}"); } > build/ship_manifest.txt
tools/setrev.sh "$STAGE" build/ship_manifest.txt >/dev/null && cp "$STAGE/set.txt" build/set.txt   # TEST93 C1: штамп набора (номер выпуска данных, sha манифеста) — движок сверяет при старте
step "установлено в build/ (прежнее -> *_ok), манифест build/ship_manifest.txt; $SMOKE_NOTE"
else
  (cd "$STAGE" && sha256 EBOOT.PBP "${FILES[@]}") > "$STAGE/manifest.txt"
  tools/setrev.sh "$STAGE" "$STAGE/manifest.txt" >/dev/null   # TEST93 C1
  step "--no-install: build/ не тронут, набор в ${STAGE#$ROOT/}/ (манифест manifest.txt); $SMOKE_NOTE"
fi

# (e) PSP
if [ $DEPLOY = 0 ]; then echo "на PSP не копирую (по умолчанию; залить: tools/ship.sh --deploy)"; exit 0; fi
[ -d "$PSPROOT/PSP" ] || { echo "подключи PSP по USB, и я закину"; exit 0; }
D="$PSPROOT/PSP/GAME/SHARVIEW"; mkdir -p "$D" || fail "не создать $D"
# аудит ведущего 30.09 (P1-4) и урок 20:01: на карте может лежать чужая сборка (тестовая ведущего) — не затирать молча.
# build/deployed.txt — хеши EBOOT, которые выкладывал этот скрипт; EBOOT карты не из списка -> стоп (SHIP_FOREIGN_OK=1 — с ведома Роба)
DEPLOYED="$ROOT/build/deployed.txt"
if [ -f "$D/EBOOT.PBP" ]; then
  onCard=$(sha256 "$D/EBOOT.PBP" | cut -c1-8)
  if ! grep -q "^$onCard" "$DEPLOYED" 2>/dev/null && [ "${SHIP_FOREIGN_OK:-0}" != 1 ]; then
    fail "на карте чужая сборка EBOOT $onCard (не выкладка tools/ship.sh) — сначала сказать Робу, чья она; заменить: SHIP_FOREIGN_OK=1"
  fi
fi
# только изменившиеся: совпавший по cmp файл на карте не переписывается (наборы уровней 2–7 — сотни МБ)
CHG=(); same=0; need=0
for f in EBOOT.PBP set.txt "${FILES[@]}"; do   # set.txt — штамп набора (TEST93 C1)
  if [ -f "$STAGE/$f" ] || [ "$f" != set.txt ]; then :; else continue; fi
  if [ -f "$D/$f" ] && cmp -s "$STAGE/$f" "$D/$f"; then same=$((same + 1)); continue; fi
  old=0; [ -f "$D/$f" ] && old=$(file_size "$D/$f")
  CHG+=("$f"); need=$((need + $(file_size "$STAGE/$f") - old))
done
free=$(df -k "$PSPROOT" | awk 'NR == 2 {print $4 * 1024}')
[ "$need" -lt "${free:-0}" ] || fail "на карте памяти мало места: нужно ещё $((need / 1048576)) МБ, свободно $(( ${free:-0} / 1048576 )) МБ"
bad=""; mb=0
for f in "${CHG[@]:-}"; do
  [ -n "$f" ] || continue
  mkdir -p "$D/$(dirname "$f")" || fail "папка на PSP $f"
  cp "$STAGE/$f" "$D/$f" && cmp -s "$STAGE/$f" "$D/$f" || bad="$bad $f"
  mb=$((mb + $(file_size "$STAGE/$f")))
done
find "$D" -name '._*' -delete 2>/dev/null; sync
[ -z "$bad" ] || fail "на PSP не совпало:$bad"
more=""; [ ${#CHG[@]} -gt 12 ] && more=" …"
echo "на PSP: скопировано ${#CHG[@]} файлов ($((mb / 1048576)) МБ: ${CHG[*]:0:12}$more), без изменений $same — сверено cmp; $SIZE_NOTE"
# запись выкладки (P1-4): хеш в build/deployed.txt, строка «На PSP» в STATE.md и строка в PROGRESS.md — одним источником правды
eb=$(sha256 "$D/EBOOT.PBP" | cut -c1-8); head_=$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null); now=$(date '+%d.%m %H:%M')
echo "$eb $(date '+%Y-%m-%d %H:%M') main $head_" >> "$DEPLOYED"
# ELF выпуска с символами (аудит ведущего 01.10 22:35): строка кода по вылету — tools/crash_addr.py, ELF по хешу EBOOT
mkdir -p "$ROOT/build/crash/elf" && cp "$ROOT/viewer/build/sharview" "$ROOT/build/crash/elf/$eb.elf" 2>/dev/null || echo "ELF выпуска не сохранил"
dirty=$(git -C "$ROOT" status --porcelain --untracked-files=no -- viewer tools 2>/dev/null | wc -l | tr -d ' ')
prevmain=$(tail -2 "$DEPLOYED" | head -1 | awk '{print $NF}'); [ "$(wc -l < "$DEPLOYED")" -ge 2 ] || prevmain=""
# аудит 01.10 22:35: блок «На PSP» переписывается целиком (заголовок + что в сборке), а не только заголовок
python3 - "$ROOT/STATE.md" "$now" "$eb" "$head_" "$dirty" "$prevmain" "$ROOT" "${#CHG[@]}" <<'PYEOF' || echo "STATE.md: блок «На PSP» не обновил — поправь руками"
import sys, re, subprocess
p, now, eb, head, dirty, prev, root, nchg = sys.argv[1:9]; s = open(p, encoding='utf-8').read()
tail = f' + {dirty} незакоммиченных файлов viewer/ tools/' if dirty != '0' else ''
lines = [f'## На PSP (`PSP/GAME/SHARVIEW`, выложено {now}, сверено cmp; EBOOT `{eb}` от main `{head}`{tail} — tools/ship.sh --deploy)', '',
         f'- файлов скопировано: {nchg}; ELF с символами: `build/crash/elf/{eb}.elf` (вылет -> `tools/crash_addr.py`)']
if prev and prev != head:
    log = subprocess.run(['git', '-C', root, 'log', '--format=%h %s', f'{prev}..{head}'], capture_output=True, text=True).stdout.splitlines()
    lines.append(f'- в сборке с прошлой выкладки (main {prev}..{head}, {len(log)} коммитов):')
    lines += [f'  - `{l.split(" ", 1)[0]}` {l.split(" ", 1)[-1][:140]}' for l in log[:25]] + ([f'  - … ещё {len(log) - 25}'] if len(log) > 25 else [])
lines.append('- ответ Роба по этой сборке: ждём (записать в docs/handoff/ВЕРДИКТЫ.md)')
m = re.search(r'^## На PSP[^\n]*\n(?:(?!^## ).*\n?)*', s, flags=re.M)
if m: open(p, 'w', encoding='utf-8').write(s[:m.start()] + '\n'.join(lines) + '\n\n' + s[m.end():].lstrip('\n'))
else: sys.exit(1)
PYEOF
echo "$(date '+%Y-%m-%d %H:%M') · выкладка на PSP: EBOOT $eb от main $head_, файлов ${#CHG[@]}, сверено cmp (tools/ship.sh --deploy)" >> "$ROOT/PROGRESS.md"
