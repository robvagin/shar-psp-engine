#!/bin/bash
# №1037 приёмка: перенос текстов заданий L1–L7: наименьшее число строк прежде красоты. accept60.sh [red]
#   Мера (host.c = uiWrap из viewer/ui_layout.h, ширины букв pix8): по ширинам 209/181/150/120/108 «стало» нигде не длиннее «было», не меньше 10 текстов стало короче, висячих слов не больше, вылезаний за ширину не больше;
#   «TALK TO THE SEA CAPTAIN» при ширине 181: было 3 строки, стало 2. «red» — «стало» считается прежним переносом -> FAIL. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); cd "$D"; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
cc -O1 -w -o host host.c || { echo FAIL; exit 1; }
[ "${1:-}" = red ] && export RED=1
python3 -I table.py | tail -9
