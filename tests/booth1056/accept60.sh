#!/bin/bash
# №1056 приёмка (Роб 27.10, TEST73 на PSP: на L3 выбрал в будке машину Барта с L6, приехала машина Лизы): на каждом уровне L1–L7 любая машина списка будки приезжает та, что выбрана.
# Причина: defaultcar более позднего уровня (bart_v L6, homer_v L7, marge_v L4, apu_v L5 …) будка показывает и открывает по сохранению, а модели в пакете msncars-lN текущего уровня не было
#   (tools/msncar_export.py брал defaultcar только до текущего уровня); без модели движок молча зовёт свою машину (PHONECAR nomodel, viewer/main_frontend.h phoneSummonCar).
# Правка: defaultcar всех семи уровней — в пакет каждого уровня. Красный — SHR_PHONE_DEFCARS_OLD=1 (пакеты побайтно как в TEST73).
# Часть 1 (хост): пакеты msncars-lN нового и прежнего экспорта; в прежнем не хватает чужих defaultcar (красный), в новом — только своя машина уровня; прежний == набор TEST73 по байтам.
# Часть 2 (эмулятор, аргумент 1 — EBOOT тура): L1–L7, по сценарию gen.py: будка открывается, все 42 машины вызываются как из будки (mvPhoneCar), будка открывается снова; check.py.
#   Новые пакеты — все зелёные; прежние (L1–L5) — красные (машины не приезжают).
# Запуск: tests/booth1056/accept60.sh [EBOOT]  (REUSE=1 — не переэкспортировать пакеты). Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test73tour/PSP/GAME/SHARTEST15}; E=${1:-$SET/EBOOT.PBP}
OUT=${OUT:-$W/build/b1056/acc}; mkdir -p $OUT; rc=0; export EMU_OWNER=${EMU_OWNER:-Управление} EMU_MINE=${EMU_MINE:-4}
[ -f "$E" ] || { echo "нет EBOOT $E"; echo FAIL; exit 1; }
pk() { [ "$2" = 1 ] && echo $1/msncars-l1.shr || echo $1/l$2/msncars-l$2.shr; }   # путь пакета уровня в наборе
if [ "${REUSE:-0}" != 1 ] || [ ! -f $OUT/ovl_new/l7/msncars-l7.shr ]; then
  . $W/.venv/bin/activate
  for L in 1 2 3 4 5 6 7; do
    mkdir -p $OUT/ovl_new/l$L $OUT/ovl_old/l$L
    ( python3 $W/tools/msncar_export.py --level $L --out $OUT/ovl_new/l$L/msncars-l$L.shr >$OUT/ovl_new/l$L/exp.log 2>&1
      SHR_PHONE_DEFCARS_OLD=1 python3 $W/tools/msncar_export.py --level $L --out $OUT/ovl_old/l$L/msncars-l$L.shr >$OUT/ovl_old/l$L/exp.log 2>&1 ) &
  done; wait
fi
# OVL: те же относительные пути, что в наборе (L1 — msncars-l1.shr в корне)
for V in new old; do rm -rf $OUT/o_$V; for L in 1 2 3 4 5 6 7; do d=$OUT/o_$V; [ $L = 1 ] || d=$d/l$L; mkdir -p $d; cp $OUT/ovl_$V/l$L/msncars-l$L.shr $d/; done; done
python3 -W ignore -I - $OUT $SET <<'PY' || rc=1
import struct, sys, hashlib
out, st = sys.argv[1], sys.argv[2]
lst = "famil_v cletu_v elect_v plowk_v cDuff cVan honor_v gramp_v moe_v mrplo_v cLimo cFire_v lisa_v skinn_v bookb_v otto_v cDonut cNerd marge_v comic_v krust_v willi_v cKlimo cCurator apu_v frink_v bbman_v carhom_v wiggu_v cCola bart_v snake_v burns_v scorp_v cArmor cSedan homer_v smith_v fone_v zombi_v hbike_v cHears".split()
own = {1: 'famil_v', 2: 'honor_v', 3: 'lisa_v', 4: 'marge_v', 5: 'apu_v', 6: 'bart_v', 7: 'homer_v'}
def names(p):
    d = open(p, 'rb').read(); n, do = struct.unpack_from('<II', d, 12)[0], struct.unpack_from('<I', d, 16)[0]
    return {d[do + i * 176:do + i * 176 + 16].split(b'\0')[0].decode().lower() for i in range(n)}
ok = True
for L in range(1, 8):
    rel = 'msncars-l1.shr' if L == 1 else f'l{L}/msncars-l{L}.shr'
    new = names(f'{out}/ovl_new/l{L}/msncars-l{L}.shr'); old = names(f'{out}/ovl_old/l{L}/msncars-l{L}.shr')
    mn = [c for c in lst if c.lower() not in new]; mo = [c for c in lst if c.lower() not in old]
    same = hashlib.sha1(open(f'{out}/ovl_old/l{L}/msncars-l{L}.shr', 'rb').read()).digest() == hashlib.sha1(open(f'{st}/{rel}', 'rb').read()).digest()
    good = mn == [own[L]] and same
    print(f'  L{L}: пакет новый — нет в пакете {mn}; прежний (красный) — нет {mo}; прежний == набор TEST73: {"да" if same else "НЕТ"}{"" if good else "   <<<"}')
    ok &= good
sys.exit(0 if ok else 1)
PY
if [ -n "$E" ]; then
  for L in 1 2 3 4 5 6 7; do python3 $H/gen.py $L > $OUT/m$L.txt; done
  run() { OVL=$OUT/o_$1 OUT=$OUT bash $W/tools/emu_run.sh $OUT/m$2.txt $1$2 "$E" 400 >/dev/null 2>&1; }
  for L in 1 2 3 4 5 6 7; do run new $L & done; wait
  for L in 1 2 3 4 5; do run old $L & done; wait
  echo "  зелёный (новые пакеты):"; for L in 1 2 3 4 5 6 7; do python3 -W ignore -I $H/check.py $OUT/out_new$L/smoke.txt $L || { rc=1; echo "    ^ L$L красный на новых пакетах"; }; done
  echo "  красный контроль (пакеты TEST73, L1–L5 обязаны падать):"
  for L in 1 2 3 4 5; do if python3 -W ignore -I $H/check.py $OUT/out_old$L/smoke.txt $L; then echo "    L$L КРАСНЫЙ НЕ КРАСНЫЙ"; rc=1; fi; done
fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
