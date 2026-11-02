#!/usr/bin/env python3
"""Перевод записей сохранения из прежнего формата ("SHSV", 256/768 Б) в SHV3 (1536 Б), правка №1114.
Использует тот же savegame.c, что игра (tools/savegen/svconv.c собирается тут же, кэш в build/savegen/), поэтому перевод бит в бит
совпадает с тем, что игра делает при загрузке.

  tools/save_v3_convert.py ВХОД [ВЫХОД]      один файл (save00.bin или DATA.BIN); без ВЫХОД — на месте, прежний файл рядом как ВХОД.shsv
  tools/save_v3_convert.py --dir ПАПКА       все save??.bin и DATA.BIN в папке (рекурсивно): сейф build/saves/*/SHARVIEW, карта PSP
  --check                                    только проверить и напечатать, ничего не писать
  --keep-old                                 не копировать прежний файл (по умолчанию копия ВХОД.shsv лежит рядом)
Код выхода: 0 — всё переведено или уже SHV3; 2 — какой-то файл не читается (зашифрован эмулятором или испорчен).
Записи эмулятора (~/.ppsspp/PSP/SAVEDATA/*/DATA.BIN) PPSSPP шифрует ключом игры: их тут не прочесть, но и не нужно — игра сама
переводит прежнюю запись при чтении (в журнале строка SVCONV); gate_env/frame_gate эту папку не трогают."""
import argparse, os, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "build", "savegen")

def build():
    os.makedirs(OUT, exist_ok=True)
    exe = os.path.join(OUT, "svconv")
    src = [os.path.join(ROOT, "tools", "savegen", "svconv.c"), os.path.join(ROOT, "viewer", "savegame.c")]
    if os.path.exists(exe) and os.path.getmtime(exe) >= max(os.path.getmtime(p) for p in src + [os.path.join(ROOT, "viewer", "savegame.h")]):
        return exe
    sys.path.insert(0, os.path.join(ROOT, "tools", "bots"))
    lib = subprocess.run(["bash", "-c", f"V={ROOT}/viewer; . '{ROOT}/tools/bots/lib.sh'; echo $BOTS_LIB"], capture_output=True, text=True, cwd=ROOT).stdout.split()
    lib = [x if os.path.isabs(x) else os.path.join(ROOT, x) for x in lib]
    cmd = ["cc", "-std=c11", "-O2", "-Wall", "-Wno-unused-function", "-Wno-misleading-indentation", "-DSKIN_HOST", "-I", os.path.join(ROOT, "viewer"), "-I", os.path.join(ROOT, "tools", "bots"),
           src[0]] + lib + [src[1], "-lm", "-o", exe]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode:
        sys.exit("сборка svconv не прошла:\n" + r.stderr[:2000])
    return exe

def one(exe, src, dst, check, keep):
    tmp = (dst or src) + ".v3tmp"
    r = subprocess.run([exe, src, tmp], capture_output=True, text=True)
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    if r.returncode == 2:
        return 2
    if r.returncode == 3 or check:
        if os.path.exists(tmp): os.remove(tmp)
        return 0
    if dst is None:
        if not keep: shutil.copy2(src, src + ".shsv")
        os.replace(tmp, src)
    else:
        os.replace(tmp, dst)
    return 0

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inp"); ap.add_argument("out", nargs="?"); ap.add_argument("--dir", action="store_true")
    ap.add_argument("--check", action="store_true"); ap.add_argument("--keep-old", action="store_true")
    a = ap.parse_args()
    exe = build(); bad = 0
    if a.dir:
        for d, _, fs in os.walk(a.inp):
            for f in sorted(fs):
                if (f.startswith("save") and f.endswith(".bin")) or f == "DATA.BIN":
                    bad += one(exe, os.path.join(d, f), None, a.check, a.keep_old) == 2
    else:
        bad += one(exe, a.inp, a.out, a.check, a.keep_old) == 2
    sys.exit(2 if bad else 0)

if __name__ == "__main__":
    main()
