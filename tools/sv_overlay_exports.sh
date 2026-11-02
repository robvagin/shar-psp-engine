#!/bin/sh
# окно g12-data, круг 4: пары экспортов OLD/NEW каждого уровня для tools/sv_overlay.py (рецепт TEST81: reissue_level.sh + флаги качества build_level.sh).
# OLD — все мои выключатели _OLD=1, NEW — по умолчанию. Результат: $X/oldN.shr, $X/newN.shr (временные, в overlay уходят только отличия)
cd "$(dirname "$0")/.."
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
X=${X:-/private/tmp/claude-501/-Users-USER-Downloads-shar-psp/d09a937d-c6b5-4716-8b24-9b181c0e2b1e/scratchpad/g12b/ex}; mkdir -p $X; . .venv/bin/activate
RECIPE="SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 SHR_LAYERS=1 SHR_T4=1 SHR_SIGN_CAP=1 SHR_PROP_OWNED_OLD=1 SHR_FENCE_ROOF_OLD=1"
OLDSW="SHR_SURF_OLD=1 SHR_CARCMD_OLD=1 ATC0_OLD=1 PRP2_COINS25_OLD=1 PRP2_PARTICLES_OLD=1 AO_FOLLOW_OLD=1"
job() { env $RECIPE $2 python tools/psp_export.py ${1%:*} --out $X/${1#*:}.shr > $X/${1#*:}.log 2>&1 && echo "${1#*:} OK" || echo "${1#*:} FAIL"; }
LIST=$(for L in 1 2 3 4 5 6 7; do echo "$L:new$L"; [ -n "$WITH_OLD" ] && echo "$L:old$L"; done)
for j in $LIST; do
  case $j in *old*) sw="$OLDSW";; *) sw="";; esac
  while [ $(jobs -r | wc -l) -ge 3 ]; do sleep 5; done
  ( job $j "$sw" ) &
done
wait; echo DONE
