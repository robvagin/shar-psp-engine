"""№1133: в пакете будки (tools/shop_export.py phone_pack) картинка Spotlight (голубое пятно pbglow) идёт со своим тегом WASPS (TAG_SPOT), а не ALWAYS:
по этому тегу движок (viewer/phone_view.c PV_T_SPOT) пятно прячет; остальное нетронуто (машина CARDS, замок GAMBLE). Данные не менялись (тег стоял с №557) —
проверка сторожит договорённость с движком. PHONE_HUSK_OLD=1 в окружении — пятно без тега (ALWAYS), красный контроль: проверка обязана упасть.
Запуск из корня репозитория: python tests/g07b_sverka8/glow_check.py  (нужны диск и .venv: PY=~/Downloads/shar-psp/.venv/bin/python)"""
import sys, tempfile, pathlib
sys.path.insert(0, 'tools'); sys.path.insert(0, 'hit-and-run-web/tools')
import shop_export as SE
cap = {}; orig = SE.booth_layout
def bl(dl, kind, assets):
    out = orig(dl, kind, assets); cap['dl'] = out; return out
SE.booth_layout = bl
SE.phone_pack(SE.phone_cars(1), 1, pathlib.Path(tempfile.mkdtemp()) / 'phone.shr')
tags = {t['name']: t.get('tag') for t in cap['dl']}
bad = []
if tags.get('Spotlight') != 'WASPS': bad.append(f"Spotlight с тегом {tags.get('Spotlight')}, ждали WASPS (пятно за машиной должно быть скрыто)")
if tags.get('PreviewImage') != 'CARDS': bad.append(f"картинка машины с тегом {tags.get('PreviewImage')}, ждали CARDS")
if tags.get('Overlay') != 'GAMBLE' or tags.get('Lock') != 'GAMBLE': bad.append('замок закрытой машины тронут')
for b in bad: print('FAIL:', b)
print('RESULT', 'FAIL' if bad else 'PASS', 'Spotlight =', tags.get('Spotlight'))
sys.exit(1 if bad else 0)
