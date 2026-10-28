#!/bin/bash
# lvx.sh ИМЯ N ФЛАГИ...: только экспорт level.shr (без PVS/wind/порядка секций) — для сверки и подмены секции COL1; ФЛАГИ — переменные окружения VAR=1
cd ~/Downloads/shar-psp-t75-lv
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
NAME=$1; N=$2; shift 2; for kv in "$@"; do export "$kv"; done
export SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 SHR_LAYERS=1 SHR_T4=1 SHR_SIGN_CAP=1 SHR_PROP_OWNED_OLD="${SHR_PROP_OWNED_OLD-1}" SHR_FAR_PLAIN=1
O=build/t75lv/x/$NAME; mkdir -p $O; . .venv/bin/activate
date +%T > $O/start.txt
python tools/psp_export.py $N --out $PWD/$O/level.shr > $O/exp.log 2>&1; echo "rc=$?" >> $O/exp.log; date +%T > $O/end.txt
