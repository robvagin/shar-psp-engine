#!/usr/bin/env python3
"""Build 95 (аудит Fable §7 п.4, спецификация Г.1 П4): «долгая игра + естественный конец уровня» для эмулятора. gen_long.py [LONG_С=900] [FRAG=0.6] [no|yes] [short] > сценарий.txt
Уровень 1, куча раздроблена (flag fragblk FRAG, кусок <= FRAG МБ, как у Роба на L4). Фаза А (LONG секунд игры): автопилот flag drive по дорогам трафика кругами по 150 с, между ними заходы:
дом, Kwik-E-Mart, школа (дверь внутрь и обратно), будка у дома, пауза с MISSION SELECT, одна смерть (двор горящих покрышек). Фаза Б: конец уровня как у игрока (путь Будки-93 gen_end.py: msnfinal,
этапы засчитаны, Level Complete, ✕, вопрос «Do you want to save your game?», ответ NO кнопкой, газета, уровень 2) и 40 с игры на L2. Ответ: no (по умолчанию) | yes.
short — одна пара заходов вместо 15 минут (проверка сценария за 6-8 минут)."""
import sys
a = [x for x in sys.argv[1:]]
nums = [float(x) for x in a if x.replace('.', '', 1).isdigit()]
LONG = nums[0] if nums else 900.0; FRAG = nums[1] if len(nums) > 1 else 0.6
YES = 'yes' in a; SHORT = 'short' in a
LV = next((int(x[1:]) for x in a if len(x) == 2 and x[0] == 'L' and x[1].isdigit()), 1)
FMV = {2: 'fmv3', 5: 'fmv5', 6: 'fmv6'}.get(LV)   # ролик конца уровня (SetFMVInfo в m8l/m8sdi): должен сыграться до Level Complete
BOOTH_AT = {1: '144.0 81.8', 2: '11.4 85.7', 5: '11.4 85.7', 6: '194.2 89.5', 3: '194.2 89.5', 4: '144.0 81.8', 7: '144.0 81.8'}[LV]
if SHORT: LONG = 150.0
o = ["flag saves", "flag cfgdefaults 1", "scene LG_BOOT", "wait 4", "scene LG_L1", "flag golevel %d" % LV, "wait %d" % (10 if LV == 1 else 45), "flag player", "flag msn", "wait 3"]
o += ["wait 1"] if FRAG <= 0 else ["flag fragblk %g" % FRAG, "wait 1"]

def door(name, enter, exit_in, exit_out):
    return [f"scene LG_{name}", f"foot {enter}", "wait 2", "press up 0.5", "wait 0.4", "press triangle 0.2", "wait 8", "shot in_" + name,
            f"foot {exit_in}", "wait 1", f"foot {exit_out}", "wait 0.5", "press triangle 0.2", "wait 6", "shot out_" + name]

HOUSE = door("house", "228.90 181.60 0.0 3.8", "501.13 392.0 0 -19.5", "502.73 398.0 0 -19.5")
KWIK = door("kwik", "213.91 299.04 0.0 5.56", "499.43 300.0 0 -19.5", "499.43 305.25 0 -19.5")
SCHOOL = door("school", "-35.18 586.0 0.0 4.64", "500.01 336.5 0 -19.5", "500.01 341.7 0 -19.5")
BOOTH = ["scene LG_booth", "foot %g %g 0" % (float(BOOTH_AT.split()[0]) + 0.5, float(BOOTH_AT.split()[1]) + 1.2), "wait 3", "press triangle 0.2", "wait 12", "shot booth_open", "press cross 0.2", "wait 4", "shot booth_after"]
PAUSE = ["scene LG_pause", "press start", "wait 1", "press down", "wait 0.4", "press cross", "wait 4", "shot msel", "press triangle", "wait 1", "press start 0.3", "wait 2"]
DEATH = ["scene LG_death", "foot 266 -515 1.5708", "wait 6", "shot death"]
EVENTS = [HOUSE, KWIK, BOOTH, SCHOOL, PAUSE, DEATH] if LV == 1 else ([HOUSE, KWIK, BOOTH, PAUSE, DEATH, DEATH] if LV == 4 else [PAUSE, BOOTH])   # комнаты и смерть расписаны только для L1 (на L2+ в данных комнат нет, кроме L4/L7)
t = 0.0; k = 0
while t < LONG:
    seg = min(150.0, LONG - t); o += ["scene LG_drive%d" % k, "flag drive %d" % int(seg), "wait %d" % (int(seg) + 4)]
    t += seg + 4
    ev = EVENTS[k % len(EVENTS)] if not SHORT else EVENTS[k % 2]
    o += ev; t += 25 if ev is not DEATH else 8
    if SHORT and k == 1: break
    k += 1
# L4: ролик fmv4 в середине уровня (этап m5, SetFMVInfo в m5i), конца-ролика у L4 в данных нет (SetLevelOver только у L2, L5, L6)
if LV == 4: o += ["scene LG_m5", "flag stop", "wait 3", "flag msnsel 4", "wait 8"] + ["flag msnpass", "wait 2"] * 24 + ["waitfor MSNFMV name=fmv4 200", "wait 10"]
# фаза Б: естественный конец уровня как у игрока
if FMV:   # уровни с роликом конца (L2 fmv3, L5 fmv5, L6 fmv6): последняя миссия m8 по служебному выбору (как MISSION SELECT, гейт fmv_levelend): ролик -> таймер -> SetLevelOver -> Level Complete -> вопрос
    o += ["scene LG_end", "flag stop", "flag player", "flag msn", "wait 4", "flag realend 2", "flag msnsel 7", "waitfor MSNFMV name=%s 200" % FMV, "wait 6", "shot lc", "press cross 0.3", "wait 4", "shot ask"]
else:     # L1 (путь Будки-93 gen_end.py): msnfinal, этапы засчитаны
    o += ["scene LG_end", "flag stop", "flag player", "flag msn", "wait 4", "flag realend 2", "flag msnfinal", "wait 3", "shot card", "press cross 0.3", "wait 3", "press cross 0.3", "wait 3"]
    for i in range(14): o += ["flag msnpass", "wait 2"]
    o += ["wait 6", "shot lc", "press cross 0.3", "wait 4", "shot ask"]
if YES: o += ["press cross 0.3", "wait 3", "press cross 0.3", "wait 6", "press cross 0.3", "wait 6", "shot saved"]
else: o += ["press down 0.3", "wait 1", "press cross 0.3"]
o += ["waitfor LEVEL ev=load n=%d 150" % (LV + 1), "wait 45", "shot next_after", "end"]
print("\n".join(o))
