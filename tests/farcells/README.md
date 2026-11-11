# Стенд картинки «дальние клетки» (tools/farcells_gate.py)

Одна команда на уровень, итог PASS/FAIL + лист `build/farcells/L<N>/sheet.png` (полные | дальние | разница: красное — плохие пиксели, синее — маска шума) и `summary.txt`.

```
.venv/bin/python tools/farcells_gate.py N --full SET --points ~/Downloads/shar-psp-t93/docs/state/viewpoints_93.json \
    --make-far ~/Downloads/shar-psp-mod96/build/farcells/m50/lN/farcells.shr          # до переключения в игре: farcells_apply на глаз КАЖДОЙ точки
.venv/bin/python tools/farcells_gate.py N --full SET_с_farcells.shr --points … --far-scn "flag farcell 1"   # когда переключение есть: один прогон «дальние»
  --only L4_p01,L4_p05     часть точек        --out DIR   каталог результата (по умолчанию build/farcells/LN)
  --views FILE             свои виды «имя x z h [camheight camdist]» вместо точек Данных
  --approach "x0 z0 x1 z1 h шагов" | none    подъезд кадр за кадром (только когда переключение в игре; с фиксированным глазом смысла нет)
  --pct 0.3 --mean 0.6 --jump 0.15            пороги (плохих пикселей %, средняя ошибка из 255, скачок между шагами подъезда, п.п.)
  --self-red               красный контроль: «дальние» сдвинуты на 6 px — обязано дать FAIL
```
SET — папка как для tools/smoke.sh (EBOOT.PBP и данные; для L2–L7 и l<N>/level.shr). Прогоны: полные №1, полные №2, дальние (по точке); маска шума = пиксели, различающиеся между двумя одинаковыми прогонами (расширена на 3 px).
Известное: движение трафика в прогоне «дальние» может разойтись с «полными» (другой темп загрузки клеток) и попадёт в «плохие»; на листе это компактные пятна на дороге, не трещины.
Точки Данных-93: свободная камера (`goto x z 0`, пауза 5 с, `cam …`, кадр).
