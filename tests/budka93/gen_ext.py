#!/usr/bin/env python3
"""Будка-93 / Build 96: принудительный отказ памяти на газетных классах bonus | msnsel | menuload (канон п.14: у каждого перехода свой сценарий с красным контролем).
gen_ext.py КЛАСС [old] > сценарий.txt. old = flag tfsm95old 1 (прежнее поведение: класс машине не виден, отказ не принудителен -> красное)."""
import sys
c = sys.argv[1]; OLD = 'old' in sys.argv[2:]
pre = ["flag tfsm95old 1"] if OLD else []
if c in ('bonus', 'menuload'):   # menuload: выход из бонус-игры идёт через feLoadShown; при отказе BONUS и отказе меню оба пути видны
    fl = ["flag tfbonusfail 1"] + (["flag tfmenufail 1"] if c == 'menuload' else [])
    o = ["flag menu", "flag bonusopen"] + pre + ["flag bonuslaps 1", "flag bonusauto 1", "flag bonustrack 1", "wait 2", "press left", "wait 0.6", "press cross", "wait 1.5"] + fl + ["flag bonusgo", "wait 10", "shot bonus_after", "end"]
elif c == 'msnsel':
    o = ["flag menu", "flag lvset 5", "wait 1.5", "press cross", "wait 70", "flag uimsndone 31", "wait 1"] + pre + ["flag tfmsnfail 1",
         "scene MSEL", "press start", "wait 1", "press down", "wait 0.4", "press cross", "wait 1"] + ["press down", "wait 0.3"] * 4 + ["shot ms_list", "wait 6", "shot after", "end"]
else:
    sys.exit("класс: bonus | msnsel | menuload")
print("\n".join(o))
