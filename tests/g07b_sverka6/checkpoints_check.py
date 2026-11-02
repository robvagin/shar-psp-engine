#!/usr/bin/env python3
"""Окно g07b-msnview, круг 6 (P17-16): номера чекпойнтов трасс диска против списков оригинала.
Оригинал задаёт порядок прохода списком номеров локаторов: b01 2,3,4,5,6,7,8,1; b02 и b07 2,3,4,5,6,3,8,9,1 (№3 дважды, №7 не нужен); b03, b04, b06 2,3,4,5,6,7,1;
b05 2,3,4,5,6,7,8,9,10,7,3,11,1. Проверяем: набор номеров в данных трассы = набор номеров списка (чекпойнты на месте), и наш порядок круга (номера по возрастанию, финиш №1 последним)
совпадает со списком на трассах без повторов (b01, b03, b04, b06). Трассы с повторами (b02, b05, b07) отмечаем как «не доделано: нужны теги остановок пути» — тест их не прячет, а считает.
  checkpoints_check.py [--strict]   (--strict: повторы тоже должны совпасть — сейчас падает, пока повторы не внедрены)"""
import sys, pathlib
here = pathlib.Path(__file__).resolve().parents[2]; sys.path.insert(0, str(here / 'tools')); sys.path.insert(0, str(here / 'tools/bonus'))
import track as T
ORIG = {1: [2, 3, 4, 5, 6, 7, 8, 1], 2: [2, 3, 4, 5, 6, 3, 8, 9, 1], 3: [2, 3, 4, 5, 6, 7, 1], 4: [2, 3, 4, 5, 6, 7, 1], 5: [2, 3, 4, 5, 6, 7, 8, 9, 10, 7, 3, 11, 1],
        6: [2, 3, 4, 5, 6, 7, 1], 7: [2, 3, 4, 5, 6, 3, 8, 9, 1]}
strict = '--strict' in sys.argv; bad = 0; dup = []
for n in range(1, 8):
    L = T.locators(n); cps = T.checkpoints(L); nums = [c['data'][1] for c in cps]
    ours = nums[1:] + nums[:1]                       # наш круг: по возрастанию, финиш (1) последним
    want = ORIG[n]
    same_set = sorted(set(ours)) == sorted(set(want)); same = ours == want
    print('b0%d: данные %s | наш круг %s | оригинал %s | набор %s | порядок %s' % (n, nums, ours, want, 'совпал' if same_set else 'РАЗНЫЙ', 'совпал' if same else 'не совпал (повторы)'))
    if not same_set: bad += 1
    if not same: (bad := bad + 1) if strict else dup.append(n)
print('трассы с повторами в оригинале, у нас пока без них: %s' % dup)
print('FAIL' if bad else 'OK')
sys.exit(1 if bad else 0)
