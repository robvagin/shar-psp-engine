#!/bin/bash
# Выпуск финала SHARVIEW одной командой (17.10.2026, решения Дирижёра по аудиту входов): тот же набор и те же ключи EBOOT, что у TEST27/28.
#   tools/release_build.sh OUT [CARD_SHARVIEW]
#     OUT            папка результата: OUT/PSP/GAME/SHARVIEW/ (EBOOT «The Simpsons Hit & Run» + все файлы набора), OUT/MANIFEST_SHA256.txt, OUT/BUILD_INFO.txt, OUT/ЗАМЕНА.txt
#     CARD_SHARVIEW  с чем сравнивать для ЗАМЕНА.txt (только чтение; по умолчанию /Volumes/PSP/PSP/GAME/SHARVIEW, нет карты — без сравнения, все файлы «новые»)
# Источник данных один: tools/test30_set.sh (ship_files --pairs из пакета данных + наложения: уровни, traffic, ui-pkg, interiors, male2/msncars/horizon-файлы), режим FINAL=1:
#   EBOOT = RELEASE + KEEP_DIAG (ящик асинхронный) + KEEP_HUD (счётчик скрыт, R+SELECT; файла hudon в финале нет) + JITTER_*_OLD, сборка только из чистого дерева,
#   название через tools/eboot_title.py --preset final, frame_gate обязателен (нет PASS — выход 1). Игровые сохранения (save00/03, settings.bin, devsave) и pcar-*.shr набор не трогает: файлов с такими именами в наборе нет.
# Переменные: те же, что у test30_set.sh (DATA, LV_SRC, TRF_SRC, UI_SRC, PERS_SRC, HZ_SRC, CEIL_SRC); TITLE_PRESET не нужен (final).
. "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/linux_env.sh"   # Linux: обёртки stat/cp/sed под синтаксис macOS (tools/linux_env.sh)
set -eu
cd "$(dirname "$0")/.."; ROOT="$PWD"
OUT="${1:?укажи папку результата}"; CARD="${2:-/Volumes/PSP/PSP/GAME/SHARVIEW}"
SN=finalset FINAL=1 GATE=1 tools/test30_set.sh
SET="$ROOT/build/finalset"
rm -rf "$OUT"; mkdir -p "$OUT/PSP/GAME"
mv "$SET/PSP/GAME/SHARTEST15" "$OUT/PSP/GAME/SHARVIEW"
cp "$SET/BUILD_INFO.txt" "$OUT/BUILD_INFO.txt"
# манифест относительно папки игры: пути те же, имя папки не входит
( cd "$OUT/PSP/GAME/SHARVIEW" && find . -type f | sort | while read f; do printf "%s  %s\n" "$(shasum -a 256 "$f" | cut -d' ' -f1)" "$f"; done ) > "$OUT/MANIFEST_SHA256.txt"
python3 - "$OUT" "$CARD" <<'PY'
import hashlib, os, sys
from pathlib import Path
out, card = Path(sys.argv[1]), Path(sys.argv[2])
new = out / "PSP/GAME/SHARVIEW"
def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()
rows, repl, same, newf = [], 0, 0, 0
for p in sorted(x for x in new.rglob("*") if x.is_file()):
    rel = p.relative_to(new).as_posix(); s = sha(p); c = card / rel
    if c.is_file():
        cs = sha(c)
        if cs == s: same += 1; st = "совпадает с картой"
        else: repl += 1; st = f"ЗАМЕНИТЬ (на карте {cs[:12]})"
    else: newf += 1; st = "НОВЫЙ (на карте нет)" if card.is_dir() else "новый (карта не указана)"
    rows.append((rel, p.stat().st_size, s, st))
keep = []
if card.is_dir():
    names = {r[0] for r in rows}
    for c in sorted(x for x in card.rglob("*") if x.is_file()):
        rel = c.relative_to(card).as_posix()
        if rel not in names: keep.append(rel)
with open(out / "ЗАМЕНА.txt", "w") as f:
    f.write(f"НАБОР ВЫПУСКА SHARVIEW · против карты {card} (только чтение)\nЗАМЕНИТЬ: {repl} · НОВЫХ: {newf} · совпадает: {same} · файлов набора: {len(rows)}\n")
    f.write("Не трогать на карте (сохранения и прочее вне набора): save00.bin, save03.bin, settings.bin, devsave, pcar-*.shr; всего на карте файлов вне набора: %d\n\n" % len(keep))
    for r in rows: f.write(f"{r[3]:44} {r[1]:>11}  {r[2][:16]}  {r[0]}\n")
    if keep:
        f.write("\nНА КАРТЕ, НЕТ В НАБОРЕ (не удалять без слова дирижёра):\n")
        for k in keep: f.write(f"  {k}\n")
print(f"ЗАМЕНА.txt: заменить {repl}, новых {newf}, совпадает {same}, вне набора на карте {len(keep)}")
PY
echo "готово: $OUT/PSP/GAME/SHARVIEW (EBOOT $(shasum -a 256 "$OUT/PSP/GAME/SHARVIEW/EBOOT.PBP" | cut -c1-8)); $OUT/ЗАМЕНА.txt; карту не трогал"
